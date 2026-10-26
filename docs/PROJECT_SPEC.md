# Especificación funcional

## Objetivo de la primera versión

La primera versión debe sentirse como un Pip-Boy desde el encendido, pero ser sencilla de depurar y suficientemente ligera para el ESP32.

### Arranque
Secuencia visual corta estilo terminal y entrada directa a STAT.

### Navegación
Cinco secciones permanentes: STAT, INV, DATA, MAP y RADIO.

La pantalla táctil será la navegación principal durante el prototipo. Más adelante se añadirá encoder y botones físicos.

### Estado inferior permanente
Mostrar Wi-Fi, temperatura, reloj y batería.

### Filosofía de implementación
Cada pantalla debe ser independiente. No recrear toda la UI en cada frame. Usar eventos de LVGL. Evitar animaciones pesadas.
