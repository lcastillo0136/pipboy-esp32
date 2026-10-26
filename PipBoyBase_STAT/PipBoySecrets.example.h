#pragma once

// Copia este archivo como PipBoySecrets.h y completa los valores. El archivo
// real esta excluido de Git para no publicar la red local ni la clave del
// dispositivo.
#define PIPBOY_WIFI_SSID "CAMBIAR_POR_TU_WIFI"
#define PIPBOY_WIFI_PASSWORD "CAMBIAR_POR_TU_CONTRASENA"

// Debe coincidir exactamente con device_api_key en
// server/pipboy-map-api/config.php. No es el token publico de Mapbox.
#define PIPBOY_MAP_API_KEY "CAMBIAR_POR_DEVICE_API_KEY"

#define PIPBOY_MAP_API_URL "https://pipboy.onix-software.com/map.php"

// Mientras no haya GPS, estas coordenadas de prueba son explicitamente
// separadas de los datos reales. Sustituyelas despues desde el modulo GPS.
#define PIPBOY_MAP_LATITUD_PRUEBA 19.432600f
#define PIPBOY_MAP_LONGITUD_PRUEBA -99.133200f
#define PIPBOY_MAP_ZOOM_PRUEBA 15
