# Servicio de mapa Pip-Boy

Endpoint PHP sin framework que consulta una imagen estática de Mapbox, la
convierte a un bitmap monocromo de 1 bit y la entrega en un formato que el
ESP32 puede dibujar sin decodificar PNG o JPEG.

## Requisitos del hosting

- PHP 7.4 o posterior.
- Extensiones `curl` y `gd` habilitadas.
- HTTPS válido en `pipboy.onix-software.com`.
- El document root del subdominio debe ser la carpeta `public/` de este
  proyecto, no la carpeta raíz `pipboy-map-api/`.

## Configuración

1. En Mapbox Studio crea un estilo oscuro minimalista y oculta las capas de
   etiquetas (`labels`). El endpoint no modifica el estilo: usa exactamente el
   que se configure.
2. Copia `config.example.php` como `config.php` y sustituye los valores
   `CHANGE_ME`. Ese archivo está ignorado por Git y debe permanecer fuera de
   `public/`.
3. Conserva la carpeta `cache/` con permisos de escritura para el proceso PHP.
   El endpoint crea los archivos de caché automáticamente.
4. Sube el contenido de esta carpeta al servidor y apunta el subdominio a
   `public/`.

No subas una clave de Mapbox al firmware ni la incluyas en un repositorio.

## Despliegue preparado

El repositorio incluye `tools/deploy-map-api.ps1`. Copia
`deploy.credentials.example.json` como `deploy.credentials.json`, agrega los
datos de acceso y ejecuta desde la raíz del repositorio:

```powershell
.\tools\deploy-map-api.ps1
```

El script también reconoce el formato existente `sftp-config.json` con los
campos `type`, `user` y `remote_path`; usa este archivo automáticamente si no
existe `deploy.credentials.json`. Ambos archivos de credenciales y `config.php`
están excluidos de Git. El script
sube solo `config.php`, `public/map.php` y `src/MapEndpoint.php`; nunca sube
la caché ni los ejemplos. Prefiere `sftp` o `ftps`; usa `ftp` únicamente si el
hosting no ofrece una alternativa cifrada.

## Solicitud del ESP32

```text
GET /map.php?lat=19.4326&lon=-99.1332&z=16&size=160x120
Host: pipboy.onix-software.com
X-Pipboy-Key: <device_api_key>
```

Parámetros:

- `lat`: latitud entre -90 y 90 (obligatorio).
- `lon`: longitud entre -180 y 180 (obligatorio).
- `z`: zoom entero permitido por `config.php`; por defecto 16.
- `size`: `160x120` (por defecto) o `304x184`.

Una solicitud correcta devuelve `200` y
`Content-Type: application/vnd.pipboy.map; version=1`. Los fallos devuelven
JSON mínimo con códigos `401`, `422`, `502` o `500`; no se revela la clave ni
la respuesta del proveedor.

## Formato `PBM1`

El cuerpo contiene 12 bytes de cabecera y los píxeles empaquetados. Cada bit
en `1` representa un píxel verde y cada bit en `0` uno negro.

| Desplazamiento | Longitud | Contenido |
| --- | ---: | --- |
| 0 | 4 | ASCII `PBM1` |
| 4 | 2 | ancho, `uint16` little-endian |
| 6 | 2 | alto, `uint16` little-endian |
| 8 | 2 | bytes por fila, `uint16` little-endian |
| 10 | 1 | formato `1`: 1 bit por píxel, MSB primero |
| 11 | 1 | reservado, `0` |
| 12 | variable | píxeles empaquetados por fila |

El tamaño de `160x120` ocupa 2,412 bytes en total (cabecera incluida). El de
`304x184` ocupa 7,004 bytes. El mapa se guarda en caché siete días, agrupando
coordenadas a cuatro decimales para reducir peticiones al proveedor.

## Atribución

El endpoint solicita la imagen sin logo ni texto superpuesto para no perder
legibilidad al convertirla a 1 bit. Por ello envía la atribución configurada
en la cabecera `X-Pipboy-Attribution`. El firmware debe mostrar la atribución
configurada siempre que visualice el mapa; revisa las fuentes finales del
estilo por si Mapbox exige texto adicional. La caché incorpora el propietario
y el ID del estilo: al publicar un estilo distinto se crea automáticamente un
bitmap nuevo, sin reutilizar el generado por el estilo anterior.
La conversión usa el contraste de cada imagen para conservar líneas
antialiasadas del estilo publicado; una imagen sin contraste se mantiene negra
en vez de llenar la pantalla de ruido.

Mapbox puede conservar una versión de un estilo en la CDN de Static Images
hasta 12 horas después de publicarlo. Si necesitas que una revisión visual
aparezca de inmediato, duplica el estilo en Mapbox Studio, publícalo y cambia
en `config.php` su `mapbox_style_id`: el ID nuevo no reutiliza esa caché.

Mapbox documenta que, incluso al desactivar la atribución incrustada, la
atribución correcta de los datos (incluido OpenStreetMap cuando corresponda)
sigue siendo obligatoria. Consulta la [documentación de Static Images API](https://docs.mapbox.com/api/maps/static-images/) y la [guía de atribución](https://docs.mapbox.com/help/glossary/attribution/).
