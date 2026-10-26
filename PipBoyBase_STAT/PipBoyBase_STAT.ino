#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>

// 0: navegacion exclusiva por controles externos (valor predeterminado).
// 1: habilita el XPT2046 y la navegacion por toque.
#ifndef PIPBOY_TOUCH_HABILITADO
#define PIPBOY_TOUCH_HABILITADO 0
#endif

#if PIPBOY_TOUCH_HABILITADO
#include <XPT2046_Touchscreen.h>
#endif
#include "fonts/PipBoyFont16px7b.h"
#include "fonts/PipBoyFont24px7b.h"
#include "reference_pipboy_gif.h"
#include "reference_pipboy_thumbsup_gif.h"

// Configuracion de hardware confirmada para ESP32-2432S028R de dos USB.
constexpr int TFT_SCLK = 14;
constexpr int TFT_MISO = 12;
constexpr int TFT_MOSI = 13;
constexpr int TFT_CS = 15;
constexpr int TFT_DC = 2;
constexpr int TFT_BL = 21;

constexpr int TOUCH_SCLK = 25;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_CS = 33;
constexpr int TOUCH_IRQ = 36;

constexpr int TOUCH_X_MIN = 478;
constexpr int TOUCH_X_MAX = 3515;
constexpr int TOUCH_Y_MIN = 513;
constexpr int TOUCH_Y_MAX = 3209;

// Gris RGB565 compatible con Adafruit_ST7789.
constexpr uint16_t GRIS = 0x7BEF;
constexpr uint16_t VERDE_PIPBOY = ST77XX_GREEN;
constexpr uint16_t VERDE_ATENUADO = 0x03E0;
constexpr uint16_t VERDE_OSCURO = 0x0140;
constexpr uint16_t VERDE_BRILLO = 0xA7F3;

// Proporciones de la cabecera de referencia, adaptadas a 320 px de ancho.
constexpr int HEADER_ALTO = 28;
constexpr int HEADER_LINEA_Y = 22;
constexpr int HEADER_MARCO_Y = 11;
constexpr int HEADER_BASE_TEXTO_Y = 21;
constexpr int CENTROS_PESTANA[] = {64, 110, 154, 201, 251};
constexpr int INICIO_TOQUE_PESTANA[] = {44, 88, 132, 180, 224};
constexpr int FIN_TOQUE_PESTANA[] = {87, 131, 179, 223, 277};
constexpr int ICONO_TERMINAL_X = 294;
constexpr int ICONO_TERMINAL_Y = 3;
constexpr int ICONO_TERMINAL_ANCHO = 16;
constexpr int ICONO_TERMINAL_ALTO = 15;
constexpr uint32_t ICONO_TERMINAL_INTERVALO_MS = 80;

// El GIF fue preparado previamente a 76 x 145 px para reservar telemetria
// debajo de la animacion sin escalar pixeles durante la reproduccion.
constexpr int STAT_GIF_X = 122;
constexpr int STAT_GIF_Y = 50;
constexpr int STAT_GIF_ANCHO = 76;
constexpr int STAT_GIF_ALTO = 145;
constexpr uint32_t STAT_GIF_INTERVALO_MINIMO_MS = 35;

// Animacion mostrada una sola vez despues de cada arranque.
constexpr int ARRANQUE_GIF_ANCHO = 105;
constexpr int ARRANQUE_GIF_ALTO = 145;
constexpr int ARRANQUE_GIF_X = (320 - ARRANQUE_GIF_ANCHO) / 2;
constexpr int ARRANQUE_GIF_Y = (240 - ARRANQUE_GIF_ALTO) / 2;
constexpr uint32_t ARRANQUE_GIF_INTERVALO_MINIMO_MS = 30;

// Iconos monocromos para los dos bloques laterales de la cabecera.
constexpr uint8_t ICONO_ESTADO[] PROGMEM = {
  0b00111100,
  0b01111110,
  0b11100111,
  0b11011011,
  0b11011011,
  0b11100111,
  0b01111110,
  0b00111100,
};

SPIClass pantallaSPI(HSPI);
#if PIPBOY_TOUCH_HABILITADO
SPIClass touchSPI(VSPI);
#endif
Adafruit_ST7789 pantalla(&pantallaSPI, TFT_CS, TFT_DC, -1);
#if PIPBOY_TOUCH_HABILITADO
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
#endif
// Un solo decodificador se reutiliza: primero para el arranque y despues
// para STAT. Asi no se duplica el buffer de trabajo en RAM.
AnimatedGIF reproductorGif;

const char *pestanas[] = {"STAT", "INV", "DATA", "MAP", "RADIO"};
int pestanaActual = 0;
#if PIPBOY_TOUCH_HABILITADO
bool dedoEnPantalla = false;
#endif
bool gifStatDisponible = false;
bool reiniciarAnimacionStat = true;
uint32_t siguienteCuadroGifStat = 0;
bool animacionArranqueActiva = false;
bool ultimoCuadroArranqueMostrado = false;
uint32_t siguienteCuadroGifArranque = 0;
uint32_t ultimoFrameIconoTerminal = 0;
uint8_t faseIconoTerminal = 0;

// Se declara antes de la secuencia de arranque, que entra a la UI al acabar.
void dibujarPantalla();

// Fuente única de la interfaz: Monofonto, convertida a GFX y guardada en
// fonts/source/pipboy-font.otf. Añada texto de UI mediante estos ayudantes.
enum class TamanoTextoPipBoy : uint8_t { Normal, Principal };

void seleccionarFuentePipBoy(TamanoTextoPipBoy tamano) {
  pantalla.setTextSize(1);
  pantalla.setFont(tamano == TamanoTextoPipBoy::Normal
                      ? &PipBoyFont16px7b
                      : &PipBoyFont24px7b);
}

void escribirTextoPipBoy(const char *texto, int16_t x, int16_t lineaBase,
                         TamanoTextoPipBoy tamano) {
  seleccionarFuentePipBoy(tamano);
  pantalla.setCursor(x, lineaBase);
  pantalla.print(texto);
}

void escribirTextoPipBoyCentrado(const char *texto, int16_t centroX,
                                 int16_t lineaBase, TamanoTextoPipBoy tamano) {
  seleccionarFuentePipBoy(tamano);
  int16_t x1, y1;
  uint16_t ancho, alto;
  pantalla.getTextBounds(texto, 0, 0, &x1, &y1, &ancho, &alto);
  pantalla.setCursor(centroX - (ancho / 2), lineaBase);
  pantalla.print(texto);
}

// AnimatedGIF entrega una linea de cada cuadro. Se escribe por segmentos para
// respetar los pixeles transparentes sin usar un framebuffer en la RAM de la
// ESP32: el propio ST7789 conserva el cuadro anterior.
void dibujarLineaGifStat(GIFDRAW *linea) {
  const int y = STAT_GIF_Y + linea->iY + linea->y;
  if (y < STAT_GIF_Y || y >= STAT_GIF_Y + STAT_GIF_ALTO ||
      linea->iX >= STAT_GIF_ANCHO) {
    return;
  }

  const int ancho = min(linea->iWidth, STAT_GIF_ANCHO - linea->iX);
  if (ancho < 1) return;

  uint8_t *pixeles = linea->pPixels;
  if (linea->ucDisposalMethod == 2) {
    for (int x = 0; x < ancho; x++) {
      if (pixeles[x] == linea->ucTransparent) {
        pixeles[x] = linea->ucBackground;
      }
    }
    linea->ucHasTransparency = 0;
  }

  uint16_t bufferLinea[STAT_GIF_ANCHO];
  uint16_t *paleta = linea->pPalette;
  int x = 0;

  pantalla.startWrite();
  while (x < ancho) {
    while (x < ancho && linea->ucHasTransparency &&
           pixeles[x] == linea->ucTransparent) {
      x++;
    }
    const int inicioTramo = x;
    int cantidad = 0;

    while (x < ancho &&
           (!linea->ucHasTransparency || pixeles[x] != linea->ucTransparent)) {
      bufferLinea[cantidad++] = paleta[pixeles[x++]];
    }

    if (cantidad > 0) {
      pantalla.setAddrWindow(STAT_GIF_X + linea->iX + inicioTramo, y,
                             cantidad, 1);
      // La paleta se solicita en big-endian: el ST7789 recibe RGB565 directo.
      pantalla.writePixels(bufferLinea, cantidad, true, true);
    }
  }
  pantalla.endWrite();
}

void dibujarLineaGifArranque(GIFDRAW *linea) {
  const int y = ARRANQUE_GIF_Y + linea->iY + linea->y;
  if (y < ARRANQUE_GIF_Y || y >= ARRANQUE_GIF_Y + ARRANQUE_GIF_ALTO ||
      linea->iX >= ARRANQUE_GIF_ANCHO) {
    return;
  }

  const int ancho = min(linea->iWidth, ARRANQUE_GIF_ANCHO - linea->iX);
  if (ancho < 1) return;

  uint8_t *pixeles = linea->pPixels;
  if (linea->ucDisposalMethod == 2) {
    for (int x = 0; x < ancho; x++) {
      if (pixeles[x] == linea->ucTransparent) {
        pixeles[x] = linea->ucBackground;
      }
    }
    linea->ucHasTransparency = 0;
  }

  uint16_t bufferLinea[ARRANQUE_GIF_ANCHO];
  uint16_t *paleta = linea->pPalette;
  int x = 0;

  pantalla.startWrite();
  while (x < ancho) {
    while (x < ancho && linea->ucHasTransparency &&
           pixeles[x] == linea->ucTransparent) {
      x++;
    }
    const int inicioTramo = x;
    int cantidad = 0;

    while (x < ancho &&
           (!linea->ucHasTransparency || pixeles[x] != linea->ucTransparent)) {
      bufferLinea[cantidad++] = paleta[pixeles[x++]];
    }

    if (cantidad > 0) {
      pantalla.setAddrWindow(ARRANQUE_GIF_X + linea->iX + inicioTramo, y,
                             cantidad, 1);
      pantalla.writePixels(bufferLinea, cantidad, true, true);
    }
  }
  pantalla.endWrite();
}

void inicializarAnimacionStat() {
  reproductorGif.begin(GIF_PALETTE_RGB565_BE);
  gifStatDisponible = reproductorGif.openFLASH(
      const_cast<uint8_t *>(referencePipboyGif), referencePipboyGifSize,
      dibujarLineaGifStat);

  if (!gifStatDisponible) {
    Serial.printf("No se pudo abrir el GIF STAT (error %d)\n",
                  reproductorGif.getLastError());
  }
}

bool iniciarAnimacionArranque() {
  reproductorGif.begin(GIF_PALETTE_RGB565_BE);
  animacionArranqueActiva = reproductorGif.openFLASH(
      const_cast<uint8_t *>(referencePipboyThumbsupGif),
      referencePipboyThumbsupGifSize, dibujarLineaGifArranque);
  ultimoCuadroArranqueMostrado = false;
  siguienteCuadroGifArranque = 0;

  if (animacionArranqueActiva) {
    pantalla.fillScreen(ST77XX_BLACK);
  } else {
    Serial.printf("No se pudo abrir el GIF de arranque (error %d)\n",
                  reproductorGif.getLastError());
  }

  return animacionArranqueActiva;
}

void terminarAnimacionArranque() {
  reproductorGif.close();
  animacionArranqueActiva = false;
  inicializarAnimacionStat();
  dibujarPantalla();
}

void actualizarAnimacionArranque() {
  if (!animacionArranqueActiva) return;

  const uint32_t ahora = millis();
  if (static_cast<int32_t>(ahora - siguienteCuadroGifArranque) < 0) return;

  // El ultimo cuadro conserva su retraso antes de limpiar la pantalla y
  // entrar en STAT.
  if (ultimoCuadroArranqueMostrado) {
    terminarAnimacionArranque();
    return;
  }

  int retrasoGifMs = 0;
  const bool hayMasCuadros =
      reproductorGif.playFrame(false, &retrasoGifMs);
  if (retrasoGifMs < ARRANQUE_GIF_INTERVALO_MINIMO_MS) {
    retrasoGifMs = ARRANQUE_GIF_INTERVALO_MINIMO_MS;
  }
  siguienteCuadroGifArranque = ahora + retrasoGifMs;
  ultimoCuadroArranqueMostrado = !hayMasCuadros;
}

void actualizarAnimacionStat() {
  if (pestanaActual != 0 || !gifStatDisponible) return;

  if (reiniciarAnimacionStat) {
    reproductorGif.reset();
    reiniciarAnimacionStat = false;
    siguienteCuadroGifStat = 0;
  }

  const uint32_t ahora = millis();
  if (static_cast<int32_t>(ahora - siguienteCuadroGifStat) < 0) return;

  int retrasoGifMs = 0;
  const bool hayMasCuadros =
      reproductorGif.playFrame(false, &retrasoGifMs);
  if (!hayMasCuadros) {
    reproductorGif.reset();
  }

  if (retrasoGifMs < STAT_GIF_INTERVALO_MINIMO_MS) {
    retrasoGifMs = STAT_GIF_INTERVALO_MINIMO_MS;
  }
  siguienteCuadroGifStat = ahora + retrasoGifMs;
}

// Version compacta del icono de monitor: conserva marco, terminal, pulso y
// linea de escaneo dentro del bloque disponible de 16 x 15 px de la cabecera.
void dibujarIconoTerminal(uint8_t fase) {
  constexpr int x = ICONO_TERMINAL_X;
  constexpr int y = ICONO_TERMINAL_Y;

  pantalla.fillRect(x, y, ICONO_TERMINAL_ANCHO, ICONO_TERMINAL_ALTO,
                    ST77XX_BLACK);

  // Resplandor y cuerpo del monitor.
  pantalla.drawRect(x + 1, y + 1, 14, 11, VERDE_OSCURO);
  pantalla.drawRect(x + 2, y + 2, 12, 9, VERDE_PIPBOY);
  pantalla.fillRect(x + 3, y + 3, 10, 7, VERDE_OSCURO);

  // Lineas de terminal e indicador central.
  pantalla.drawFastHLine(x + 4, y + 4, 7, VERDE_PIPBOY);
  pantalla.drawFastHLine(x + 4, y + 6, 5, VERDE_PIPBOY);
  pantalla.fillRect(x + 10, y + 8, 2, 1, VERDE_ATENUADO);

  if (fase < 5) {
    pantalla.drawFastHLine(x + 4, y + 4, 7, VERDE_BRILLO);
    pantalla.fillRect(x + 10, y + 8, 1, 1, VERDE_PIPBOY);
  }

  const int scanY = y + 3 + (fase % 7);
  pantalla.drawFastHLine(x + 3, scanY, 10, VERDE_BRILLO);

  // Base del icono.
  pantalla.drawFastHLine(x + 4, y + 11, 8, VERDE_ATENUADO);
  pantalla.drawFastHLine(x + 2, y + 13, 12, VERDE_OSCURO);
}

void actualizarIconoTerminal() {
  const uint32_t ahora = millis();
  if (ahora - ultimoFrameIconoTerminal < ICONO_TERMINAL_INTERVALO_MS) return;

  ultimoFrameIconoTerminal = ahora;
  faseIconoTerminal = (faseIconoTerminal + 1) % 14;
  dibujarIconoTerminal(faseIconoTerminal);
}

void dibujarPestanas() {
  pantalla.fillRect(0, 0, 320, HEADER_ALTO, ST77XX_BLACK);

  // Bloques verdes laterales con sus pictogramas oscuros.
  pantalla.fillRect(11, 3, 16, 15, VERDE_PIPBOY);
  pantalla.drawBitmap(15, 6, ICONO_ESTADO, 8, 8, ST77XX_BLACK);
  dibujarIconoTerminal(faseIconoTerminal);

  seleccionarFuentePipBoy(TamanoTextoPipBoy::Normal);
  int16_t x1, y1;
  uint16_t ancho, alto;
  pantalla.getTextBounds(pestanas[pestanaActual], 0, 0, &x1, &y1, &ancho,
                         &alto);
  const int textoX = CENTROS_PESTANA[pestanaActual] - (ancho / 2);
  const int marcoX = textoX - 4;
  const int marcoAncho = ancho + 9;
  const int marcoFinX = marcoX + marcoAncho - 1;

  // La pestaña activa queda abierta por abajo: la línea de base se corta
  // exactamente entre sus dos laterales.
  pantalla.drawFastHLine(3, HEADER_LINEA_Y, marcoX - 3, VERDE_PIPBOY);
  pantalla.drawFastHLine(marcoFinX + 1, HEADER_LINEA_Y,
                         317 - marcoFinX, VERDE_PIPBOY);
  pantalla.drawFastVLine(3, HEADER_LINEA_Y, 5, VERDE_PIPBOY);
  pantalla.drawFastVLine(317, HEADER_LINEA_Y, 5, VERDE_PIPBOY);

  // El borde superior también se interrumpe en el ancho del texto, para que
  // el nombre de la pestaña lo atraviese limpiamente, como en la referencia.
  pantalla.drawFastVLine(marcoX, HEADER_MARCO_Y,
                         HEADER_LINEA_Y - HEADER_MARCO_Y + 1,
                         VERDE_PIPBOY);
  pantalla.drawFastHLine(marcoX, HEADER_MARCO_Y, textoX - marcoX,
                         VERDE_PIPBOY);
  pantalla.drawFastHLine(textoX + ancho, HEADER_MARCO_Y,
                         marcoFinX - (textoX + ancho) + 1, VERDE_PIPBOY);
  pantalla.drawFastVLine(marcoFinX, HEADER_MARCO_Y,
                         HEADER_LINEA_Y - HEADER_MARCO_Y + 1,
                         VERDE_PIPBOY);

  for (int i = 0; i < 5; i++) {
    pantalla.setTextColor(VERDE_PIPBOY);
    escribirTextoPipBoyCentrado(pestanas[i], CENTROS_PESTANA[i],
                                HEADER_BASE_TEXTO_Y,
                                TamanoTextoPipBoy::Normal);
  }
}

void dibujarBarraInferior() {
  pantalla.fillRect(0, 216, 320, 24, ST77XX_BLACK);
  pantalla.drawFastHLine(0, 216, 320, GRIS);
  pantalla.setTextColor(VERDE_PIPBOY);
  escribirTextoPipBoy("WIFI", 8, 232, TamanoTextoPipBoy::Normal);
  escribirTextoPipBoy("26 C", 85, 232, TamanoTextoPipBoy::Normal);
  escribirTextoPipBoy("12:34", 150, 232, TamanoTextoPipBoy::Normal);
  escribirTextoPipBoy("BAT 87%", 245, 232, TamanoTextoPipBoy::Normal);
}

void dibujarBarraVital(const char *etiqueta, int x, int porcentaje) {
  pantalla.setTextColor(VERDE_PIPBOY);
  escribirTextoPipBoy(etiqueta, x, 212, TamanoTextoPipBoy::Normal);
  pantalla.drawRect(x + 49, 201, 91, 11, VERDE_PIPBOY);
  pantalla.fillRect(x + 52, 204, (86 * porcentaje) / 100, 5,
                    VERDE_PIPBOY);
}

void dibujarStat() {
  pantalla.setTextColor(VERDE_ATENUADO);
  escribirTextoPipBoyCentrado("STATUS", 70, 44, TamanoTextoPipBoy::Normal);
  escribirTextoPipBoyCentrado("SPECIAL", 160, 44, TamanoTextoPipBoy::Normal);
  escribirTextoPipBoyCentrado("PERKS", 246, 44, TamanoTextoPipBoy::Normal);
  pantalla.setTextColor(VERDE_PIPBOY);
  escribirTextoPipBoyCentrado("STATUS", 70, 44, TamanoTextoPipBoy::Normal);

  if (!gifStatDisponible) {
    escribirTextoPipBoyCentrado("GIF NO DISPONIBLE", 160, 128,
                                TamanoTextoPipBoy::Normal);
  }

  dibujarBarraVital("HP 92", 8, 92);
  dibujarBarraVital("AP 73", 163, 73);
  reiniciarAnimacionStat = true;
}

void dibujarContenido() {
  pantalla.fillRect(0, HEADER_ALTO, 320, 216 - HEADER_ALTO, ST77XX_BLACK);
  if (pestanaActual == 0) {
    dibujarStat();
    return;
  }

  pantalla.setTextColor(VERDE_PIPBOY);
  escribirTextoPipBoy(pestanas[pestanaActual], 12, 76,
                      TamanoTextoPipBoy::Principal);
  escribirTextoPipBoy("Pantalla base sin diseno.", 12, 108,
                      TamanoTextoPipBoy::Normal);
#if PIPBOY_TOUCH_HABILITADO
  escribirTextoPipBoy("Touch y navegacion activos.", 12, 130,
                      TamanoTextoPipBoy::Normal);
#else
  escribirTextoPipBoy("Controles externos activos.", 12, 130,
                      TamanoTextoPipBoy::Normal);
#endif
}

void dibujarPantalla() {
  pantalla.fillScreen(ST77XX_BLACK);
  dibujarPestanas();
  dibujarContenido();
  dibujarBarraInferior();
}

// Punto de entrada comun para botones, encoder o el touch. Los controles
// externos pueden llamar a estas funciones sin conocer detalles de la UI.
void seleccionarPestana(uint8_t nuevaPestana) {
  if (nuevaPestana >= 5 || nuevaPestana == pestanaActual) return;

  pestanaActual = nuevaPestana;
  dibujarPantalla();
}

void seleccionarPestanaSiguiente() {
  seleccionarPestana((pestanaActual + 1) % 5);
}

void seleccionarPestanaAnterior() {
  seleccionarPestana((pestanaActual + 4) % 5);
}

#if PIPBOY_TOUCH_HABILITADO
void revisarTouch() {
  if (!touch.touched()) {
    dedoEnPantalla = false;
    return;
  }

  if (dedoEnPantalla) return;

  const TS_Point punto = touch.getPoint();
  if (punto.z < 100) return;

  dedoEnPantalla = true;
  // Se conserva el mapeo que funcionaba con la calibracion validada de esta
  // revision fisica de la placa.
  const int x = map(constrain(punto.x, TOUCH_X_MIN, TOUCH_X_MAX),
                    TOUCH_X_MIN, TOUCH_X_MAX, 0, 319);
  const int y = map(constrain(punto.y, TOUCH_Y_MIN, TOUCH_Y_MAX),
                    TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 239);

  if (y < HEADER_ALTO) {
    for (int i = 0; i < 5; i++) {
      if (x >= INICIO_TOQUE_PESTANA[i] && x <= FIN_TOQUE_PESTANA[i]) {
        seleccionarPestana(i);
        break;
      }
    }
  }
}
#endif

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pantallaSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  pantalla.init(240, 320);
  pantalla.setRotation(3);
  pantalla.invertDisplay(false);

#if PIPBOY_TOUCH_HABILITADO
  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
#endif

  if (!iniciarAnimacionArranque()) {
    inicializarAnimacionStat();
    dibujarPantalla();
  }
}

void loop() {
  if (animacionArranqueActiva) {
    actualizarAnimacionArranque();
    return;
  }

#if PIPBOY_TOUCH_HABILITADO
  revisarTouch();
#endif
  actualizarAnimacionStat();
  actualizarIconoTerminal();
}
