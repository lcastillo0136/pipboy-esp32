# Pip-Boy ESP32

Proyecto de Pip-Boy funcional basado en la placa ESP32-2432S028R ("Cheap Yellow Display"), con pantalla táctil TFT de 2.8" y resolución lógica horizontal de 320x240.

## Objetivo

Construir un dispositivo portátil de muñeca inspirado en una interfaz retro-futurista tipo Pip-Boy, pero con funciones reales: estado del sistema, sensores, Wi-Fi, Home Assistant, brújula/GPS, inventario/listas, reloj, batería y control multimedia.

## Hardware base

- ESP32-2432S028R
- TFT 2.8" 240x320, usada horizontalmente a 320x240
- Touch resistivo XPT2046
- microSD
- Wi-Fi / Bluetooth
- Batería Li-ion/LiPo
- Módulo de carga/protección + salida regulada a 5 V
- Interruptor físico

## Hardware opcional futuro

- BME280: temperatura, humedad y presión
- QMC5883L: brújula
- GPS
- DS3231: RTC
- Encoder rotatorio
- Botones físicos
- Vibrador háptico
- Altavoz/amplificador

## Interfaz principal

STAT | INV | DATA | MAP | RADIO

### STAT
Estado general, batería, Wi-Fi, hora, temperatura/sensores y barras HP/AP.

### Footer de prueba
El pie de STAT muestra fecha, hora, un icono de batería con carga y `LEVEL`.
El nivel representa la edad y su barra avanza desde el día posterior al
cumpleaños hasta el siguiente cumpleaños. Para las pruebas actuales se usa el
perfil de nacimiento 1991-01-07, fecha inicial 2026-10-26 y batería al 87 %.
La hora avanza desde el arranque; todavía no es una fuente de hora real.

En una fase posterior, la configuración de perfil y reloj se guardará en la
microSD, después de confirmar su pinout, y la fecha/hora vendrá de RTC o NTP.

### INV
Inventario/listas personales almacenadas localmente.

### DATA
Hora, fecha, notas, estado del sistema y Home Assistant.

### MAP
Brújula, GPS opcional, coordenadas, rumbo, altitud y puntos guardados.

### RADIO
Control multimedia: play/pause, anterior/siguiente, volumen y control remoto por Wi-Fi.

## Estilo visual

- Resolución objetivo: 320x240
- Fondo casi negro con tono verde
- Monocromo verde fosforescente
- Tipografía monoespaciada/pixel
- Scanlines CRT sutiles
- Animaciones discretas de terminal
- Prioridad absoluta a legibilidad

## Stack propuesto

- PlatformIO
- Arduino Framework para ESP32
- LVGL 9
- TFT_eSPI o driver compatible para ILI9341
- XPT2046 para touch
- WiFi
- SD

## Fases

### v0.1
- Inicializar pantalla
- Inicializar touch
- Interfaz base 320x240
- Navegación STAT / INV / DATA / MAP / RADIO
- Footer con fecha, hora, batería simulada y nivel calculado

### v0.2
- Lectura real de batería
- Wi-Fi
- Hora por NTP
- Persistencia de perfil, fecha y hora en microSD

### v0.3
- Home Assistant
- BME280
- Controles físicos

### v0.4
- Brújula
- GPS
- MAP funcional

### v1.0
- Gestión de energía
- Suspensión
- Interfaz final
- Carcasa de muñeca
- Pruebas de autonomía
