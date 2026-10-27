<?php

declare(strict_types=1);

/**
 * Static, one-bit map endpoint for the Pip-Boy ESP32.
 *
 * The response format is intentionally decoder-free on the device:
 *
 *   Offset  Length  Value
 *   0       4       ASCII "PBM1"
 *   4       2       width, unsigned little-endian
 *   6       2       height, unsigned little-endian
 *   8       2       bytes per row, unsigned little-endian
 *   10      1       pixel format: 1 = packed 1-bit, MSB first
 *   11      1       reserved (0)
 *   12      N       pixels, 1 = foreground green, 0 = black
 */
final class PipboyMapEndpoint
{
    private const HEADER_LENGTH = 12;
    private const MAX_UPSTREAM_BYTES = 2097152;
    // Cambiar esta version fuerza un bitmap nuevo si evoluciona el algoritmo
    // de conversión, sin reutilizar una caché de otra representación visual.
    private const BITMAP_CONVERSION_VERSION = '4';

    public static function run(string $configPath): void
    {
        try {
            $config = self::loadConfig($configPath);
            self::requireGetMethod();
            self::authenticate($config);

            $request = self::parseRequest($config);
            $cacheFile = self::cachePath($config, $request);
            $payload = self::readFreshCache($cacheFile, $config);

            if ($payload === null) {
                $image = self::fetchMapboxImage($config, $request);
                self::saveSourceImage($image, $request);
                $payload = self::convertToPackedBitmap(
                    $image,
                    $request['width'],
                    $request['height']
                );
                self::saveProcessedImage($payload, $request);

                // Los nodos CDN de Mapbox pueden servir temporalmente la
                // versión previa de un estilo. Si esa imagen no contiene
                // trazos, no se conserva: la siguiente consulta podrá
                // recuperar el mapa en cuanto la propagación concluya.
                if (self::hasForegroundPixels($payload)) {
                    self::writeCache($cacheFile, $payload);
                }
            }

            self::sendMap($payload, $config['attribution']);
        } catch (PipboyMapHttpException $exception) {
            self::sendError($exception->getStatusCode(), $exception->getPublicMessage());
        } catch (Throwable $exception) {
            error_log('PipBoy map endpoint failure: ' . $exception->getMessage());
            self::sendError(500, 'internal_error');
        }
    }

    private static function loadConfig(string $configPath): array
    {
        if (!is_file($configPath)) {
            throw new PipboyMapHttpException(500, 'server_not_configured');
        }

        $config = require $configPath;
        if (!is_array($config)) {
            throw new PipboyMapHttpException(500, 'server_not_configured');
        }

        $requiredKeys = array(
            'mapbox_access_token',
            'mapbox_style_owner',
            'mapbox_style_id',
            'device_api_key',
            'attribution',
            'allowed_sizes',
            'default_size',
            'default_zoom',
            'minimum_zoom',
            'maximum_zoom',
            'cache_directory',
            'cache_ttl_seconds',
            'upstream_timeout_seconds',
        );

        foreach ($requiredKeys as $key) {
            if (!array_key_exists($key, $config)) {
                throw new PipboyMapHttpException(500, 'server_not_configured');
            }
        }

        foreach (array('mapbox_access_token', 'mapbox_style_owner', 'mapbox_style_id', 'device_api_key') as $key) {
            if (!is_string($config[$key]) || $config[$key] === '' || $config[$key] === 'CHANGE_ME') {
                throw new PipboyMapHttpException(500, 'server_not_configured');
            }
        }

        if (!is_array($config['allowed_sizes']) || !isset($config['allowed_sizes'][$config['default_size']])) {
            throw new PipboyMapHttpException(500, 'server_not_configured');
        }

        if (!function_exists('curl_init') || !function_exists('imagecreatefromstring') ||
            !function_exists('imagecreatetruecolor') || !function_exists('imagepng')) {
            throw new PipboyMapHttpException(500, 'server_missing_extensions');
        }

        return $config;
    }

    private static function requireGetMethod(): void
    {
        $method = isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET';
        if ($method !== 'GET') {
            header('Allow: GET');
            throw new PipboyMapHttpException(405, 'method_not_allowed');
        }
    }

    private static function authenticate(array $config): void
    {
        $providedKey = isset($_SERVER['HTTP_X_PIPBOY_KEY']) ? (string) $_SERVER['HTTP_X_PIPBOY_KEY'] : '';
        if ($providedKey === '' || !hash_equals((string) $config['device_api_key'], $providedKey)) {
            throw new PipboyMapHttpException(401, 'unauthorized');
        }
    }

    private static function parseRequest(array $config): array
    {
        $latitude = self::requiredCoordinate('lat', -90.0, 90.0);
        $longitude = self::requiredCoordinate('lon', -180.0, 180.0);
        $zoom = self::optionalInteger('z', (int) $config['default_zoom']);

        if ($zoom < (int) $config['minimum_zoom'] || $zoom > (int) $config['maximum_zoom']) {
            throw new PipboyMapHttpException(422, 'invalid_zoom');
        }

        $sizeName = isset($_GET['size']) ? (string) $_GET['size'] : (string) $config['default_size'];
        if (!isset($config['allowed_sizes'][$sizeName])) {
            throw new PipboyMapHttpException(422, 'invalid_size');
        }

        $size = $config['allowed_sizes'][$sizeName];
        if (!is_array($size) || count($size) !== 2 || (int) $size[0] < 1 || (int) $size[1] < 1) {
            throw new PipboyMapHttpException(500, 'server_not_configured');
        }

        return array(
            'latitude' => $latitude,
            'longitude' => $longitude,
            'zoom' => $zoom,
            'width' => (int) $size[0],
            'height' => (int) $size[1],
            'size_name' => $sizeName,
        );
    }

    private static function requiredCoordinate(string $name, float $minimum, float $maximum): float
    {
        if (!isset($_GET[$name]) || !is_string($_GET[$name])) {
            throw new PipboyMapHttpException(422, 'invalid_coordinates');
        }

        $value = trim($_GET[$name]);
        if (!preg_match('/^-?(?:\d+|\d*\.\d+)$/', $value)) {
            throw new PipboyMapHttpException(422, 'invalid_coordinates');
        }

        $number = (float) $value;
        if (!is_finite($number) || $number < $minimum || $number > $maximum) {
            throw new PipboyMapHttpException(422, 'invalid_coordinates');
        }

        return $number;
    }

    private static function optionalInteger(string $name, int $default): int
    {
        if (!isset($_GET[$name])) {
            return $default;
        }

        $value = (string) $_GET[$name];
        if (!preg_match('/^\d{1,2}$/', $value)) {
            throw new PipboyMapHttpException(422, 'invalid_zoom');
        }

        return (int) $value;
    }

    private static function cachePath(array $config, array $request): string
    {
        $cacheDirectory = (string) $config['cache_directory'];
        if (!is_dir($cacheDirectory) && !mkdir($cacheDirectory, 0750, true) && !is_dir($cacheDirectory)) {
            throw new PipboyMapHttpException(500, 'cache_unavailable');
        }

        $cacheKey = implode('|', array(
            self::BITMAP_CONVERSION_VERSION,
            // El estilo forma parte del resultado. Incluirlo invalida
            // automaticamente los bitmaps creados antes de publicar o
            // sustituir el estilo de Mapbox.
            (string) $config['mapbox_style_owner'],
            (string) $config['mapbox_style_id'],
            number_format(round($request['latitude'], 4), 4, '.', ''),
            number_format(round($request['longitude'], 4), 4, '.', ''),
            $request['zoom'],
            $request['size_name'],
        ));

        return rtrim($cacheDirectory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . hash('sha256', $cacheKey) . '.pbm1';
    }

    private static function readFreshCache(string $cacheFile, array $config): ?string
    {
        if (!is_file($cacheFile)) {
            return null;
        }

        $age = time() - (int) filemtime($cacheFile);
        if ($age > (int) $config['cache_ttl_seconds']) {
            return null;
        }

        $payload = file_get_contents($cacheFile);
        if ($payload === false || strlen($payload) < self::HEADER_LENGTH || substr($payload, 0, 4) !== 'PBM1') {
            return null;
        }

        return self::hasForegroundPixels($payload) ? $payload : null;
    }

    private static function fetchMapboxImage(array $config, array $request): string
    {
        self::validateMapboxStylePart((string) $config['mapbox_style_owner']);
        self::validateMapboxStylePart((string) $config['mapbox_style_id']);

        $url = sprintf(
            'https://api.mapbox.com/styles/v1/%s/%s/static/%s,%s,%d/%dx%d.png?attribution=false&logo=false&access_token=%s',
            rawurlencode((string) $config['mapbox_style_owner']),
            rawurlencode((string) $config['mapbox_style_id']),
            number_format($request['longitude'], 6, '.', ''),
            number_format($request['latitude'], 6, '.', ''),
            $request['zoom'],
            $request['width'],
            $request['height'],
            rawurlencode((string) $config['mapbox_access_token'])
        );

        $responseBody = '';
        $curl = curl_init($url);
        if ($curl === false) {
            throw new PipboyMapHttpException(502, 'map_provider_unavailable');
        }

        curl_setopt_array($curl, array(
            CURLOPT_RETURNTRANSFER => false,
            CURLOPT_FOLLOWLOCATION => false,
            CURLOPT_CONNECTTIMEOUT => (int) $config['upstream_timeout_seconds'],
            CURLOPT_TIMEOUT => (int) $config['upstream_timeout_seconds'],
            CURLOPT_USERAGENT => 'PipBoyMapProxy/1.0',
            CURLOPT_HTTPHEADER => array('Accept: image/png,image/jpeg;q=0.9'),
            CURLOPT_SSL_VERIFYPEER => true,
            CURLOPT_SSL_VERIFYHOST => 2,
            CURLOPT_WRITEFUNCTION => static function ($handle, string $chunk) use (&$responseBody): int {
                if (strlen($responseBody) + strlen($chunk) > self::MAX_UPSTREAM_BYTES) {
                    return 0;
                }

                $responseBody .= $chunk;
                return strlen($chunk);
            },
        ));

        $result = curl_exec($curl);
        $statusCode = (int) curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
        $contentType = (string) curl_getinfo($curl, CURLINFO_CONTENT_TYPE);
        curl_close($curl);

        if ($result === false || $statusCode < 200 || $statusCode >= 300 || stripos($contentType, 'image/') !== 0) {
            throw new PipboyMapHttpException(502, 'map_provider_unavailable');
        }

        return $responseBody;
    }

    private static function validateMapboxStylePart(string $value): void
    {
        if (!preg_match('/^[A-Za-z0-9_-]{1,128}$/', $value)) {
            throw new PipboyMapHttpException(500, 'server_not_configured');
        }
    }

    private static function saveSourceImage(string $image, array $request): void
    {
        // El nombre se compone solo de valores numéricos ya validados. Cada
        // ubicación conserva la última imagen PNG descargada de Mapbox.
        $filename = sprintf(
            'map_lat%s_lon%s_z%d_%dx%d.png',
            self::coordinateFilename((float) $request['latitude']),
            self::coordinateFilename((float) $request['longitude']),
            (int) $request['zoom'],
            (int) $request['width'],
            (int) $request['height']
        );
        $imagePath = self::imageDirectory() . DIRECTORY_SEPARATOR . $filename;
        $temporaryPath = $imagePath . '.' . bin2hex(random_bytes(6)) . '.tmp';

        if (file_put_contents($temporaryPath, $image, LOCK_EX) === false || !rename($temporaryPath, $imagePath)) {
            @unlink($temporaryPath);
            throw new PipboyMapHttpException(500, 'image_storage_unavailable');
        }
    }

    private static function saveProcessedImage(string $payload, array $request): void
    {
        $width = (int) $request['width'];
        $height = (int) $request['height'];
        $bytesPerRow = (int) ceil($width / 8);
        $expectedLength = self::HEADER_LENGTH + ($bytesPerRow * $height);

        if (strlen($payload) !== $expectedLength || substr($payload, 0, 4) !== 'PBM1') {
            throw new PipboyMapHttpException(500, 'bitmap_conversion_failed');
        }

        $image = imagecreatetruecolor($width, $height);
        if ($image === false) {
            throw new PipboyMapHttpException(500, 'image_storage_unavailable');
        }

        $black = imagecolorallocate($image, 0, 0, 0);
        $green = imagecolorallocate($image, 0, 255, 70);
        imagefill($image, 0, 0, $black);

        // Reconstruye el PNG desde el mismo PBM1 enviado a la placa. Es una
        // captura fiel de la conversión, no una segunda interpretación del
        // PNG de Mapbox ni del umbral de luminancia.
        for ($y = 0; $y < $height; $y++) {
            for ($x = 0; $x < $width; $x++) {
                $offset = self::HEADER_LENGTH + ($y * $bytesPerRow) + intdiv($x, 8);
                if ((ord($payload[$offset]) & (0x80 >> ($x % 8))) !== 0) {
                    imagesetpixel($image, $x, $y, $green);
                }
            }
        }

        $filename = sprintf(
            'map_lat%s_lon%s_z%d_%dx%d_processed.png',
            self::coordinateFilename((float) $request['latitude']),
            self::coordinateFilename((float) $request['longitude']),
            (int) $request['zoom'],
            $width,
            $height
        );
        $imagePath = self::imageDirectory() . DIRECTORY_SEPARATOR . $filename;
        $temporaryPath = $imagePath . '.' . bin2hex(random_bytes(6)) . '.tmp';
        $saved = imagepng($image, $temporaryPath, 9);
        imagedestroy($image);

        if (!$saved || !rename($temporaryPath, $imagePath)) {
            @unlink($temporaryPath);
            throw new PipboyMapHttpException(500, 'image_storage_unavailable');
        }
    }

    private static function imageDirectory(): string
    {
        $directory = dirname(__DIR__) . DIRECTORY_SEPARATOR . 'public' . DIRECTORY_SEPARATOR . 'images';
        if (!is_dir($directory) && !mkdir($directory, 0755, true) && !is_dir($directory)) {
            throw new PipboyMapHttpException(500, 'image_storage_unavailable');
        }

        return $directory;
    }

    private static function coordinateFilename(float $coordinate): string
    {
        return str_replace(
            array('-', '.'),
            array('m', '_'),
            number_format($coordinate, 6, '.', '')
        );
    }

    private static function convertToPackedBitmap(string $sourceImage, int $width, int $height): string
    {
        $image = @imagecreatefromstring($sourceImage);
        if ($image === false) {
            throw new PipboyMapHttpException(502, 'invalid_map_image');
        }

        if (imagesx($image) !== $width || imagesy($image) !== $height) {
            imagedestroy($image);
            throw new PipboyMapHttpException(502, 'invalid_map_image');
        }

        // Mapbox puede responder un PNG indexado. En GD, imagecolorat()
        // devuelve en ese caso el índice de la paleta, no un color RGB; sin
        // convertirlo, los caminos se interpretan como negros y desaparecen
        // al empaquetar el bitmap de 1 bit.
        if (!imageistruecolor($image) && !imagepalettetotruecolor($image)) {
            imagedestroy($image);
            throw new PipboyMapHttpException(502, 'invalid_map_image');
        }

        $bytesPerRow = (int) ceil($width / 8);
        $pixels = '';
        $luminanciaMinima = 255.0;
        $luminanciaMaxima = 0.0;

        // Medimos el rango de luminosidad para distinguir una imagen vacía
        // de una que sí contiene detalle del estilo de Mapbox.
        for ($y = 0; $y < $height; $y++) {
            for ($x = 0; $x < $width; $x++) {
                $rgba = imagecolorat($image, $x, $y);
                $red = ($rgba >> 16) & 0xFF;
                $green = ($rgba >> 8) & 0xFF;
                $blue = $rgba & 0xFF;
                $luminancia = (0.2126 * $red) + (0.7152 * $green) + (0.0722 * $blue);
                $luminanciaMinima = min($luminanciaMinima, $luminancia);
                $luminanciaMaxima = max($luminanciaMaxima, $luminancia);
            }
        }

        $rangoLuminancia = $luminanciaMaxima - $luminanciaMinima;
        // Una imagen de fondo uniforme permanece vacía. En el estilo actual,
        // 40 conserva también las vialidades secundarias sin encender el
        // fondo negro al convertir a un solo bit por píxel.
        $umbral = $rangoLuminancia < 12.0
            ? PHP_FLOAT_MAX
            : 40.0;

        for ($y = 0; $y < $height; $y++) {
            $row = '';
            $packedByte = 0;

            for ($x = 0; $x < $width; $x++) {
                $rgba = imagecolorat($image, $x, $y);
                $red = ($rgba >> 16) & 0xFF;
                $green = ($rgba >> 8) & 0xFF;
                $blue = $rgba & 0xFF;
                $alpha = ($rgba >> 24) & 0x7F;
                $luminance = (0.2126 * $red) + (0.7152 * $green) + (0.0722 * $blue);

                // El estilo es oscuro: se preservan las vías y detalles con
                // contraste alto como píxeles de fósforo verde.
                if ($alpha < 110 && $luminance >= $umbral) {
                    $packedByte |= 1 << (7 - ($x % 8));
                }

                if (($x % 8) === 7) {
                    $row .= chr($packedByte);
                    $packedByte = 0;
                }
            }

            if (($width % 8) !== 0) {
                $row .= chr($packedByte);
            }

            if (strlen($row) !== $bytesPerRow) {
                imagedestroy($image);
                throw new PipboyMapHttpException(500, 'bitmap_conversion_failed');
            }

            $pixels .= $row;
        }

        imagedestroy($image);

        return pack('a4vvvCC', 'PBM1', $width, $height, $bytesPerRow, 1, 0) . $pixels;
    }

    private static function hasForegroundPixels(string $payload): bool
    {
        if (strlen($payload) <= self::HEADER_LENGTH) {
            return false;
        }

        for ($index = self::HEADER_LENGTH, $length = strlen($payload); $index < $length; $index++) {
            if (ord($payload[$index]) !== 0) {
                return true;
            }
        }

        return false;
    }

    private static function writeCache(string $cacheFile, string $payload): void
    {
        $temporaryFile = $cacheFile . '.' . bin2hex(random_bytes(6)) . '.tmp';
        if (file_put_contents($temporaryFile, $payload, LOCK_EX) === false || !rename($temporaryFile, $cacheFile)) {
            @unlink($temporaryFile);
            throw new PipboyMapHttpException(500, 'cache_unavailable');
        }
    }

    private static function sendMap(string $payload, string $attribution): void
    {
        $safeAttribution = str_replace(array("\r", "\n"), '', $attribution);

        http_response_code(200);
        header('Content-Type: application/vnd.pipboy.map; version=1');
        header('Content-Length: ' . strlen($payload));
        header('Cache-Control: no-store');
        header('X-Content-Type-Options: nosniff');
        header('X-Pipboy-Attribution: ' . $safeAttribution);
        echo $payload;
        exit;
    }

    private static function sendError(int $statusCode, string $error): void
    {
        http_response_code($statusCode);
        header('Content-Type: application/json; charset=utf-8');
        header('Cache-Control: no-store');
        header('X-Content-Type-Options: nosniff');
        echo json_encode(array('error' => $error), JSON_UNESCAPED_SLASHES);
        exit;
    }
}

final class PipboyMapHttpException extends RuntimeException
{
    private $statusCode;
    private $publicMessage;

    public function __construct(int $statusCode, string $publicMessage)
    {
        parent::__construct($publicMessage);
        $this->statusCode = $statusCode;
        $this->publicMessage = $publicMessage;
    }

    public function getStatusCode(): int
    {
        return $this->statusCode;
    }

    public function getPublicMessage(): string
    {
        return $this->publicMessage;
    }
}
