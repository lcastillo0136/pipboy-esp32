# PipBoyBase_STAT

Sketch para Arduino IDE de la pantalla `STAT` de la ESP32-2432S028R.

## Navegacion

El touch esta desactivado de forma predeterminada para que la navegacion se
controle con botones o encoder externos. Los futuros controladores deben usar
`seleccionarPestanaSiguiente()`, `seleccionarPestanaAnterior()` o
`seleccionarPestana(indice)`.

Para habilitar el XPT2046, cambia al inicio de `PipBoyBase_STAT.ino`:

```cpp
#define PIPBOY_TOUCH_HABILITADO 1
```

### Botón BOOT

La navegación por el botón físico BOOT está habilitada de forma predeterminada
en `GPIO0` y no bloquea la animación ni el refresco de pantalla:

- Pulsación corta: pestaña siguiente.
- Pulsación de 700 ms o más: pestaña anterior.

No mantengas BOOT presionado durante un reinicio o al encender la placa: GPIO0
selecciona el modo de carga de firmware en ese momento. Para deshabilitar este
control, define `PIPBOY_BOTON_BOOT_HABILITADO` como `0` antes de compilar.

## Dependencia adicional

Instala **AnimatedGIF** de Larry Bank desde el gestor de bibliotecas de
Arduino. El GIF se decodifica por lineas directamente sobre el ST7789, sin un
framebuffer completo en RAM.

## Recurso animado

`reference_pipboy_gif.h` es el GIF 76x145 embebido en la flash del programa.
No se edita manualmente. Si se sustituye
`../data/reference-pipboy.gif`, se regenera desde la raiz del proyecto con:

```powershell
.\tools\convert_gif_to_header.ps1 `
  -InputGif .\data\reference-pipboy.gif `
  -OutputHeader .\PipBoyBase_STAT\reference_pipboy_gif.h `
  -Symbol referencePipboyGif
```

La animacion esta en `X=122`, `Y=50`; ocupa 76x145 px y deja libre la zona
inferior para las barras HP/AP y el estado permanente.

## Animacion de arranque

Al iniciar, una consola verde escribe el registro de carga carácter a carácter.
Después de una pausa de 800 ms tras `PLEASE STAND BY...`,
`reference-pipboy-thumbsup.gif` se muestra centrado una sola vez (15 cuadros,
aproximadamente 1.5 s) y después se entra directamente en `STAT`. El GIF se
embebe en flash como `reference_pipboy_thumbsup_gif.h` y se regenera con el
mismo script, usando el simbolo `referencePipboyThumbsupGif`.
