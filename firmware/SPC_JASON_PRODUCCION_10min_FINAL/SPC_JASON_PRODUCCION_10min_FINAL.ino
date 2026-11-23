#include <Arduino.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>
#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

// SPC final: ESP32 -> WiFi -> receptor SPC v2 (puerto publicado configurable)
// 12 SHT30, CRC8, RTC DS3231, CSV + cola persistente SD, CRC16-CCITT-FALSE,
// HTTP 204 y reintentos. El receptor v2 guarda en SQLite, NO en InfluxDB.
// Configurar en secrets.h: #define SPC_RECEPTOR_URL "http://IP_SERVIDOR:PUERTO/api/v2/write"

// Poner true SOLO para una carga de ajuste; despues volver a false y recargar.
constexpr bool AJUSTAR_RTC_UNA_VEZ = false;
constexpr uint8_t SDA_SENSORES = 21, SCL_SENSORES = 22;
constexpr uint8_t SDA_RTC = 17, SCL_RTC = 16; // Cableado real corregido en preperforada
constexpr uint8_t SD_SCK = 14, SD_MISO = 27, SD_MOSI = 13, SD_CS = 26;
constexpr uint8_t MUX_A = 0x70, MUX_B = 0x71, SHT_ADDR = 0x44;
constexpr uint32_t INTERVALO_MS = 600000UL, REINTENTO_MS = 10000UL; // 10 minutos entre ciclos
// Un unico HTTP por pasada: la adquisicion siempre tiene prioridad.
constexpr uint32_t PAUSA_ENTRE_ENVIOS_MS = 100UL;
constexpr uint32_t HTTP_TIMEOUT_MS = 2000UL;
constexpr uint32_t MARGEN_ADQUISICION_MS = 2500UL;
// El RTC previamente se configuró en hora local de Argentina (UTC-3).
constexpr uint32_t SEGUNDOS_A_UTC = 3UL * 3600UL;
const char *ARCHIVO_CSV = "/spc_final.csv";
const char *ARCHIVO_COLA = "/spc_final_cola.txt";
const char *ARCHIVO_ACK = "/spc_final_ack.txt";
const char *ARCHIVO_EVENTOS = "/spc_eventos.csv";
const char *ARCHIVO_EVENTOS_1 = "/spc_eventos_1.csv";
const char *ARCHIVO_EVENTOS_2 = "/spc_eventos_2.csv";
// Log rotativo: 10 MB por archivo, hasta ~30 MB de historial.
constexpr uint32_t LIMITE_EVENTOS_BYTES = 10UL * 1024UL * 1024UL;
bool wifiEstadoAnterior = false;
bool estadoWifiInicializado = false;
bool httpEnFalla = false;
bool sensorFalloPrevio[12] = {};
uint32_t fallosConsecutivos[12] = {};
bool colaTeniaPendientes = false;


struct Sensor { const char *nombre; uint8_t mux, canal; };
const Sensor sensores[] = {
  {"C4I",0x71,6}, {"C0I",0x71,5}, {"Y3I",0x71,4},
  {"Y0I",0x71,3}, {"PcI",0x71,2}, {"P0I",0x71,1},
  {"AmbI",0x71,0},
  {"AmbE",0x70,7}, {"C4E",0x70,6}, {"C0E",0x70,5},
  {"Y3E",0x70,4}, {"Y0E",0x70,3}
};
constexpr size_t N_SENSORES = sizeof(sensores)/sizeof(sensores[0]);
// SSD1306 128x32 I2C: misma linea SDA21/SCL22 que los MUX, direccion usual 0x3C.
// La caracterizacion se conserva como validacion; offsets desactivados hasta
// confirmar correspondencia temporal Testo/CSV y ubicacion final de cada sensor.
constexpr bool APLICAR_OFFSETS_TEMPERATURA = false;
const float offsetTemperatura[N_SENSORES] = {0,0,0,0,0,0,0,0,0,0,0,0};
Adafruit_SSD1306 oled(128,32,&Wire,-1);
bool oledOK=false;
float tempUltima[N_SENSORES]={}, humUltima[N_SENSORES]={};
bool sensorUltimoOK[N_SENSORES]={};
uint8_t sensoresOKUltimoCiclo=0;
uint32_t ultimoOLED=0;
// Pantalla 128x32: 15 s ambiente, 5 s estado. Fallas con prioridad.
// Exterior = AmbE (MUX 0x70 CH7); interior = AmbI (MUX 0x71 CH0).
constexpr uint32_t OLED_AMBIENTE_MS=15000UL;
constexpr uint32_t OLED_ESTADO_MS=5000UL;
constexpr uint32_t OLED_REFRESCO_MS=500UL;
bool primeraMedicionLista=false;
bool errorAlmacenamiento=false;
bool receptorSinRespuesta=false;
int ultimoHTTP=-1;
uint32_t inicioFaseOLED=0;
bool faseEstadoOLED=false;

// Estado compartido de RTC, microSD y cola persistente.
TwoWire I2C_RTC(1);
RTC_DS3231 rtc;
SPIClass spiSD(VSPI);
bool rtcOK = false, sdOK = false;
uint32_t totalEnCola = 0, confirmados = 0, ciclo = 0;
uint32_t ultimoCiclo = 0, ultimoReintento = 0;
// Posicion en bytes del proximo registro no confirmado. Evita recorrer
// toda la cola SD desde el principio en cada transmision.
uint32_t offsetProximoPendiente = 0;
bool offsetColaPreparado = false;
bool intentarEnvioPronto = true;

int indiceSensor(const char *nombre) {
  for(size_t i=0;i<N_SENSORES;i++) if(strcmp(sensores[i].nombre,nombre)==0) return int(i);
  return -1;
}
// La pantalla alterna por tiempo transcurrido, no por el resto de millis().
// El estado del receptor se actualiza despues de cada intento HTTP.
void actualizarOLED(bool forzar=false) {
  if(!oledOK) return;
  const uint32_t ahora=millis();
  if(!forzar && ahora-ultimoOLED<OLED_REFRESCO_MS) return;
  ultimoOLED=ahora;

  const uint32_t duracion=faseEstadoOLED?OLED_ESTADO_MS:OLED_AMBIENTE_MS;
  if(ahora-inicioFaseOLED>=duracion) {
    faseEstadoOLED=!faseEstadoOLED;
    inicioFaseOLED=ahora;
  }

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0);

  const uint32_t pendientes=totalEnCola>=confirmados?totalEnCola-confirmados:0;
  if(!rtcOK) {
    oled.println(F("ERROR RTC DS3231"));
    oled.println(F("Adquisicion detenida"));
  } else if(!sdOK || errorAlmacenamiento) {
    oled.println(F("ERROR MICROSD"));
    oled.println(F("Revisar conexiones"));
    oled.println(F("Datos sin respaldo"));
  } else if(WiFi.status()!=WL_CONNECTED) {
    oled.println(F("ERROR WIFI"));
    oled.println(F("Sin conexion a red"));
    oled.print(F("Pendientes: ")); oled.println(pendientes);
  } else if(receptorSinRespuesta) {
    oled.println(F("ERROR RECEPTOR HTTP"));
    oled.println(F("Sin confirmacion 204"));
    oled.print(F("Pendientes: ")); oled.println(pendientes);
  } else if(primeraMedicionLista && sensoresOKUltimoCiclo<N_SENSORES) {
    oled.println(F("ERROR SENSOR SHT30"));
    oled.print(F("Lecturas: "));
    oled.print(sensoresOKUltimoCiclo);
    oled.print('/'); oled.println(N_SENSORES);
    oled.println(F("Ver monitor serie"));
  } else if(!faseEstadoOLED) {
    // Fuente predeterminada 6 pixeles por caracter. Columnas fijas:
    // etiqueta x=0, temperatura x=36, humedad x=84.
    oled.setCursor(0,0);  oled.print(F("AMBIENTE"));
    oled.setCursor(36,0); oled.print(F("T (C)"));
    oled.setCursor(84,0); oled.print(F("HR (%)"));
    const int ext=indiceSensor("AmbE"), inte=indiceSensor("AmbI");
    oled.setCursor(0,11); oled.print(F("EXT"));
    oled.setCursor(36,11);
    if(ext>=0 && sensorUltimoOK[ext]) {
      oled.print(tempUltima[ext],1);
      oled.setCursor(84,11); oled.print(humUltima[ext],1);
    } else oled.print(F("--"));
    oled.setCursor(0,22); oled.print(F("INT"));
    oled.setCursor(36,22);
    if(inte>=0 && sensorUltimoOK[inte]) {
      oled.print(tempUltima[inte],1);
      oled.setCursor(84,22); oled.print(humUltima[inte],1);
    } else oled.print(F("--"));
  } else {
    oled.println(F("ESTADO DEL SISTEMA"));
    oled.setCursor(0,11);
    oled.print(F("SHT ")); oled.print(sensoresOKUltimoCiclo);
    oled.print(F("/12  SD OK"));
    oled.setCursor(0,22);
    oled.print(F("Pend ")); oled.print(pendientes);
    oled.print(F("  Cic ")); oled.print(ciclo);
  }
  oled.display();
}

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
// Los eventos son informativos; nunca bloquean la adquisicion.
bool crearCabeceraEventos() {
  File f=SD.open(ARCHIVO_EVENTOS, FILE_WRITE);
  if(!f) return false;
  f.println("fecha_hora,evento,componente,detalle");
  f.close();
  return true;
}
void rotarEventosSiHaceFalta() {
  if(!SD.exists(ARCHIVO_EVENTOS)) {
    crearCabeceraEventos();
    return;
  }
  File f=SD.open(ARCHIVO_EVENTOS, FILE_READ);
  if(!f) return;
  const uint32_t tam=(uint32_t)f.size();
  f.close();
  if(tam < LIMITE_EVENTOS_BYTES) return;

  if(SD.exists(ARCHIVO_EVENTOS_2)) SD.remove(ARCHIVO_EVENTOS_2);
  if(SD.exists(ARCHIVO_EVENTOS_1) && !SD.rename(ARCHIVO_EVENTOS_1,ARCHIVO_EVENTOS_2)) {
    Serial.println("WARN: no se pudo rotar spc_eventos_1.csv");
  }
  if(!SD.rename(ARCHIVO_EVENTOS,ARCHIVO_EVENTOS_1)) {
    Serial.println("WARN: no se pudo rotar spc_eventos.csv");
    return;
  }
  if(!crearCabeceraEventos()) Serial.println("WARN: no se pudo crear nuevo log de eventos");
}
void evento(const char *tipo, const String &componente, const String &detalle) {
  if(!sdOK || !rtcOK) return;
  rotarEventosSiHaceFalta();
  File f=SD.open(ARCHIVO_EVENTOS, FILE_APPEND);
  if(!f) { Serial.println("WARN: no se pudo abrir log de eventos"); return; }
  String limpio=detalle;
  limpio.replace(",", ";"); limpio.replace("\n", " "); limpio.replace("\r", " ");
  f.print(fechaLocal(rtc.now())); f.print(','); f.print(tipo);
  f.print(','); f.print(componente); f.print(','); f.println(limpio);
  f.close();
}
void verificarEstadoWiFi() {
  bool conectado=(WiFi.status()==WL_CONNECTED);
  if(!estadoWifiInicializado) {
    estadoWifiInicializado=true;
    wifiEstadoAnterior=conectado;
    evento(conectado?"WIFI_UP":"WIFI_DOWN","WIFI",
           String("Inicio; pendientes=")+String(totalEnCola-confirmados));
  } else if(conectado!=wifiEstadoAnterior) {
    wifiEstadoAnterior=conectado;
    evento(conectado?"WIFI_UP":"WIFI_DOWN","WIFI",
           String("Pendientes=")+String(totalEnCola-confirmados));
  }
}
void verificarSensor(size_t i, bool correcto) {
  if(correcto) {
    if(sensorFalloPrevio[i]) {
      evento("SENSOR_RECUPERADO",sensores[i].nombre,
             String("Fallos consecutivos=")+String(fallosConsecutivos[i]));
    }
    sensorFalloPrevio[i]=false; fallosConsecutivos[i]=0;
  } else {
    fallosConsecutivos[i]++;
    if(!sensorFalloPrevio[i]) {
      evento("SENSOR_ERROR",sensores[i].nombre,
             String("MUX=0x")+String(sensores[i].mux,HEX)+" CH="+String(sensores[i].canal));
    }
    sensorFalloPrevio[i]=true;
  }
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
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type","text/plain; charset=utf-8");
  http.addHeader("X-SPC-ID",id);
  http.addHeader("X-SPC-CRC16",crcTexto);
  status=http.POST((uint8_t*)lp.c_str(),lp.length());
  http.end();
  return status==204;
}
// Reconstruir el desplazamiento de lectura UNA vez al iniciar. El ACK
// sigue siendo la fuente de verdad y no se modifica el formato de archivos.
bool prepararOffsetCola() {
  if(offsetColaPreparado) return true;
  offsetProximoPendiente=0;
  if(confirmados==0) { offsetColaPreparado=true; return true; }
  File f=SD.open(ARCHIVO_COLA,FILE_READ);
  if(!f) { errorAlmacenamiento=true; actualizarOLED(true); return false; }
  for(uint32_t i=0;i<confirmados;i++) {
    if(!f.available()) {
      f.close(); errorAlmacenamiento=true; actualizarOLED(true); return false;
    }
    f.readStringUntil('\n');
  }
  offsetProximoPendiente=f.position();
  f.close();
  offsetColaPreparado=true;
  return true;
}

// Procesa COMO MAXIMO un registro por invocacion. No se ejecuta desde medir().
void reenviarPendientes() {
  if(!sdOK || errorAlmacenamiento || confirmados>=totalEnCola) return;
  if(!prepararOffsetCola()) return;
  File f=SD.open(ARCHIVO_COLA,FILE_READ);
  if(!f || !f.seek(offsetProximoPendiente)) {
    if(f) f.close();
    Serial.println("ERROR abrir/posicionar cola para reenvio");
    errorAlmacenamiento=true; actualizarOLED(true); return;
  }
  String linea=f.readStringUntil('\n');
  const uint32_t offsetSiguiente=f.position();
  f.close();
  String id,lp;
  if(!parsearRegistro(linea,id,lp)) {
    Serial.println("ERROR registro de cola invalido; no se avanza ACK");
    errorAlmacenamiento=true; actualizarOLED(true); return;
  }
  int codigo=-1;
  bool ok=enviarHTTP(id,lp,codigo);
  ultimoHTTP=codigo;
  Serial.printf("  %s | HTTP %d | %s\n",id.c_str(),codigo,ok?"CONFIRMADO":"PENDIENTE");
  if(!ok) {
    receptorSinRespuesta=true;
    if(!httpEnFalla) {
      httpEnFalla=true;
      evento("HTTP_ERROR","RECEPTOR",String("HTTP=")+String(codigo)+" pendientes="+String(totalEnCola-confirmados));
    }
    intentarEnvioPronto=false;
    actualizarOLED(true);
    return;
  }
  const uint32_t siguienteACK=confirmados+1;
  if(!guardarACK(siguienteACK)) {
    Serial.println("ERROR guardar ACK en SD");
    errorAlmacenamiento=true; actualizarOLED(true); return;
  }
  confirmados=siguienteACK;
  if(httpEnFalla) {
    httpEnFalla=false;
    evento("HTTP_RECUPERADO","RECEPTOR",String("HTTP=204 pendientes=")+String(totalEnCola-confirmados));
  }
  if(confirmados==totalEnCola && colaTeniaPendientes) {
    evento("COLA_VACIA","COLA",String("Confirmados=")+String(confirmados));
    colaTeniaPendientes=false;
  }
  offsetProximoPendiente=offsetSiguiente;
  intentarEnvioPronto=true;
  if(receptorSinRespuesta) {
    receptorSinRespuesta=false;
    inicioFaseOLED=millis(); faseEstadoOLED=false;
  }
  actualizarOLED(true);
}

void medir() {
  if(!rtcOK || !sdOK) return;
  DateTime d=rtc.now();
  if(!fechaValida(d)) { Serial.println("ERROR fecha RTC; ciclo cancelado"); evento("RTC_ERROR","DS3231","Fecha invalida"); return; }
  ciclo++;
  String fecha=fechaLocal(d);
  uint32_t utc=d.unixtime()+SEGUNDOS_A_UTC;
  Serial.printf("\n=== CICLO %lu | RTC %s ===\n",(unsigned long)ciclo,fecha.c_str());
  uint8_t ok=0,fallos=0,guardados=0;
  for(size_t i=0;i<N_SENSORES;i++) sensorUltimoOK[i]=false;
  for(size_t i=0;i<N_SENSORES;i++) {
    const Sensor &s=sensores[i];
    Serial.printf("%s MUX 0x%02X CH%u: ",s.nombre,s.mux,s.canal);
    if(!seleccionar(s)) { fallos++; verificarSensor(i,false); continue; }
    float t,h;
    if(!leerSHT(t,h)) { fallos++; verificarSensor(i,false); continue; }
    verificarSensor(i,true);
    ok++;
    if(APLICAR_OFFSETS_TEMPERATURA) t+=offsetTemperatura[i];
    tempUltima[i]=t; humUltima[i]=h; sensorUltimoOK[i]=true;
    Serial.printf("T=%.2f HR=%.2f CRC8 OK\n",t,h);
    String lp="higrotermico,sensor="+String(s.nombre)+
      " temperature_c="+String(t,2)+",humidity_rh="+String(h,2)+" "+String(utc);
    // Primero respaldo CSV; despues cola. Nunca enviar antes de persistir.
    String csv=fecha+","+String(s.nombre)+","+String(t,2)+","+String(h,2)+",OK";
    if(!appendLinea(ARCHIVO_CSV,csv)) {
      Serial.println("  ERROR guardar CSV; no se envia"); evento("SD_ERROR","CSV",s.nombre); errorAlmacenamiento=true; actualizarOLED(true); continue;
    }
    if(totalEnCola==UINT32_MAX) { Serial.println("  ERROR contador cola agotado"); break; }
    uint32_t siguiente=totalEnCola+1;
    char id[40]; snprintf(id,sizeof(id),"SPC01-%s-%010lu",s.nombre,(unsigned long)siguiente);
    if(!appendLinea(ARCHIVO_COLA,String(id)+"\t"+lp)) {
      Serial.println("  ERROR guardar cola; detener para revisar SD");
      evento("SD_ERROR","COLA",s.nombre);
      sdOK=false; errorAlmacenamiento=true; actualizarOLED(true); break;
    }
    totalEnCola=siguiente; guardados++;
    colaTeniaPendientes=true;
  }
  escribirMux(MUX_A,0); escribirMux(MUX_B,0);
  sensoresOKUltimoCiclo=ok; primeraMedicionLista=true; actualizarOLED(true);
  Serial.printf("RESUMEN %u/%u sensores OK | %u errores | %u en cola\n",
    ok,(unsigned)N_SENSORES,fallos,guardados);
  // No enviar aqui: la adquisicion termina y vuelve inmediatamente al scheduler.
}
void setup() {
  Serial.begin(115200); delay(1000);
  Serial.println("\nSPC FINAL 12 UBICACIONES + DS3231 + SD + HTTP CRC16 -> SQLite v2");
  Wire.begin(SDA_SENSORES,SCL_SENSORES); Wire.setClock(100000); Wire.setTimeOut(100);
  // MUX desconectados al detectar OLED para evitar otros dispositivos en el bus.
  escribirMux(MUX_A,0); escribirMux(MUX_B,0);
  oledOK=oled.begin(SSD1306_SWITCHCAPVCC,0x3C);
  if(oledOK) { oled.clearDisplay(); oled.setCursor(0,0); oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.println(F("JASON - SPC"));
    oled.println(F("BOCA CAMPEON!"));
    oled.println(F("Monitoreo T / HR"));
    oled.display();
    Serial.println("OLED SSD1306 OK (0x3C)");
  } else Serial.println("OLED no detectada; se continua sin pantalla");
  if(oledOK) delay(2500);
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
  // FAT toma la fecha de creacion/modificacion del reloj del sistema.
  // DS3231 contiene hora local argentina; settimeofday recibe UTC.
  if(rtcOK) {
    struct timeval tv;
    tv.tv_sec=(time_t)(rtc.now().unixtime()+SEGUNDOS_A_UTC);
    tv.tv_usec=0;
    settimeofday(&tv,nullptr);
    setenv("TZ","ART3",1); tzset();
    Serial.println("Reloj del ESP32 sincronizado con DS3231 para FAT");
  }
  spiSD.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
  sdOK=SD.begin(SD_CS,spiSD,1000000);
  if(!sdOK) Serial.println("ERROR inicializar microSD");
  else {
    Serial.println("microSD OK");
    if(!SD.exists(ARCHIVO_CSV)) {
      if(!appendLinea(ARCHIVO_CSV,"timestamp_local,ubicacion,temperatura_C,humedad_RH,estado")) sdOK=false;
    }
    if(sdOK) {
      confirmados=recuperarACK();
      sdOK=inspeccionarCola();
      if(sdOK) {
        if(!SD.exists(ARCHIVO_EVENTOS)) crearCabeceraEventos();
        colaTeniaPendientes=(totalEnCola>confirmados);
        evento("BOOT","ESP32",String("Jason iniciado; reset_reason=")+String((int)esp_reset_reason())+
               "; pendientes="+String(totalEnCola-confirmados));
      }
      Serial.printf("Cola SD: %lu registros | ACK %lu\n",
        (unsigned long)totalEnCola,(unsigned long)confirmados);
    }
  }
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  uint32_t inicio=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-inicio<15000) { delay(500); Serial.print('.'); }
  Serial.println();
  verificarEstadoWiFi();
  if(WiFi.status()==WL_CONNECTED) {
    Serial.print("WiFi OK IP ESP32: "); Serial.println(WiFi.localIP());
  } else Serial.println("WiFi no conectado; se guardaran pendientes");
  if(rtcOK && sdOK) { medir(); }
  else Serial.println("ADQUISICION BLOQUEADA por RTC/SD");
  ultimoCiclo=millis(); ultimoReintento=millis(); inicioFaseOLED=millis(); actualizarOLED(true);
}
void loop() {
  const uint32_t ahora=millis();
  actualizarOLED();
  verificarEstadoWiFi();

  // Primero la medicion. Mantener una referencia temporal fija evita que
  // el tiempo empleado en transmitir desplace continuamente los ciclos.
  if(rtcOK && sdOK && (uint32_t)(ahora-ultimoCiclo)>=INTERVALO_MS) {
    ultimoCiclo += INTERVALO_MS;
    // Si una operacion excepcional se demoro mas de un periodo,
    // no generar una rafaga de mediciones para recuperar tiempo.
    if((uint32_t)(ahora-ultimoCiclo)>=INTERVALO_MS)
      ultimoCiclo=ahora;
    medir();
    return;
  }

  if(!sdOK || errorAlmacenamiento || confirmados>=totalEnCola) return;
  const uint32_t tiempoHastaMedicion=INTERVALO_MS-(uint32_t)(ahora-ultimoCiclo);
  // Nunca iniciar una transaccion HTTP cerca del siguiente ciclo.
  if(rtcOK && tiempoHastaMedicion<=MARGEN_ADQUISICION_MS) return;

  const uint32_t espera=intentarEnvioPronto?PAUSA_ENTRE_ENVIOS_MS:REINTENTO_MS;
  if((uint32_t)(ahora-ultimoReintento)>=espera) {
    ultimoReintento=ahora;
    reenviarPendientes();
  }
}
