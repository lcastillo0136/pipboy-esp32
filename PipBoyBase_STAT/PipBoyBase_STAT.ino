#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>

// 0: navegacion exclusiva por controles externos (valor predeterminado).
// 1: habilita el XPT2046 y la navegacion por toque.
#ifndef PIPBOY_TOUCH_HABILITADO
#define PIPBOY_TOUCH_HABILITADO 0
#endif

// El boton BOOT de la ESP32-2432S028R esta conectado a GPIO0. Solo debe
// mantenerse pulsado despues del arranque: durante RESET selecciona el modo
// de carga de firmware.
#ifndef PIPBOY_BOTON_BOOT_HABILITADO
#define PIPBOY_BOTON_BOOT_HABILITADO 1
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
constexpr int BOTON_BOOT_GPIO = 0;
constexpr uint32_t BOTON_BOOT_REBOTE_MS = 35;
constexpr uint32_t BOTON_BOOT_PULSACION_LARGA_MS = 700;

constexpr int TOUCH_SCLK = 25;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_CS = 33;
constexpr int TOUCH_IRQ = 36;

constexpr int TOUCH_X_MIN = 478;
constexpr int TOUCH_X_MAX = 3515;
constexpr int TOUCH_Y_MIN = 513;
constexpr int TOUCH_Y_MAX = 3209;

constexpr uint16_t VERDE_PIPBOY = ST77XX_GREEN;
constexpr uint16_t VERDE_ATENUADO = 0x03E0;
constexpr uint16_t VERDE_OSCURO = 0x0140;
constexpr uint16_t VERDE_BRILLO = 0xA7F3;
// Fondo RGB565 ajustable para los paneles del footer.
constexpr uint16_t FONDO_SECCION_FOOTER = 0x0100;
// Franja compacta anclada al borde inferior, con 2 px arriba y abajo.
constexpr int FOOTER_Y = 228;
constexpr int FOOTER_ALTO = 12;
constexpr int FOOTER_PADDING_VERTICAL = 2;
constexpr int FOOTER_CONTENIDO_Y = FOOTER_Y + FOOTER_PADDING_VERTICAL;
constexpr int FOOTER_CONTENIDO_ALTO = FOOTER_ALTO -
                                      (FOOTER_PADDING_VERTICAL * 2);

// Proporciones de la cabecera de referencia, adaptadas a 320 px de ancho.
constexpr int HEADER_ALTO = 28;
constexpr int HEADER_LINEA_Y = 22;
constexpr int HEADER_MARCO_Y = 11;
constexpr int HEADER_BASE_TEXTO_Y = 21;
constexpr uint8_t PESTANA_STAT = 0;
constexpr uint8_t PESTANA_DATA = 2;
constexpr int DATA_RELOJ_CENTRO_X = 160;
constexpr int DATA_RELOJ_LINEA_BASE = 154;
constexpr int DATA_RELOJ_ESCALA = 3;
constexpr uint32_t DATA_RELOJ_PARPADEO_MS = 500;
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

// Consola de prearranque. Usa una microfuente 3 x 5 px para conservar la
// legibilidad sin ocupar demasiado espacio de la pantalla fisica.
constexpr int CONSOLA_MARGEN_X = 4;
constexpr int CONSOLA_MARGEN_Y = 4;
constexpr int CONSOLA_ANCHO_CARACTER = 4;
constexpr int CONSOLA_ALTO_CARACTER = 6;
constexpr int CONSOLA_COLUMNAS = 78;
constexpr int CONSOLA_FILAS = 38;
constexpr uint32_t CONSOLA_INTERVALO_CARACTER_MS = 12;
constexpr uint32_t CONSOLA_PAUSA_LINEA_MS = 20;
constexpr uint32_t CONSOLA_PAUSA_FINAL_MS = 800;

struct FechaHora {
  uint16_t anio;
  uint8_t mes;
  uint8_t dia;
  uint8_t hora;
  uint8_t minuto;
  uint8_t segundo;
};

struct ConfiguracionPerfil {
  uint16_t anioNacimiento;
  uint8_t mesNacimiento;
  uint8_t diaNacimiento;
  uint8_t bateriaPorcentaje;
};

// Valores de prueba. Mas adelante esta estructura se cargara y guardara en
// microSD cuando se confirme su pinout y se implemente la pantalla SETTINGS.
constexpr ConfiguracionPerfil PERFIL_PRUEBA = {1991, 1, 7, 87};
constexpr FechaHora FECHA_HORA_PRUEBA = {2026, 10, 26, 12, 0, 0};
constexpr uint32_t FOOTER_INTERVALO_ACTUALIZACION_MS = 1000;

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
enum class EstadoArranque : uint8_t { Consola, PausaAntesGif, Gif };
EstadoArranque estadoArranque = EstadoArranque::Consola;
char consolaArranque[CONSOLA_FILAS][CONSOLA_COLUMNAS];
uint8_t filaConsola = 0;
uint8_t columnaConsola = 0;
uint8_t lineaTextoArranque = 0;
uint8_t caracterTextoArranque = 0;
uint32_t siguienteCaracterConsola = 0;
uint32_t ultimoFrameIconoTerminal = 0;
uint8_t faseIconoTerminal = 0;
uint32_t inicioRelojPruebaMs = 0;
uint32_t ultimaActualizacionFooterMs = 0;
bool footerInicializado = false;
FechaHora fechaFooterDibujada = {0, 0, 0, 0, 0, 0};
uint16_t edadFooterDibujada = 0;
uint8_t progresoFooterDibujado = 0;
uint8_t bateriaFooterDibujada = 0;
uint8_t horaDataDibujada = 255;
uint8_t minutoDataDibujado = 255;
bool dosPuntosDataVisibles = true;
uint32_t ultimoParpadeoDataMs = 0;
#if PIPBOY_BOTON_BOOT_HABILITADO
bool lecturaBotonBootAnterior = false;
bool botonBootEstable = false;
uint32_t ultimoCambioBotonBootMs = 0;
uint32_t inicioPulsacionBootMs = 0;
#endif

const char *const TEXTO_ARRANQUE[] = {
  "LOADER V1.1",
  "",
  "EXEC VERSION 41.10",
  "64k RAM SYSTEM",
  "38911 BYTES FREE",
  "",
  "NO WILDTAPE FOUND",
  "LOAD ROM(1): DEUTRIX 303",
  "",
  "1 0 0x00000A 0x0000000000000000 start memory discovery",
  "1 0 0x000014 0x0000000000000000 CPU0 starting cell relocation0",
  "1 0 0x000009 0x0000000000000000 CPU0 launch EFI0 0x000004",
  "1 0 0x000009 0x0000000000E003D CPU0 starting EFI0 0x000004",
  "1 0 0x000004 0x0000000000000000 start memory discovery 0x000004",
  "1 0 0x000004 0x0000000000000000 start memory discovery 0",
  "1 0 0x000014 0x0000000000000000 CPU0 starting cell relocation0",
  "1 0 0x000009 0x0000000000000000 CPU0 launch EFI0",
  "1 0 0x000009 0x0000000000E003D CPU0 starting EFI0",
  "1 0 0x000009 0x0000000000000000 start memory discovery0",
  "1 0 0x000009 0x0000000000000000 start memory discovery 0",
  "1 0 0x000014 0x0000000000000000 CPU0 starting cell relocation0",
  "1 0 0x000014 0x0000000000000000 CPU0 launch EFI0",
  "1 0 0x000014 0x0000000000000000 CPU0 starting EFI0",
  "1 0 0x000009 0x0000000000E003D CPU0 starting EFI0",
  "",
  "JIFFY INITIALIZATION",
  "MEASURING OFFSETS",
  "FILE SYSTEM CHECK",
  "",
  "PLEASE STAND BY...",
};
constexpr uint8_t TOTAL_LINEAS_TEXTO_ARRANQUE =
    sizeof(TEXTO_ARRANQUE) / sizeof(TEXTO_ARRANQUE[0]);

// A-Z, 0-9, punto, dos puntos, parentesis, asterisco, guion y espacio.
// Cada fila usa los tres bits menos significativos.
const uint8_t FUENTE_MICRO[][5] PROGMEM = {
  {0b111, 0b101, 0b111, 0b101, 0b101}, // A
  {0b110, 0b101, 0b110, 0b101, 0b110}, // B
  {0b111, 0b100, 0b100, 0b100, 0b111}, // C
  {0b110, 0b101, 0b101, 0b101, 0b110}, // D
  {0b111, 0b100, 0b110, 0b100, 0b111}, // E
  {0b111, 0b100, 0b110, 0b100, 0b100}, // F
  {0b111, 0b100, 0b101, 0b101, 0b111}, // G
  {0b101, 0b101, 0b111, 0b101, 0b101}, // H
  {0b111, 0b010, 0b010, 0b010, 0b111}, // I
  {0b001, 0b001, 0b001, 0b101, 0b010}, // J
  {0b101, 0b101, 0b110, 0b101, 0b101}, // K
  {0b100, 0b100, 0b100, 0b100, 0b111}, // L
  {0b101, 0b111, 0b111, 0b101, 0b101}, // M
  {0b101, 0b111, 0b111, 0b111, 0b101}, // N
  {0b111, 0b101, 0b101, 0b101, 0b111}, // O
  {0b110, 0b101, 0b110, 0b100, 0b100}, // P
  {0b111, 0b101, 0b101, 0b111, 0b001}, // Q
  {0b110, 0b101, 0b110, 0b101, 0b101}, // R
  {0b111, 0b100, 0b111, 0b001, 0b111}, // S
  {0b111, 0b010, 0b010, 0b010, 0b010}, // T
  {0b101, 0b101, 0b101, 0b101, 0b111}, // U
  {0b101, 0b101, 0b101, 0b101, 0b010}, // V
  {0b101, 0b101, 0b111, 0b111, 0b101}, // W
  {0b101, 0b101, 0b010, 0b101, 0b101}, // X
  {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
  {0b111, 0b001, 0b010, 0b100, 0b111}, // Z
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b110, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b110, 0b001, 0b011, 0b001, 0b110}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b010, 0b010, 0b010}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
  {0b000, 0b000, 0b000, 0b000, 0b010}, // .
  {0b000, 0b010, 0b000, 0b010, 0b000}, // :
  {0b011, 0b100, 0b100, 0b100, 0b011}, // (
  {0b110, 0b001, 0b001, 0b001, 0b110}, // )
  {0b000, 0b101, 0b010, 0b101, 0b000}, // *
  {0b000, 0b000, 0b111, 0b000, 0b000}, // -
  {0b000, 0b000, 0b000, 0b000, 0b000}, // espacio
};

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

int8_t indiceGlifoMicro(char caracter) {
  if (caracter >= 'a' && caracter <= 'z') caracter -= 'a' - 'A';
  if (caracter >= 'A' && caracter <= 'Z') return caracter - 'A';
  if (caracter >= '0' && caracter <= '9') return 26 + caracter - '0';

  switch (caracter) {
    case '.': return 36;
    case ':': return 37;
    case '(': return 38;
    case ')': return 39;
    case '*': return 40;
    case '-': return 41;
    case ' ': return 42;
    default: return -1;
  }
}

void dibujarCaracterConsolaMicro(int columna, int fila, char caracter) {
  const int8_t indice = indiceGlifoMicro(caracter);
  if (indice < 0 || caracter == ' ') return;

  const int x = CONSOLA_MARGEN_X + columna * CONSOLA_ANCHO_CARACTER;
  const int y = CONSOLA_MARGEN_Y + fila * CONSOLA_ALTO_CARACTER;
  for (int filaGlifo = 0; filaGlifo < 5; filaGlifo++) {
    const uint8_t bits = pgm_read_byte(&FUENTE_MICRO[indice][filaGlifo]);
    for (int columnaGlifo = 0; columnaGlifo < 3; columnaGlifo++) {
      if (bits & (1 << (2 - columnaGlifo))) {
        pantalla.drawPixel(x + columnaGlifo, y + filaGlifo, VERDE_PIPBOY);
      }
    }
  }
}

void limpiarConsolaArranque() {
  pantalla.fillScreen(ST77XX_BLACK);

  for (int fila = 0; fila < CONSOLA_FILAS; fila++) {
    for (int columna = 0; columna < CONSOLA_COLUMNAS; columna++) {
      consolaArranque[fila][columna] = ' ';
    }
  }
}

void redibujarConsolaArranque() {
  pantalla.fillScreen(ST77XX_BLACK);

  for (int fila = 0; fila < CONSOLA_FILAS; fila++) {
    for (int columna = 0; columna < CONSOLA_COLUMNAS; columna++) {
      dibujarCaracterConsolaMicro(columna, fila,
                                  consolaArranque[fila][columna]);
    }
  }
}

void avanzarLineaConsolaArranque() {
  columnaConsola = 0;
  if (filaConsola < CONSOLA_FILAS - 1) {
    filaConsola++;
    return;
  }

  for (int fila = 0; fila < CONSOLA_FILAS - 1; fila++) {
    for (int columna = 0; columna < CONSOLA_COLUMNAS; columna++) {
      consolaArranque[fila][columna] = consolaArranque[fila + 1][columna];
    }
  }
  for (int columna = 0; columna < CONSOLA_COLUMNAS; columna++) {
    consolaArranque[CONSOLA_FILAS - 1][columna] = ' ';
  }
  redibujarConsolaArranque();
}

void escribirCaracterConsolaArranque(char caracter) {
  if (columnaConsola >= CONSOLA_COLUMNAS) {
    avanzarLineaConsolaArranque();
  }

  consolaArranque[filaConsola][columnaConsola] = caracter;
  dibujarCaracterConsolaMicro(columnaConsola, filaConsola, caracter);
  columnaConsola++;
}

bool iniciarGifArranque() {
  reproductorGif.begin(GIF_PALETTE_RGB565_BE);
  const bool gifAbierto = reproductorGif.openFLASH(
      const_cast<uint8_t *>(referencePipboyThumbsupGif),
      referencePipboyThumbsupGifSize, dibujarLineaGifArranque);
  ultimoCuadroArranqueMostrado = false;
  siguienteCuadroGifArranque = 0;

  if (gifAbierto) {
    pantalla.fillScreen(ST77XX_BLACK);
    estadoArranque = EstadoArranque::Gif;
  } else {
    Serial.printf("No se pudo abrir el GIF de arranque (error %d)\n",
                  reproductorGif.getLastError());
  }
  return gifAbierto;
}

bool iniciarAnimacionArranque() {
  animacionArranqueActiva = true;
  estadoArranque = EstadoArranque::Consola;
  filaConsola = 0;
  columnaConsola = 0;
  lineaTextoArranque = 0;
  caracterTextoArranque = 0;
  siguienteCaracterConsola = 0;
  limpiarConsolaArranque();
  return true;
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

  if (estadoArranque == EstadoArranque::Consola) {
    if (static_cast<int32_t>(ahora - siguienteCaracterConsola) < 0) return;

    const char *linea = TEXTO_ARRANQUE[lineaTextoArranque];
    const char caracter = linea[caracterTextoArranque];
    if (caracter != '\0') {
      escribirCaracterConsolaArranque(caracter);
      caracterTextoArranque++;
      siguienteCaracterConsola = ahora + CONSOLA_INTERVALO_CARACTER_MS;
      return;
    }

    if (lineaTextoArranque + 1 >= TOTAL_LINEAS_TEXTO_ARRANQUE) {
      estadoArranque = EstadoArranque::PausaAntesGif;
      siguienteCaracterConsola = ahora + CONSOLA_PAUSA_FINAL_MS;
      return;
    }

    avanzarLineaConsolaArranque();
    lineaTextoArranque++;
    caracterTextoArranque = 0;
    siguienteCaracterConsola = ahora + CONSOLA_PAUSA_LINEA_MS;
    return;
  }

  if (estadoArranque == EstadoArranque::PausaAntesGif) {
    if (static_cast<int32_t>(ahora - siguienteCaracterConsola) < 0) return;
    if (!iniciarGifArranque()) {
      terminarAnimacionArranque();
    }
    return;
  }

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

// Adaptacion compacta del engrane: fondo oscuro, doble marco, cuerpo solido
// y un brillo que recorre sus ocho dientes.
void dibujarIconoEngrane(uint8_t fase) {
  constexpr int x = 11;
  constexpr int y = 3;
  constexpr int centroX = x + 8;
  constexpr int centroY = y + 7;

  pantalla.fillRect(x, y, 16, 15, ST77XX_BLACK);
  pantalla.drawRect(x + 1, y + 1, 14, 13, VERDE_OSCURO);
  pantalla.drawRect(x + 2, y + 2, 12, 11, VERDE_ATENUADO);
  pantalla.fillRect(x + 3, y + 3, 10, 9, VERDE_OSCURO);

  // Dientes principales y diagonales.
  pantalla.fillRect(centroX - 1, centroY - 6, 2, 3, VERDE_PIPBOY);
  pantalla.fillRect(centroX - 1, centroY + 4, 2, 3, VERDE_PIPBOY);
  pantalla.fillRect(centroX - 6, centroY - 1, 3, 2, VERDE_PIPBOY);
  pantalla.fillRect(centroX + 4, centroY - 1, 3, 2, VERDE_PIPBOY);
  pantalla.fillRect(centroX - 4, centroY - 4, 2, 2, VERDE_PIPBOY);
  pantalla.fillRect(centroX + 2, centroY - 4, 2, 2, VERDE_PIPBOY);
  pantalla.fillRect(centroX - 4, centroY + 2, 2, 2, VERDE_PIPBOY);
  pantalla.fillRect(centroX + 2, centroY + 2, 2, 2, VERDE_PIPBOY);

  pantalla.fillCircle(centroX, centroY, 4, VERDE_ATENUADO);
  pantalla.drawCircle(centroX, centroY, 4, VERDE_PIPBOY);
  pantalla.fillCircle(centroX, centroY, 2, VERDE_OSCURO);
  pantalla.drawCircle(centroX, centroY, 2, VERDE_PIPBOY);
  pantalla.drawPixel(centroX, centroY, VERDE_ATENUADO);

  // El brillo recorre los dientes en ocho pasos.
  switch (fase % 8) {
    case 0: pantalla.fillRect(centroX - 1, centroY - 6, 2, 3, VERDE_BRILLO); break;
    case 1: pantalla.fillRect(centroX + 2, centroY - 4, 2, 2, VERDE_BRILLO); break;
    case 2: pantalla.fillRect(centroX + 4, centroY - 1, 3, 2, VERDE_BRILLO); break;
    case 3: pantalla.fillRect(centroX + 2, centroY + 2, 2, 2, VERDE_BRILLO); break;
    case 4: pantalla.fillRect(centroX - 1, centroY + 4, 2, 3, VERDE_BRILLO); break;
    case 5: pantalla.fillRect(centroX - 4, centroY + 2, 2, 2, VERDE_BRILLO); break;
    case 6: pantalla.fillRect(centroX - 6, centroY - 1, 3, 2, VERDE_BRILLO); break;
    default: pantalla.fillRect(centroX - 4, centroY - 4, 2, 2, VERDE_BRILLO); break;
  }
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
  dibujarIconoEngrane(faseIconoTerminal);
  dibujarIconoTerminal(faseIconoTerminal);
}

void dibujarPestanas() {
  pantalla.fillRect(0, 0, 320, HEADER_ALTO, ST77XX_BLACK);

  dibujarIconoEngrane(faseIconoTerminal);
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

bool esAnioBisiesto(uint16_t anio) {
  return (anio % 4 == 0 && anio % 100 != 0) || anio % 400 == 0;
}

uint8_t diasDelMes(uint16_t anio, uint8_t mes) {
  static const uint8_t DIAS_POR_MES[] = {31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};
  if (mes == 2 && esAnioBisiesto(anio)) return 29;
  return DIAS_POR_MES[mes - 1];
}

void avanzarDia(FechaHora &fecha) {
  fecha.dia++;
  if (fecha.dia <= diasDelMes(fecha.anio, fecha.mes)) return;

  fecha.dia = 1;
  fecha.mes++;
  if (fecha.mes <= 12) return;

  fecha.mes = 1;
  fecha.anio++;
}

int32_t diasDesdeEpoch(const FechaHora &fecha) {
  const uint32_t aniosCompletos = fecha.anio - 1;
  int32_t dias = 365L * aniosCompletos + aniosCompletos / 4 -
                 aniosCompletos / 100 + aniosCompletos / 400;
  for (uint8_t mes = 1; mes < fecha.mes; mes++) {
    dias += diasDelMes(fecha.anio, mes);
  }
  return dias + fecha.dia - 1;
}

FechaHora obtenerFechaHoraPrueba() {
  FechaHora fecha = FECHA_HORA_PRUEBA;
  uint32_t segundos = (millis() - inicioRelojPruebaMs) / 1000;
  segundos += fecha.hora * 3600UL + fecha.minuto * 60UL + fecha.segundo;

  while (segundos >= 86400UL) {
    segundos -= 86400UL;
    avanzarDia(fecha);
  }

  fecha.hora = segundos / 3600UL;
  segundos %= 3600UL;
  fecha.minuto = segundos / 60UL;
  fecha.segundo = segundos % 60UL;
  return fecha;
}

uint16_t calcularEdad(const FechaHora &fecha) {
  if (fecha.anio < PERFIL_PRUEBA.anioNacimiento) return 0;

  uint16_t edad = fecha.anio - PERFIL_PRUEBA.anioNacimiento;
  const bool aunNoCumple = fecha.mes < PERFIL_PRUEBA.mesNacimiento ||
                           (fecha.mes == PERFIL_PRUEBA.mesNacimiento &&
                            fecha.dia < PERFIL_PRUEBA.diaNacimiento);
  return aunNoCumple && edad > 0 ? edad - 1 : edad;
}

uint8_t calcularProgresoNivel(const FechaHora &fecha) {
  const bool yaCumplio = fecha.mes > PERFIL_PRUEBA.mesNacimiento ||
                         (fecha.mes == PERFIL_PRUEBA.mesNacimiento &&
                          fecha.dia >= PERFIL_PRUEBA.diaNacimiento);
  const uint16_t anioUltimoCumple = fecha.anio - (yaCumplio ? 0 : 1);
  FechaHora inicio = {anioUltimoCumple, PERFIL_PRUEBA.mesNacimiento,
                      PERFIL_PRUEBA.diaNacimiento, 0, 0, 0};
  avanzarDia(inicio);  // El progreso comienza el dia posterior al cumpleanos.
  const FechaHora siguienteCumple = {
      static_cast<uint16_t>(anioUltimoCumple + 1),
      PERFIL_PRUEBA.mesNacimiento, PERFIL_PRUEBA.diaNacimiento, 0, 0, 0};

  const int32_t total = diasDesdeEpoch(siguienteCumple) - diasDesdeEpoch(inicio);
  const int32_t transcurridos = diasDesdeEpoch(fecha) - diasDesdeEpoch(inicio);
  if (transcurridos <= 0) return 0;
  if (transcurridos >= total) return 100;
  return static_cast<uint8_t>((transcurridos * 100L) / total);
}

void escribirTextoFooter(const char *texto, int16_t x, int16_t y) {
  pantalla.setFont(NULL);
  pantalla.setTextSize(1);
  pantalla.setTextColor(VERDE_PIPBOY, FONDO_SECCION_FOOTER);
  pantalla.setCursor(x, y);
  pantalla.print(texto);
}

void dibujarIconoBateria(int16_t x, int16_t y, uint8_t porcentaje) {
  const uint8_t porcentajeLimitado = porcentaje > 100 ? 100 : porcentaje;
  constexpr int ANCHO = 42;
  constexpr int ALTO = 8;
  pantalla.drawRect(x, y, ANCHO, ALTO, VERDE_PIPBOY);
  pantalla.fillRect(x + ANCHO, y + 2, 3, 4, VERDE_PIPBOY);
  pantalla.fillRect(x + 2, y + 2, ((ANCHO - 4) * porcentajeLimitado) / 100,
                    ALTO - 4, VERDE_PIPBOY);
}

void dibujarEstructuraFooter() {
  // Tres secciones: fecha/hora | nivel | bateria. Esta estructura solo se
  // pinta al abrir la pantalla; asi el refresco de datos no causa parpadeo.
  pantalla.fillRect(0, FOOTER_Y, 106, FOOTER_ALTO, FONDO_SECCION_FOOTER);
  pantalla.fillRect(106, FOOTER_Y, 3, FOOTER_ALTO, ST77XX_BLACK);
  pantalla.fillRect(109, FOOTER_Y, 103, FOOTER_ALTO,
                    FONDO_SECCION_FOOTER);
  pantalla.fillRect(212, FOOTER_Y, 3, FOOTER_ALTO, ST77XX_BLACK);
  pantalla.fillRect(215, FOOTER_Y, 105, FOOTER_ALTO,
                    FONDO_SECCION_FOOTER);
}

void dibujarFechaFooter(const FechaHora &fecha) {
  char fechaTexto[11];
  snprintf(fechaTexto, sizeof(fechaTexto), "%04u-%02u-%02u",
           static_cast<unsigned int>(fecha.anio),
           static_cast<unsigned int>(fecha.mes),
           static_cast<unsigned int>(fecha.dia));
  escribirTextoFooter(fechaTexto, 5, FOOTER_CONTENIDO_Y);
}

void dibujarHoraFooter(const FechaHora &fecha) {
  char horaTexto[6];
  snprintf(horaTexto, sizeof(horaTexto), "%02u:%02u",
           static_cast<unsigned int>(fecha.hora),
           static_cast<unsigned int>(fecha.minuto));
  escribirTextoFooter(horaTexto, 69, FOOTER_CONTENIDO_Y);
}

void dibujarNivelFooter(uint16_t edad, uint8_t progresoNivel) {
  char nivelTexto[14];
  snprintf(nivelTexto, sizeof(nivelTexto), "LVL %u",
           static_cast<unsigned int>(edad));

  pantalla.fillRect(112, FOOTER_Y, 93, FOOTER_ALTO,
                    FONDO_SECCION_FOOTER);
  escribirTextoFooter(nivelTexto, 114, FOOTER_CONTENIDO_Y);
  pantalla.drawRect(153, FOOTER_CONTENIDO_Y, 52, FOOTER_CONTENIDO_ALTO,
                    VERDE_ATENUADO);
  pantalla.fillRect(155, FOOTER_CONTENIDO_Y + 2,
                    (48 * progresoNivel) / 100, 4, VERDE_PIPBOY);
}

void dibujarBateriaFooter(uint8_t porcentaje) {
  char bateriaTexto[5];
  snprintf(bateriaTexto, sizeof(bateriaTexto), "%u%%",
           static_cast<unsigned int>(porcentaje));

  // La bateria se redibuja solo si cambia su lectura.
  pantalla.fillRect(230, FOOTER_Y, 74, FOOTER_ALTO,
                    FONDO_SECCION_FOOTER);
  escribirTextoFooter(bateriaTexto, 233, FOOTER_CONTENIDO_Y);
  dibujarIconoBateria(257, FOOTER_CONTENIDO_Y, porcentaje);
}

void guardarEstadoFooter(const FechaHora &fecha, uint16_t edad,
                         uint8_t progresoNivel, uint8_t bateria) {
  fechaFooterDibujada = fecha;
  edadFooterDibujada = edad;
  progresoFooterDibujado = progresoNivel;
  bateriaFooterDibujada = bateria;
}

void dibujarBarraInferior() {
  const FechaHora fecha = obtenerFechaHoraPrueba();
  const uint16_t edad = calcularEdad(fecha);
  const uint8_t progresoNivel = calcularProgresoNivel(fecha);
  const uint8_t bateria = PERFIL_PRUEBA.bateriaPorcentaje;
  const bool mostrarHora = pestanaActual != PESTANA_DATA;

  dibujarEstructuraFooter();
  dibujarFechaFooter(fecha);
  if (mostrarHora) dibujarHoraFooter(fecha);
  dibujarNivelFooter(edad, progresoNivel);
  dibujarBateriaFooter(bateria);
  seleccionarFuentePipBoy(TamanoTextoPipBoy::Normal);
  guardarEstadoFooter(fecha, edad, progresoNivel, bateria);
  footerInicializado = true;
  ultimaActualizacionFooterMs = millis();
}

void actualizarBarraInferior() {
  const uint32_t ahora = millis();
  if (ahora - ultimaActualizacionFooterMs < FOOTER_INTERVALO_ACTUALIZACION_MS) {
    return;
  }
  ultimaActualizacionFooterMs = ahora;
  if (!footerInicializado) {
    dibujarBarraInferior();
    return;
  }

  const FechaHora fecha = obtenerFechaHoraPrueba();
  const uint16_t edad = calcularEdad(fecha);
  const uint8_t progresoNivel = calcularProgresoNivel(fecha);
  const uint8_t bateria = PERFIL_PRUEBA.bateriaPorcentaje;
  const bool mostrarHora = pestanaActual != PESTANA_DATA;
  const bool fechaCambio = fecha.anio != fechaFooterDibujada.anio ||
                           fecha.mes != fechaFooterDibujada.mes ||
                           fecha.dia != fechaFooterDibujada.dia;
  const bool horaCambio = fecha.hora != fechaFooterDibujada.hora ||
                          fecha.minuto != fechaFooterDibujada.minuto;
  bool huboCambios = false;

  if (fechaCambio) {
    dibujarFechaFooter(fecha);
    huboCambios = true;
  }

  if (mostrarHora && horaCambio) {
    dibujarHoraFooter(fecha);
    huboCambios = true;
  }

  if (edad != edadFooterDibujada || progresoNivel != progresoFooterDibujado) {
    dibujarNivelFooter(edad, progresoNivel);
    huboCambios = true;
  }

  if (bateria != bateriaFooterDibujada) {
    dibujarBateriaFooter(bateria);
    huboCambios = true;
  }

  const bool estadoCambio = fechaCambio || horaCambio ||
                            edad != edadFooterDibujada ||
                            progresoNivel != progresoFooterDibujado ||
                            bateria != bateriaFooterDibujada;
  if (!estadoCambio) return;

  guardarEstadoFooter(fecha, edad, progresoNivel, bateria);
  if (huboCambios) seleccionarFuentePipBoy(TamanoTextoPipBoy::Normal);
}

void dibujarBarraVital(const char *etiqueta, int x, int porcentaje) {
  pantalla.setTextColor(VERDE_PIPBOY);
  escribirTextoPipBoy(etiqueta, x, 212, TamanoTextoPipBoy::Normal);
  pantalla.drawRect(x + 49, 201, 91, 11, VERDE_PIPBOY);
  pantalla.fillRect(x + 52, 204, (86 * porcentaje) / 100, 5,
                    VERDE_PIPBOY);
}

void dibujarHoraGrandeData(const FechaHora &fecha, bool limpiarAnterior) {
  char horaTexto[6];
  snprintf(horaTexto, sizeof(horaTexto), "%02u:%02u",
           static_cast<unsigned int>(fecha.hora),
           static_cast<unsigned int>(fecha.minuto));

  if (limpiarAnterior) {
    // Se limpia solo el area del reloj cuando cambia el minuto.
    pantalla.fillRect(55, 82, 210, 82, ST77XX_BLACK);
  }

  pantalla.setTextColor(VERDE_PIPBOY);
  seleccionarFuentePipBoy(TamanoTextoPipBoy::Principal);
  pantalla.setTextSize(DATA_RELOJ_ESCALA);
  int16_t x1, y1;
  uint16_t ancho, alto;
  pantalla.getTextBounds(horaTexto, 0, 0, &x1, &y1, &ancho, &alto);
  pantalla.setCursor(DATA_RELOJ_CENTRO_X - (ancho / 2),
                    DATA_RELOJ_LINEA_BASE);
  pantalla.print(horaTexto);
  seleccionarFuentePipBoy(TamanoTextoPipBoy::Normal);

  horaDataDibujada = fecha.hora;
  minutoDataDibujado = fecha.minuto;
  dosPuntosDataVisibles = true;
  ultimoParpadeoDataMs = millis();
}

void dibujarData() {
  const FechaHora fecha = obtenerFechaHoraPrueba();
  pantalla.setTextColor(VERDE_ATENUADO);
  escribirTextoPipBoyCentrado("CLOCK", 160, 58, TamanoTextoPipBoy::Normal);
  pantalla.drawFastHLine(38, 68, 244, VERDE_OSCURO);
  dibujarHoraGrandeData(fecha, false);
}

void actualizarRelojData() {
  if (pestanaActual != PESTANA_DATA) return;

  const FechaHora fecha = obtenerFechaHoraPrueba();
  if (fecha.hora != horaDataDibujada || fecha.minuto != minutoDataDibujado) {
    dibujarHoraGrandeData(fecha, true);
  }

  const uint32_t ahora = millis();
  if (ahora - ultimoParpadeoDataMs < DATA_RELOJ_PARPADEO_MS) return;

  // "HH:MM" ocupa 180 px con la fuente principal a escala 3. El separador
  // empieza tras los dos primeros digitos, por lo que se actualiza aislado.
  const int xDosPuntos = DATA_RELOJ_CENTRO_X - 90 + 72;
  pantalla.fillRect(xDosPuntos, 100, 36, 60, ST77XX_BLACK);
  dosPuntosDataVisibles = !dosPuntosDataVisibles;
  if (dosPuntosDataVisibles) {
    pantalla.setTextColor(VERDE_PIPBOY);
    seleccionarFuentePipBoy(TamanoTextoPipBoy::Principal);
    pantalla.setTextSize(DATA_RELOJ_ESCALA);
    pantalla.setCursor(xDosPuntos, DATA_RELOJ_LINEA_BASE);
    pantalla.print(':');
    seleccionarFuentePipBoy(TamanoTextoPipBoy::Normal);
  }
  ultimoParpadeoDataMs = ahora;
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
  if (pestanaActual == PESTANA_STAT) {
    dibujarStat();
    return;
  }

  if (pestanaActual == PESTANA_DATA) {
    dibujarData();
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

#if PIPBOY_BOTON_BOOT_HABILITADO
void revisarBotonBoot() {
  const uint32_t ahora = millis();
  const bool pulsado = digitalRead(BOTON_BOOT_GPIO) == LOW;

  if (pulsado != lecturaBotonBootAnterior) {
    lecturaBotonBootAnterior = pulsado;
    ultimoCambioBotonBootMs = ahora;
  }

  if (ahora - ultimoCambioBotonBootMs < BOTON_BOOT_REBOTE_MS ||
      pulsado == botonBootEstable) {
    return;
  }

  botonBootEstable = pulsado;
  if (pulsado) {
    inicioPulsacionBootMs = ahora;
    return;
  }

  if (ahora - inicioPulsacionBootMs >= BOTON_BOOT_PULSACION_LARGA_MS) {
    seleccionarPestanaAnterior();
  } else {
    seleccionarPestanaSiguiente();
  }
}
#endif

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
  inicioRelojPruebaMs = millis();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

#if PIPBOY_BOTON_BOOT_HABILITADO
  pinMode(BOTON_BOOT_GPIO, INPUT_PULLUP);
  const bool botonBootPulsado = digitalRead(BOTON_BOOT_GPIO) == LOW;
  lecturaBotonBootAnterior = botonBootPulsado;
  botonBootEstable = botonBootPulsado;
#endif

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
#if PIPBOY_BOTON_BOOT_HABILITADO
  revisarBotonBoot();
#endif
  actualizarAnimacionStat();
  actualizarRelojData();
  actualizarIconoTerminal();
  actualizarBarraInferior();
}
