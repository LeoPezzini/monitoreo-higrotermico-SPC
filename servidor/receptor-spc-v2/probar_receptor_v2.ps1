# Ejecutar en PowerShell con el receptor v2 levantado en localhost:8081.
# Los ID de test son independientes de la adquisicion real.
function Get-Crc16CcittFalse([byte[]]$bytes) {
    [int]$crc = 0xFFFF
    foreach ($b in $bytes) {
        $crc = $crc -bxor ([int]$b -shl 8)
        for ($i=0; $i -lt 8; $i++) {
            if (($crc -band 0x8000) -ne 0) { $crc = (($crc -shl 1) -bxor 0x1021) -band 0xFFFF }
            else { $crc = ($crc -shl 1) -band 0xFFFF }
        }
    }
    return ('{0:X4}' -f $crc)
}
function Send-Spc([string]$id, [string]$lp, [string]$crcOverride = '') {
    $utf8 = [System.Text.Encoding]::UTF8
    $body = $utf8.GetBytes($lp)
    $crc = Get-Crc16CcittFalse ($utf8.GetBytes($id + "`n" + $lp))
    if ($crcOverride) { $crc = $crcOverride }
    try {
        $response = Invoke-WebRequest -Uri 'http://localhost:8081/api/v2/write' -Method POST -Body $body -ContentType 'text/plain; charset=utf-8' -Headers @{'X-SPC-ID'=$id;'X-SPC-CRC16'=$crc} -UseBasicParsing -ErrorAction Stop
        Write-Host "ID=$id HTTP=$($response.StatusCode) CRC=$crc"
    } catch {
        $status = if ($_.Exception.Response) { [int]$_.Exception.Response.StatusCode } else { 'SIN_RESPUESTA' }
        Write-Host "ID=$id HTTP=$status CRC=$crc"
    }
}
$id = 'SPC01-TESTCRC-20261112-0001'
$lp = 'higrotermico,sensor=INT01 temperature_c=25.36,humidity_rh=34.44 1793923126'
Write-Host '1) Nueva -> 204'
Send-Spc $id $lp
Write-Host '2) Misma medicion repetida -> 204, sin duplicar'
Send-Spc $id $lp
Write-Host '3) Datos alterados pero CRC original -> 422'
$id2 = 'SPC01-TESTCRC-20261112-0002'
$crcOriginal = Get-Crc16CcittFalse ([System.Text.Encoding]::UTF8.GetBytes($id2 + "`n" + $lp))
Send-Spc $id2 ($lp.Replace('25.36','29.36')) $crcOriginal
Write-Host '4) Mismo ID con contenido diferente y CRC valido -> 409'
Send-Spc $id ($lp.Replace('25.36','29.36'))
