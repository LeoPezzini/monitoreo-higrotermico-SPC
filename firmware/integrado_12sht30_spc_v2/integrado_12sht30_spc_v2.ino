#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"

// PRUEBA LOCAL: ESP32 -> WiFi -> receptor SPC v2 (Docker, puerto HOST 8081)
// 12 SHT30, CRC8, RTC DS3231, CSV + cola persistente SD, CRC16-CCITT-FALSE,
// HTTP 204 y reintentos. El receptor v2 guarda en SQLite, NO en InfluxDB.
// Completar IP en secrets.h: #define SPC_RECEPTOR_URL "http://IP_NOTEBOOK:8081/api/v2/write"

// Poner true SOLO para una carga de ajuste; despues volver a false y recargar.
constexpr bool AJUSTAR_RTC_UNA_VEZ = false;
constexpr uint8_t SDA_SENSORES = 21, SCL_SENSORES = 22;
constexpr uint8_t SDA_RTC = 17, SCL_RTC = 16; // Cableado real corregido en preperforada
constexpr uint8_t SD_SCK = 14, SD_MISO = 27, SD_MOSI = 13, SD_CS = 26;
constexpr uint8_t MUX_A = 0x70, MUX_B = 0x71, SHT_ADDR = 0x44;
constexpr uint32_t INTERVALO_MS = 30000, REINTENTO_MS = 10000;
constexpr uint8_t MAX_ENVIOS_PASADA = 24;
// El RTC previamente se configuró en hora local de Argentina (UTC-3).
constexpr uint32_t SEGUNDOS_A_UTC = 3UL * 3600UL;
const char *ARCHIVO_CSV = "/spc12.csv";
const char *ARCHIVO_COLA = "/spc12q.txt";
const char *ARCHIVO_ACK = "/spc12ack.txt";

struct Sensor { uint8_t id, mux, canal; };
const Sensor sensores[] = {
  {1,0x70,2},{2,0x70,3},{3,0x70,4},{4,0x70,5},{5,0x70,6},
  {7,0x71,0},{8,0x71,1},{9,0x71,2},{10,0x71,3},
  {11,0x71,4},{14,0x71,5},{15,0x71,6}
};
constexpr size_t N_SENSORES = sizeof(sensores)/sizeof(sensores[0]);
TwoWire I2C_RTC(1);
RTC_DS3231 rtc;
SPIClass spiSD(VSPI);
bool rtcOK = false, sdOK = false;
uint32_t totalEnCola = 0, confirmados = 0, ciclo = 0;
uint32_t ultimoCiclo = 0, ultimoReintento = 0;

uint8_t crc8(const uint8_t *p, uint8_t n) {
  uint8_t crc = 0xFF;
  while (n--) {
    crc ^= *p++;
    for (uint8_t b=0;b<8;b++) crc = (crc & 0x80) ? uint8_t((crc<<1)^0x31) : uint8_t(crc<<1);
  }
  return crc;
}
uint16_t crc16(const String &id, const String &payload) {
  uint16_t crc = 0xFFFF;
  auto feed = [&](uint8_t c) {
    crc ^= uint16_t(c) << 8;
    for (uint8_t b=0;b<8;b++) crc = (crc & 0x8000) ? uint16_t((crc<<1)^0x1021) : uint16_t(crc<<1);
  };
  for (size_t i=0;i<id.length();i++) feed(uint8_t(id[i]));
  feed('\n');
  for (size_t i=0;i<payload.length();i++) feed(uint8_t(payload[i]));
  return crc;
}
uint8_t escribirMux(uint8_t addr, uint8_t value) {
  Wire.beginTransmission(addr); Wire.write(value); return Wire.endTransmission();
}
bool seleccionar(const Sensor &s) {
  uint8_t e = escribirMux(MUX_A,0);
  if (e) { Serial.printf("  ERROR deshabilitar 0x70: %u\n",e); return false; }
  e = escribirMux(MUX_B,0);
  if (e) { Serial.printf("  ERROR deshabilitar 0x71: %u\n",e); return false; }
  delay(3);
  e = escribirMux(s.mux, uint8_t(1U << s.canal));
  if (e) { Serial.printf("  ERROR seleccionar 0x%02X CH%u: %u\n",s.mux,s.canal,e); return false; }
  delay(3); return true;
}
bool leerSHT(float &t, float &h) {
  Wire.beginTransmission(SHT_ADDR); Wire.write(0x24); Wire.write(0x00);
  uint8_t e=Wire.endTransmission();
  if(e) { Serial.printf("  ERROR comando SHT: %u\n",e); return false; }
  delay(20);
  if(Wire.requestFrom(uint8_t(SHT_ADDR),uint8_t(6))!=6) {
    while(Wire.available()) Wire.read();
    Serial.println("  ERROR lectura SHT incompleta"); return false;
  }
  uint8_t d[6]; for(uint8_t i=0;i<6;i++) d[i]=Wire.read();
  if(crc8(d,2)!=d[2] || crc8(d+3,2)!=d[5]) {
    Serial.println("  ERROR CRC8 SHT"); return false;
  }
  uint16_t rt=(uint16_t(d[0])<<8)|d[1], rh=(uint16_t(d[3])<<8)|d[4];
  t=-45.0f+175.0f*rt/65535.0f; h=100.0f*rh/65535.0f;
  return true;
}
String fechaLocal(const DateTime &d) {
  char b[24]; snprintf(b,sizeof(b),"%04u-%02u-%02uT%02u:%02u:%02u",
    d.year(),d.month(),d.day(),d.hour(),d.minute(),d.second());
  return String(b);
}
bool fechaValida(const DateTime &d) {
  return d.isValid() && d.year()>=2025 && d.year()<=2035;
}
bool appendLinea(const char *path, const String &s) {
  File f=SD.open(path,FILE_APPEND);
  if(!f) return false;
  size_t n=f.print(s); n+=f.print('\n'); f.close();
  return n==s.length()+1;
}
// Cola: una linea por medicion: ID<TAB>line protocol.
// No se escribe ID separado de su payload; la cola es la fuente de verdad.
// Si un registro final se interrumpe, no se confirma ni transmite.
bool parsearRegistro(const String &linea, String &id, String &lp) {
  int tab=linea.indexOf('\t');
  if(tab<1) return false;
  id=linea.substring(0,tab); lp=linea.substring(tab+1);
  return id.length()>0 && lp.length()>0 && lp.indexOf('\t')<0 && lp.indexOf('\r')<0;
}
// Validacion de ACK append-only: indice,~indice
uint32_t recuperarACK() {
  File f=SD.open(ARCHIVO_ACK,FILE_READ); if(!f) return 0;
  uint32_t ultimo=0;
  while(f.available()) {
    String l=f.readStringUntil('\n'); l.trim();
    int c=l.indexOf(','); if(c<1) continue;
    String a=l.substring(0,c),b=l.substring(c+1);
    bool valido=a.length() && b.length();
    for(size_t i=0;i<a.length();i++) if(!isDigit(a[i])) valido=false;
    for(size_t i=0;i<b.length();i++) if(!isDigit(b[i])) valido=false;
    if(!valido) continue;
    uint32_t n=strtoul(a.c_str(),nullptr,10), inv=strtoul(b.c_str(),nullptr,10);
    if(inv==~n && n>=ultimo) ultimo=n;
  }
  f.close(); return ultimo;
}
bool guardarACK(uint32_t n) {
  return appendLinea(ARCHIVO_ACK,String(n)+","+String(~n));
}
// Verifica integridad de las lineas al arranque; no sobrescribir cola previa.
// Un final incompleto bloquea la adquisicion hasta repararlo manualmente.
bool inspeccionarCola() {
  if(!SD.exists(ARCHIVO_COLA)) return true;
  File f=SD.open(ARCHIVO_COLA,FILE_READ); if(!f) return false;
  uint32_t n=0; bool ok=true;
  if(f.size()>0) {
    f.seek(f.size()-1);
    if(f.read()!='\n') ok=false;
    f.seek(0);
  }
  while(ok && f.available()) {
    String l=f.readStringUntil('\n');
    String id,lp;
    if(!parsearRegistro(l,id,lp)) { ok=false; break; }
    n++;
  }
  f.close(); totalEnCola=n;
  if(confirmados>totalEnCola) ok=false;
  if(!ok) Serial.println("ERROR: cola SD inconsistente; no se modifica");
  return ok;
}
bool enviarHTTP(const String &id, const String &lp, int &status) {
  status=-1;
  if(WiFi.status()!=WL_CONNECTED) { WiFi.reconnect(); return false; }
  HTTPClient http;
  if(!http.begin(SPC_RECEPTOR_URL)) return false;
  char crcTexto[5]; snprintf(crcTexto,sizeof(crcTexto),"%04X",crc16(id,lp));
  http.setTimeout(5000);
  http.addHeader("Content-Type","text/plain; charset=utf-8");
  http.addHeader("X-SPC-ID",id);
  http.addHeader("X-SPC-CRC16",crcTexto);
  status=http.POST((uint8_t*)lp.c_str(),lp.length());
  http.end();
  return status==204;
}
void reenviarPendientes() {
  if(!sdOK || confirmados>=totalEnCola) return;
  File f=SD.open(ARCHIVO_COLA,FILE_READ);
  if(!f) { Serial.println("ERROR abrir cola para reenvio"); return; }
  uint32_t indice=0; uint8_t enviados=0;
  while(f.available() && enviados<MAX_ENVIOS_PASADA) {
    String linea=f.readStringUntil('\n'); indice++;
    if(indice<=confirmados) continue;
    String id,lp;
    if(!parsearRegistro(linea,id,lp)) {
      Serial.printf("ERROR cola linea %lu\n",(unsigned long)indice); break;
    }
    int codigo=-1; bool ok=enviarHTTP(id,lp,codigo);
    Serial.printf("  %s | HTTP %d | %s\n",id.c_str(),codigo,ok?"CONFIRMADO":"PENDIENTE");
    if(!ok) break;
    if(!guardarACK(indice)) { Serial.println("ERROR guardar ACK en SD"); break; }
    confirmados=indice; enviados++;
  }
  f.close();
  Serial.printf("  Cola: %lu confirmados / %lu registros\n",
    (unsigned long)confirmados,(unsigned long)totalEnCola);
}
void medir() {
  if(!rtcOK || !sdOK) return;
  DateTime d=rtc.now();
  if(!fechaValida(d)) { Serial.println("ERROR fecha RTC; ciclo cancelado"); return; }
  ciclo++;
  String fecha=fechaLocal(d);
  uint32_t utc=d.unixtime()+SEGUNDOS_A_UTC;
  Serial.printf("\n=== CICLO %lu | RTC %s ===\n",(unsigned long)ciclo,fecha.c_str());
  uint8_t ok=0,fallos=0,guardados=0;
  for(const Sensor &s:sensores) {
    Serial.printf("S%02u MUX 0x%02X CH%u: ",s.id,s.mux,s.canal);
    if(!seleccionar(s)) { fallos++; continue; }
    float t,h;
    if(!leerSHT(t,h)) { fallos++; continue; }
    ok++;
    char nombre[5]; snprintf(nombre,sizeof(nombre),"S%02u",s.id);
    Serial.printf("T=%.2f HR=%.2f CRC8 OK\n",t,h);
    String lp="higrotermico,sensor="+String(nombre)+
      " temperature_c="+String(t,2)+",humidity_rh="+String(h,2)+" "+String(utc);
    // Primero respaldo CSV; despues cola. Nunca enviar antes de persistir.
    String csv=fecha+","+String(nombre)+","+String(t,2)+","+String(h,2)+",OK";
    if(!appendLinea(ARCHIVO_CSV,csv)) {
      Serial.println("  ERROR guardar CSV; no se envia"); continue;
    }
    if(totalEnCola==UINT32_MAX) { Serial.println("  ERROR contador cola agotado"); break; }
    uint32_t siguiente=totalEnCola+1;
    char id[40]; snprintf(id,sizeof(id),"SPC01-S%02u-%010lu",s.id,(unsigned long)siguiente);
    if(!appendLinea(ARCHIVO_COLA,String(id)+"\t"+lp)) {
      Serial.println("  ERROR guardar cola; detener para revisar SD");
      sdOK=false; break;
    }
    totalEnCola=siguiente; guardados++;
  }
  escribirMux(MUX_A,0); escribirMux(MUX_B,0);
  Serial.printf("RESUMEN %u/%u sensores OK | %u errores | %u en cola\n",
    ok,(unsigned)N_SENSORES,fallos,guardados);
  reenviarPendientes();
}
void setup() {
  Serial.begin(115200); delay(1000);
  Serial.println("\nSPC 12 SHT30 + DS3231 + SD + HTTP CRC16 -> SQLite v2");
  Wire.begin(SDA_SENSORES,SCL_SENSORES); Wire.setClock(100000); Wire.setTimeOut(100);
  I2C_RTC.begin(SDA_RTC,SCL_RTC,100000); I2C_RTC.setTimeOut(100);
  rtcOK=rtc.begin(&I2C_RTC);
  if(!rtcOK) Serial.println("ERROR DS3231 no detectado");
  else {
    if (AJUSTAR_RTC_UNA_VEZ) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println("RTC AJUSTADO A HORA DE COMPILACION (una vez)");
    }
    DateTime d=rtc.now();
    Serial.printf("RTC %s | lostPower=%s\n",fechaLocal(d).c_str(),rtc.lostPower()?"SI":"NO");
    if(rtc.lostPower() || !fechaValida(d)) {
      Serial.println("ERROR RTC: comprobar fecha/hora, NO se reajusta automaticamente");
      rtcOK=false;
    }
  }
  spiSD.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
  sdOK=SD.begin(SD_CS,spiSD,1000000);
  if(!sdOK) Serial.println("ERROR inicializar microSD");
  else {
    Serial.println("microSD OK");
    if(!SD.exists(ARCHIVO_CSV)) {
      if(!appendLinea(ARCHIVO_CSV,"timestamp_local,sensor,temperatura_C,humedad_RH,estado")) sdOK=false;
    }
    if(sdOK) {
      confirmados=recuperarACK();
      sdOK=inspeccionarCola();
      Serial.printf("Cola SD: %lu registros | ACK %lu\n",
        (unsigned long)totalEnCola,(unsigned long)confirmados);
    }
  }
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  uint32_t inicio=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-inicio<15000) { delay(500); Serial.print('.'); }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED) {
    Serial.print("WiFi OK IP ESP32: "); Serial.println(WiFi.localIP());
  } else Serial.println("WiFi no conectado; se guardaran pendientes");
  if(rtcOK && sdOK) { reenviarPendientes(); medir(); }
  else Serial.println("ADQUISICION BLOQUEADA por RTC/SD");
  ultimoCiclo=millis(); ultimoReintento=millis();
}
void loop() {
  uint32_t ahora=millis();
  if(rtcOK && sdOK && ahora-ultimoCiclo>=INTERVALO_MS) {
    ultimoCiclo=ahora; medir(); ultimoReintento=millis();
  }
  if(sdOK && ahora-ultimoReintento>=REINTENTO_MS) {
    ultimoReintento=ahora; reenviarPendientes();
  }
}