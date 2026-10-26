# Fuente oficial de la interfaz

La tipografía de todos los textos del Pip-Boy es **Monofonto Regular**,
guardada como `source/pipboy-font.otf`. Esta copia es la fuente de verdad y
debe conservarse cuando se mueva o respalde el sketch.

El firmware usa las conversiones compactas para Adafruit GFX:

- `PipBoyFont16px7b.h`: etiquetas, pestañas, barra inferior y texto normal.
- `PipBoyFont24px7b.h`: títulos de las pantallas.

Los encabezados son fuentes de mapa de bits en `PROGMEM`; no hay que intentar
cargar el archivo OTF en la ESP32. Para volver a generarlos tras sustituir la
fuente original, instala Pillow localmente y ejecuta desde la raíz del
proyecto:

```powershell
python -m pip install --target tools/.font-tools Pillow
$env:PYTHONPATH = "tools/.font-tools"
python tools/convert_adafruit_font.py PipBoyBase_STAT/fonts/source/pipboy-font.otf PipBoyBase_STAT/fonts/PipBoyFont16px7b.h PipBoyFont16px7b 16
python tools/convert_adafruit_font.py PipBoyBase_STAT/fonts/source/pipboy-font.otf PipBoyBase_STAT/fonts/PipBoyFont24px7b.h PipBoyFont24px7b 24
```

En el sketch, use siempre `escribirTextoPipBoy()` o
`escribirTextoPipBoyCentrado()` en lugar de llamar a `pantalla.print()` para
texto de interfaz. Así todas las pantallas mantienen la misma tipografía.
