# Estilo minimalista de Mapbox

`pipboy-minimal-style.json` es un estilo Mapbox GL v8 creado desde cero para
la API de imágenes estáticas del Pip-Boy. No importa Mapbox Standard y no usa
estilos Classic: solo incluye una fuente vectorial de Mapbox Streets v8 y las
capas necesarias para un minimapa sin texto.

## Contenido visual

- Fondo negro con verdes muy oscuros.
- Agua y uso de suelo discretos.
- Límites administrativos punteados.
- Carreteras secundarias verdes y carreteras principales verde fosforescente.
- Vías de tren discontinuas.
- Sin capas `symbol`: no hay nombres de calles, ciudades, puntos de interés,
  iconos ni marcadores.

La cruz de la posición no forma parte del estilo. El ESP32 la dibujará encima
del mapa recibido para mantenerla nítida y ahorrar peticiones al proveedor.

## Cargarlo en Mapbox Studio

1. Descarga o localiza `pipboy-minimal-style.json` en este proyecto.
2. En Mapbox Studio abre la sección **Styles** y usa la opción de cargar un
   estilo (`Upload style` / `Upload style.json`). Si la interfaz no muestra
   esa opción, abre el menú de creación de estilo; no crees el estilo Standard
   predeterminado.
3. Selecciona este archivo, verifica en la vista previa que solo hay líneas y
   polígonos verdes, y pulsa **Publish**.
4. Copia la URL publicada, con el formato:

   ```text
   mapbox://styles/TU_USUARIO/ID_DEL_ESTILO
   ```

5. Usa las dos partes finales en `../config.php`:

   ```php
   'mapbox_style_owner' => 'TU_USUARIO',
   'mapbox_style_id' => 'ID_DEL_ESTILO',
   ```

## Antes de desplegar

La compatibilidad se verificará con una solicitud real de 160×120 antes de
subir el endpoint al hosting. No coloques tokens en este archivo ni los envíes
por el chat.

Las capas y clases usadas proceden de Mapbox Streets v8. Consulta la
[referencia de capas](https://docs.mapbox.com/data/tilesets/reference/mapbox-streets-v8/) y la
[API de imágenes estáticas](https://docs.mapbox.com/api/maps/static-images/).
