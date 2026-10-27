#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Descarga el PNG original de Mapbox en una tarea de FreeRTOS. El sketch
// principal lo decodifica al dibujarlo, para no tocar SPI desde la tarea de red.
class PipboyMapClient {
 public:
  static constexpr uint16_t ANCHO_MAPA = 205;
  static constexpr uint16_t ALTO_MAPA = 144;
  // El PNG de Mapbox de 205 x 144 suele ocupar alrededor de 10 KB. El limite
  // deja margen ante zonas urbanas densas sin aceptar respuestas arbitrarias.
  static constexpr size_t MAX_PNG_BYTES = 24576;

  enum class Estado : uint8_t {
    SinConfiguracion,
    EsperandoSolicitud,
    ConectandoWifi,
    Descargando,
    Listo,
    ErrorWifi,
    ErrorTls,
    ErrorHttp,
    ErrorFormato,
    ErrorMemoria,
  };

  struct Configuracion {
    const char *wifiSsid;
    const char *wifiPassword;
    const char *urlEndpoint;
    const char *claveDispositivo;
    const char *certificadoRaiz;
    uint32_t intervaloActualizacionMs;
    uint32_t timeoutWifiMs;
    uint32_t timeoutHttpMs;
    uint32_t reintentoMs;
    bool desconectarWifiTrasDescarga;
  };

  void iniciar(const Configuracion &configuracion);
  void solicitarActualizacion(float latitud, float longitud, uint8_t zoom,
                              bool forzar = false);
  void actualizar();

  bool hayMapaDisponible() const;
  bool hayMapaNuevo() const;
  const uint8_t *imagenPng() const;
  size_t bytesImagenPng() const;
  void confirmarMapaDibujado();
  Estado estado() const;
  const char *textoEstado() const;

  // Raiz ISRG Root X1 que valida la cadena actual de Let's Encrypt del
  // subdominio. Evita usar setInsecure() y no requiere almacenar el token.
  static const char *certificadoRaizLetsEncrypt();

 private:
  static void tareaDescarga(void *contexto);
  void descargarEnSegundoPlano();
  void iniciarTareaDescarga();
  void terminarConError(Estado error);
  void terminarConMapa(size_t bytesImagen);
  bool estaConfigurado() const;
  bool debeActualizar(uint32_t ahora) const;
  bool validarRespuesta(size_t bytesImagen) const;
  static bool tiempoAlcanzado(uint32_t ahora, uint32_t objetivo);

  Configuracion _configuracion = {};
  volatile Estado _estado = Estado::SinConfiguracion;
  volatile bool _wifiIniciado = false;
  volatile bool _tareaActiva = false;
  volatile bool _solicitudPendiente = false;
  volatile bool _forzarSolicitud = false;
  volatile bool _mapaDisponible = false;
  volatile bool _mapaNuevo = false;
  volatile uint32_t _inicioConexionWifiMs = 0;
  volatile uint32_t _ultimoMapaMs = 0;
  volatile uint32_t _proximoIntentoMs = 0;
  volatile size_t _bytesImagen = 0;
  float _latitud = 0.0f;
  float _longitud = 0.0f;
  uint8_t _zoom = 15;
  TaskHandle_t _tarea = nullptr;
  portMUX_TYPE _mutex = portMUX_INITIALIZER_UNLOCKED;
  uint8_t _respuesta[MAX_PNG_BYTES] = {};
};
