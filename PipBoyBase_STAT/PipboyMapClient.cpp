#include "PipboyMapClient.h"

#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {

constexpr uint32_t TAREA_STACK_BYTES = 8192;
constexpr UBaseType_t TAREA_PRIORIDAD = 1;
constexpr BaseType_t TAREA_NUCLEO = 0;
constexpr uint32_t LECTURA_INTERVALO_MS = 10;
constexpr size_t BLOQUE_DESCARGA_BYTES = 512;
constexpr char RUTA_MAPA_PNG[] = "/pipboy-map.png";
constexpr char RUTA_MAPA_TEMPORAL[] = "/pipboy-map.tmp";

// ISRG Root X1. La cadena HTTPS de pipboy.onix-software.com se verifico contra
// esta raiz; conservarla en flash permite TLS real sin modo inseguro.
const char CERTIFICADO_ISRG_ROOT_X1[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "m+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
    "-----END CERTIFICATE-----\n";

bool textoConfigurado(const char *texto) {
  return texto != nullptr && texto[0] != '\0' &&
         strstr(texto, "CAMBIAR_") == nullptr;
}

}  // namespace

void PipboyMapClient::iniciar(const Configuracion &configuracion) {
  const bool configurado = textoConfigurado(configuracion.wifiSsid) &&
                           textoConfigurado(configuracion.urlEndpoint) &&
                           textoConfigurado(configuracion.claveDispositivo) &&
                           textoConfigurado(configuracion.certificadoRaiz);
  const bool almacenamientoDisponible = SPIFFS.begin(false);
  if (almacenamientoDisponible) {
    SPIFFS.remove(RUTA_MAPA_TEMPORAL);
  }
  portENTER_CRITICAL(&_mutex);
  _configuracion = configuracion;
  _estado = !configurado ? Estado::SinConfiguracion
                          : (almacenamientoDisponible
                                 ? Estado::EsperandoSolicitud
                                 : Estado::ErrorAlmacenamiento);
  _wifiIniciado = false;
  _tareaActiva = false;
  _solicitudPendiente = false;
  _forzarSolicitud = false;
  _almacenamientoDisponible = almacenamientoDisponible;
  _mapaDisponible = false;
  _mapaNuevo = false;
  _ultimoMapaMs = 0;
  _proximoIntentoMs = 0;
  portEXIT_CRITICAL(&_mutex);
}

void PipboyMapClient::solicitarActualizacion(float latitud, float longitud,
                                              uint8_t zoom, bool forzar) {
  if (latitud < -90.0f || latitud > 90.0f || longitud < -180.0f ||
      longitud > 180.0f || zoom < 9 || zoom > 18 || !estaConfigurado()) {
    return;
  }

  portENTER_CRITICAL(&_mutex);
  _latitud = latitud;
  _longitud = longitud;
  _zoom = zoom;
  _solicitudPendiente = true;
  _forzarSolicitud = _forzarSolicitud || forzar;
  portEXIT_CRITICAL(&_mutex);
}

void PipboyMapClient::actualizar() {
  if (!estaConfigurado() || !_almacenamientoDisponible || _tareaActiva) return;

  const uint32_t ahora = millis();
  if (!_solicitudPendiente && _mapaDisponible && debeActualizar(ahora)) {
    _solicitudPendiente = true;
  }

  if (!_solicitudPendiente ||
      (_proximoIntentoMs != 0 && !tiempoAlcanzado(ahora, _proximoIntentoMs))) {
    return;
  }

  if (_mapaDisponible && !_forzarSolicitud && !debeActualizar(ahora)) {
    _solicitudPendiente = false;
    _estado = Estado::Listo;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (!_wifiIniciado) {
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(true);
      WiFi.begin(_configuracion.wifiSsid, _configuracion.wifiPassword);
      _wifiIniciado = true;
      _inicioConexionWifiMs = ahora;
      _estado = Estado::ConectandoWifi;
      return;
    }

    if (ahora - _inicioConexionWifiMs >= _configuracion.timeoutWifiMs) {
      WiFi.disconnect(false, false);
      _wifiIniciado = false;
      terminarConError(Estado::ErrorWifi);
    }
    return;
  }

  iniciarTareaDescarga();
}

bool PipboyMapClient::hayMapaDisponible() const { return _mapaDisponible; }

bool PipboyMapClient::hayMapaNuevo() const { return _mapaNuevo; }

const char *PipboyMapClient::rutaMapaPng() const {
  return _mapaDisponible ? RUTA_MAPA_PNG : nullptr;
}

void PipboyMapClient::confirmarMapaDibujado() { _mapaNuevo = false; }

PipboyMapClient::Estado PipboyMapClient::estado() const { return _estado; }

const char *PipboyMapClient::textoEstado() const {
  switch (_estado) {
    case Estado::SinConfiguracion: return "CONFIGURA WIFI Y API KEY";
    case Estado::EsperandoSolicitud: return "ESPERANDO MAPA";
    case Estado::ConectandoWifi: return "CONECTANDO WIFI...";
    case Estado::Descargando: return "DESCARGANDO MAPA...";
    case Estado::Listo: return "MAPA ACTUALIZADO";
    case Estado::ErrorWifi: return "ERROR WIFI / REINTENTO";
    case Estado::ErrorTls: return "ERROR TLS / REINTENTO";
    case Estado::ErrorHttp: return "ERROR API / REINTENTO";
    case Estado::ErrorFormato: return "PNG INVALIDO / REINTENTO";
    case Estado::ErrorAlmacenamiento: return "ERROR SPIFFS / REINICIA";
    case Estado::ErrorMemoria: return "ERROR TAREA / REINTENTO";
  }
  return "ESTADO DESCONOCIDO";
}

const char *PipboyMapClient::certificadoRaizLetsEncrypt() {
  return CERTIFICADO_ISRG_ROOT_X1;
}

void PipboyMapClient::tareaDescarga(void *contexto) {
  PipboyMapClient *cliente = static_cast<PipboyMapClient *>(contexto);
  cliente->descargarEnSegundoPlano();
  portENTER_CRITICAL(&cliente->_mutex);
  cliente->_tareaActiva = false;
  cliente->_tarea = nullptr;
  portEXIT_CRITICAL(&cliente->_mutex);
  vTaskDelete(nullptr);
}

void PipboyMapClient::iniciarTareaDescarga() {
  _estado = Estado::Descargando;
  _tareaActiva = true;
  _proximoIntentoMs = 0;

  const BaseType_t creada = xTaskCreatePinnedToCore(
      tareaDescarga, "PipboyMap", TAREA_STACK_BYTES, this, TAREA_PRIORIDAD,
      &_tarea, TAREA_NUCLEO);
  if (creada != pdPASS) {
    _tarea = nullptr;
    _tareaActiva = false;
    terminarConError(Estado::ErrorMemoria);
  }
}

void PipboyMapClient::descargarEnSegundoPlano() {
  WiFiClientSecure clienteSeguro;
  clienteSeguro.setCACert(_configuracion.certificadoRaiz);

  String url = String(_configuracion.urlEndpoint) + "?lat=" +
               String(_latitud, 6) + "&lon=" + String(_longitud, 6) +
               "&z=" + String(_zoom) + "&size=" + String(ANCHO_MAPA) +
               "x" + String(ALTO_MAPA);

  HTTPClient http;
  http.setConnectTimeout(_configuracion.timeoutHttpMs);
  http.setTimeout(_configuracion.timeoutHttpMs);
  http.setReuse(false);

  if (!http.begin(clienteSeguro, url)) {
    terminarConError(Estado::ErrorTls);
    return;
  }

  http.addHeader("X-Pipboy-Key", _configuracion.claveDispositivo);
  const int codigoHttp = http.GET();
  if (codigoHttp <= 0) {
    http.end();
    terminarConError(Estado::ErrorTls);
    return;
  }
  const int bytesEsperados = http.getSize();
  if (codigoHttp != HTTP_CODE_OK || bytesEsperados < 24 ||
      bytesEsperados > static_cast<int>(MAX_PNG_BYTES)) {
    http.end();
    terminarConError(Estado::ErrorHttp);
    return;
  }

  File archivoTemporal = SPIFFS.open(RUTA_MAPA_TEMPORAL, FILE_WRITE);
  if (!archivoTemporal) {
    http.end();
    terminarConError(Estado::ErrorAlmacenamiento);
    return;
  }

  WiFiClient *flujo = http.getStreamPtr();
  uint8_t bloque[BLOQUE_DESCARGA_BYTES];
  size_t recibidos = 0;
  bool escrituraCorrecta = true;
  uint32_t ultimoByteMs = millis();
  while (recibidos < static_cast<size_t>(bytesEsperados)) {
    const size_t disponibles = flujo->available();
    if (disponibles > 0) {
      const size_t porLeer =
          min(min(disponibles, BLOQUE_DESCARGA_BYTES),
              static_cast<size_t>(bytesEsperados) - recibidos);
      const size_t leidos = flujo->readBytes(
          reinterpret_cast<char *>(bloque), porLeer);
      if (leidos == 0) break;
      if (archivoTemporal.write(bloque, leidos) != leidos) {
        escrituraCorrecta = false;
        break;
      }
      recibidos += leidos;
      ultimoByteMs = millis();
      continue;
    }

    if (!http.connected() ||
        millis() - ultimoByteMs >= _configuracion.timeoutHttpMs) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(LECTURA_INTERVALO_MS));
  }
  archivoTemporal.close();
  http.end();

  if (!escrituraCorrecta) {
    SPIFFS.remove(RUTA_MAPA_TEMPORAL);
    terminarConError(Estado::ErrorAlmacenamiento);
    return;
  }
  if (recibidos != static_cast<size_t>(bytesEsperados) ||
      !validarArchivoMapa(recibidos)) {
    SPIFFS.remove(RUTA_MAPA_TEMPORAL);
    terminarConError(Estado::ErrorFormato);
    return;
  }

  if ((SPIFFS.exists(RUTA_MAPA_PNG) && !SPIFFS.remove(RUTA_MAPA_PNG)) ||
      !SPIFFS.rename(RUTA_MAPA_TEMPORAL, RUTA_MAPA_PNG)) {
    SPIFFS.remove(RUTA_MAPA_TEMPORAL);
    terminarConError(Estado::ErrorAlmacenamiento);
    return;
  }
  terminarConMapa();
}

void PipboyMapClient::terminarConError(Estado error) {
  if (_configuracion.desconectarWifiTrasDescarga) {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    _wifiIniciado = false;
  }
  portENTER_CRITICAL(&_mutex);
  _estado = error;
  _forzarSolicitud = false;
  _proximoIntentoMs = millis() + _configuracion.reintentoMs;
  portEXIT_CRITICAL(&_mutex);
}

void PipboyMapClient::terminarConMapa() {
  if (_configuracion.desconectarWifiTrasDescarga) {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    _wifiIniciado = false;
  }
  portENTER_CRITICAL(&_mutex);
  _ultimoMapaMs = millis();
  _proximoIntentoMs = 0;
  _solicitudPendiente = false;
  _forzarSolicitud = false;
  _mapaDisponible = true;
  _mapaNuevo = true;
  _estado = Estado::Listo;
  portEXIT_CRITICAL(&_mutex);
}

bool PipboyMapClient::estaConfigurado() const {
  return textoConfigurado(_configuracion.wifiSsid) &&
         textoConfigurado(_configuracion.urlEndpoint) &&
         textoConfigurado(_configuracion.claveDispositivo) &&
         textoConfigurado(_configuracion.certificadoRaiz);
}

bool PipboyMapClient::debeActualizar(uint32_t ahora) const {
  return _ultimoMapaMs == 0 ||
         ahora - _ultimoMapaMs >= _configuracion.intervaloActualizacionMs;
}

bool PipboyMapClient::validarArchivoMapa(size_t bytesImagen) const {
  if (bytesImagen < 24) {
    return false;
  }

  File archivoTemporal = SPIFFS.open(RUTA_MAPA_TEMPORAL, FILE_READ);
  uint8_t cabecera[24] = {};
  const size_t leidos = archivoTemporal
                             ? archivoTemporal.read(cabecera, sizeof(cabecera))
                             : 0;
  if (archivoTemporal) {
    archivoTemporal.close();
  }
  if (leidos != sizeof(cabecera) || cabecera[0] != 0x89 ||
      cabecera[1] != 'P' || cabecera[2] != 'N' || cabecera[3] != 'G' ||
      cabecera[4] != 0x0D || cabecera[5] != 0x0A || cabecera[6] != 0x1A ||
      cabecera[7] != 0x0A || cabecera[12] != 'I' || cabecera[13] != 'H' ||
      cabecera[14] != 'D' || cabecera[15] != 'R') {
    return false;
  }

  const uint32_t ancho = (static_cast<uint32_t>(cabecera[16]) << 24) |
                         (static_cast<uint32_t>(cabecera[17]) << 16) |
                         (static_cast<uint32_t>(cabecera[18]) << 8) |
                         cabecera[19];
  const uint32_t alto = (static_cast<uint32_t>(cabecera[20]) << 24) |
                        (static_cast<uint32_t>(cabecera[21]) << 16) |
                        (static_cast<uint32_t>(cabecera[22]) << 8) |
                        cabecera[23];
  return ancho == ANCHO_MAPA && alto == ALTO_MAPA;
}

bool PipboyMapClient::tiempoAlcanzado(uint32_t ahora, uint32_t objetivo) {
  return static_cast<int32_t>(ahora - objetivo) >= 0;
}
