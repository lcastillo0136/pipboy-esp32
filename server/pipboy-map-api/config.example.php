<?php

/**
 * Copy this file to config.php and replace every CHANGE_ME value.
 *
 * Keep config.php one directory above public/. The subdomain document root must
 * point to public/ so neither this file nor the cache can be requested over HTTP.
 */
return array(
    // A Mapbox public token with the styles:tiles scope. It remains server-side.
    'mapbox_access_token' => 'CHANGE_ME',

    // The owner and ID of the label-free style created in Mapbox Studio.
    // Example URL: mapbox://styles/onix-software/clxxxxxxxxxxxxxx
    'mapbox_style_owner' => 'CHANGE_ME',
    'mapbox_style_id' => 'CHANGE_ME',

    // Sent by the ESP32 in the X-Pipboy-Key request header.
    // Generate a long random value; do not reuse the Mapbox token here.
    'device_api_key' => 'CHANGE_ME',

    // Required visual attribution for this Mapbox/OpenStreetMap-based style.
    // The firmware must display this text whenever it renders a map response.
    // Adjust it if the selected Mapbox style requires additional attribution.
    'attribution' => "\xC2\xA9 Mapbox \xC2\xA9 OpenStreetMap",

    // Only these dimensions can be requested. This prevents the endpoint from
    // becoming a general-purpose image proxy and bounds ESP32 RAM consumption.
    'allowed_sizes' => array(
        '160x120' => array(160, 120),
        '304x184' => array(304, 184),
    ),
    'default_size' => '160x120',
    'default_zoom' => 16,
    'minimum_zoom' => 9,
    'maximum_zoom' => 18,

    // A map normally changes infrequently. GPS values are rounded to four
    // decimals before caching (about 11 m at the equator).
    'cache_directory' => __DIR__ . DIRECTORY_SEPARATOR . 'cache',
    'cache_ttl_seconds' => 604800,
    'upstream_timeout_seconds' => 10,
);
