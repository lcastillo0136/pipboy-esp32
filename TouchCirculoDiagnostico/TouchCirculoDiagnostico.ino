#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <XPT2046_Touchscreen.h>

// Pines confirmados para la ESP32-2432S028R con ST7789.
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
constexpr int UMBRAL_PRESION = 400;
constexpr uint32_t INTERVALO_LECTURA_MS = 20;
constexpr int RADIO_CIRCULO = 12;

SPIClass pantallaSPI(HSPI);
SPIClass touchSPI(VSPI);
Adafruit_ST7789 pantalla(&pantallaSPI, TFT_CS, TFT_DC, -1);

// No se usa IRQ: la diagnostica previa demostro que GPIO 36 no baja al tocar.
XPT2046_Touchscreen touch(TOUCH_CS);

bool hayCirculo = false;
int ultimoX = 0;
int ultimoY = 0;
uint32_t proximaLectura = 0;

void dibujarCabecera() {
  pantalla.fillScreen(ST77XX_BLACK);
  pantalla.setTextColor(ST77XX_GREEN);
  pantalla.setTextSize(2);
  pantalla.setCursor(12, 10);
  pantalla.println("TOUCH: CIRCULO");
  pantalla.drawFastHLine(0, 34, 320, ST77XX_GREEN);
  pantalla.setTextSize(1);
  pantalla.setCursor(12, 48);
  pantalla.println("El circulo muestra la presion detectada");
}

int convertirX(int xRaw) {
  return map(constrain(xRaw, TOUCH_X_MIN, TOUCH_X_MAX),
             TOUCH_X_MIN, TOUCH_X_MAX, 0, 319);
}

int convertirY(int yRaw) {
  return map(constrain(yRaw, TOUCH_Y_MIN, TOUCH_Y_MAX),
             TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 239);
}

void borrarCirculoAnterior() {
  if (!hayCirculo) return;

  pantalla.drawCircle(ultimoX, ultimoY, RADIO_CIRCULO, ST77XX_BLACK);
  pantalla.drawFastHLine(ultimoX - 7, ultimoY, 15, ST77XX_BLACK);
  pantalla.drawFastVLine(ultimoX, ultimoY - 7, 15, ST77XX_BLACK);
  hayCirculo = false;
}

void mostrarLectura(int xRaw, int yRaw, int z, bool hayPresion) {
  pantalla.fillRect(0, 64, 320, 20, ST77XX_BLACK);
  pantalla.setCursor(12, 70);
  pantalla.setTextColor(hayPresion ? ST77XX_GREEN : 0x7BEF);
  pantalla.print("X:");
  pantalla.print(xRaw);
  pantalla.print(" Y:");
  pantalla.print(yRaw);
  pantalla.print(" Z:");
  pantalla.print(z);
}

void mostrarCirculo(int x, int y) {
  borrarCirculoAnterior();
  pantalla.drawCircle(x, y, RADIO_CIRCULO, ST77XX_GREEN);
  pantalla.drawFastHLine(x - 7, y, 15, ST77XX_GREEN);
  pantalla.drawFastVLine(x, y - 7, 15, ST77XX_GREEN);
  ultimoX = x;
  ultimoY = y;
  hayCirculo = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pantallaSPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  pantalla.init(240, 320);
  pantalla.setRotation(3);

  touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  pinMode(TOUCH_IRQ, INPUT);

  dibujarCabecera();
  Serial.println("=== TOUCH CIRCULO DIAGNOSTICO ===");
}

void loop() {
  const uint32_t ahora = millis();
  if (static_cast<int32_t>(ahora - proximaLectura) < 0) return;
  proximaLectura = ahora + INTERVALO_LECTURA_MS;

  const TS_Point punto = touch.getPoint();
  const bool hayPresion = punto.z >= UMBRAL_PRESION;
  mostrarLectura(punto.x, punto.y, punto.z, hayPresion);

  if (!hayPresion) {
    borrarCirculoAnterior();
    return;
  }

  const int x = convertirX(punto.x);
  const int y = convertirY(punto.y);
  mostrarCirculo(x, y);

  Serial.printf("X=%d Y=%d Z=%d -> pantalla=%d,%d IRQ=%d\n",
                punto.x, punto.y, punto.z, x, y, digitalRead(TOUCH_IRQ));
}
