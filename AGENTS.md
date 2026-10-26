# AGENTS.md — Contexto para Codex

## Proyecto
Construir un Pip-Boy funcional usando una ESP32-2432S028R con pantalla táctil TFT de 2.8".

## Principios
1. Mantener compatibilidad con ESP32-2432S028R.
2. La interfaz debe trabajar en orientación horizontal 320x240.
3. Priorizar bajo consumo de RAM y flash.
4. No añadir dependencias innecesarias.
5. Separar UI, hardware y red.
6. No bloquear el loop principal con delays largos.
7. Diseñar pensando en batería.
8. Mantener la interfaz legible en pantalla física de 2.8".
9. El aspecto debe ser retro-futurista y monocromo verde, sin sacrificar usabilidad.
10. Las funciones decorativas deben estar separadas de los datos reales.

# ESP32-2432S028R — Especificaciones confirmadas

## Placa

- Modelo: ESP32-2432S028R (CYD) con pantalla táctil TFT de 2.8 pulgadas.
- Revisión física: dos puertos USB (micro-USB y USB-C).
- Tipo de chip: ESP32-D0WD-V3 (revisión v3.1).
- Características: Wi-Fi, Bluetooth, doble núcleo + LP Core, 240 MHz,
  calibración Vref en eFuse y esquema de codificación None.
- Frecuencia del cristal: 40 MHz.
- MAC: `70:4B:CA:5D:33:30`.
- Memoria flash detectada: 4 MB.
- Controlador de pantalla: ST7789. No usar ILI9341.
- Resolución de interfaz probada: 320×240 horizontal.
- Rotación probada: `pantalla.setRotation(3)`.

## Carga y entorno probado

- Puerto serie probado: COM3.
- Placa seleccionada en Arduino IDE: `ESP32 Dev Module`.

## Pines de pantalla ST7789

- SCLK: GPIO 14
- MISO: GPIO 12
- MOSI: GPIO 13
- CS: GPIO 15
- DC: GPIO 2
- Luz de fondo: GPIO 21
- RESET: no conectado (`-1`)

## Pines de touch XPT2046

- SCLK: GPIO 25
- MISO: GPIO 39
- MOSI: GPIO 32
- CS: GPIO 33
- IRQ: GPIO 36

## Calibración táctil validada

- X mínimo: 478
- X máximo: 3515
- Y mínimo: 513
- Y máximo: 3209

Los pines y la calibración anteriores fueron probados físicamente. No inventar ni cambiar valores no confirmados sin volver a validar en la placa.

## Hardware confirmado y probado

Placa del usuario: ESP32-2432S028R (CYD) con **dos puertos USB**
(micro-USB y USB-C). Esta revisión usa controlador de pantalla **ST7789**;
no usar ILI9341 para la pantalla.

- Chip detectado: ESP32-D0WD-V3.
- Flash detectada: 4 MB.
- Puerto de carga probado: COM3.
- Perfil de Arduino IDE que funcionó: `ESP32 Dev Module`.
- Orientación de la interfaz: 320×240 horizontal.
- Rotación que funcionó en el sketch de prueba: `pantalla.setRotation(3)`.

### Sketch que debe usarse ahora

El sketch funcional de Arduino IDE está en:
`PipBoyBase_STAT/PipBoyBase_STAT.ino`.

No mover ni reemplazar el respaldo del usuario en
`C:\Users\PCreathors\Documents\Arduino\PipBoyBase` sin solicitarlo.

## Interfaz acordada
Top bar:
STAT | INV | DATA | MAP | RADIO

Bottom bar:
WIFI ● | temperatura | hora | BAT %

Pantalla STAT:
- personaje/mascota retro original
- HP 92
- AP 73
- estética CRT verde

## Regla visual permanente

La referencia visual es el Pip-Boy clásico proporcionado por el usuario:
fondo negro, líneas y texto verde monocromo, menú superior perfilado,
submenú `STATUS | SPECIAL | PERKS`, personaje central y telemetría inferior.

- Referencia visual guardada: `docs/DESIGN_SYSTEM.md`.
- GIF de referencia: `data/reference-pipboy.gif`.

### Hitos completados

- [x] ESP32 arranca y muestra el diagnóstico serie.
- [x] Pantalla ST7789 inicializada correctamente.
- [x] Pantalla en formato horizontal 320×240.
- [x] Touch XPT2046 leído y calibrado.
- [ ] Barra superior táctil con `STAT`, `INV`, `DATA`, `MAP` y `RADIO`.
- [ ] Barra inferior simulada con WiFi, 26 °C, hora y batería.
- [ ] Diseño experimental, GIF de referencia y fuente personalizada eliminados.

### Trabajo pendiente por pantalla

- [ ] Definir una dirección visual nueva para todas las pantallas.
- [ ] `STAT`: crear contenido cuando exista una dirección visual aprobada.
- [ ] `INV`: crear contenido cuando exista una dirección visual aprobada.
- [ ] `DATA`: crear contenido cuando exista una dirección visual aprobada.
- [ ] `MAP`: crear contenido cuando exista una dirección visual aprobada.
- [ ] `RADIO`: crear contenido cuando exista una dirección visual aprobada.
- [ ] Sustituir datos simulados por hardware, WiFi y Home Assistant después
  de terminar y validar la interfaz.

## Desarrollo

El usuario desarrolla y carga actualmente desde **Arduino IDE**; no exigir
Visual Studio Code ni PlatformIO para continuar las pruebas visuales.

Bibliotecas que ya se instalaron y probaron en Arduino IDE:

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- XPT2046_Touchscreen

Antes de modificar configuración de pines, verificar el pinout exacto de la revisión de hardware de la placa.
