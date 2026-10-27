<?php

declare(strict_types=1);

/**
 * Static PNG map endpoint for the Pip-Boy ESP32.
 *
 * The endpoint returns the exact PNG bytes received from Mapbox. It never
 * thresholds, recolors, re-encodes, resizes, or otherwise processes pixels.
 */
final class PipboyMapEndpoint
{
    private const MAX_UPSTREAM_BYTES = 2097152;
    // Separa las entradas PNG crudas de la caché PBM1 usada por versiones
    // anteriores del endpoint.
    private const RAW_PNG_CACHE_VERSION = '1';

    public static function run(string $configPath): void
    {
        try {
            $config = self::loadConfig($configPath);
            self::requireGetMethod();
            self::authenticate($config);

            $request = self::parseRequest($config);
            $cacheFile = self::cachePath($config, $request);
            $image = self::readFreshCache($cacheFile, $config);

            if ($image === null) {
                $image = self::fetchMapboxImage($config, $request);
                self::saveSourceImage($image, $request);
                self::writeCache($cacheFile, $image);
            }

            self::sendMap($image, $config['attribution']);
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

        if (!function_exists('curl_init')) {
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
            self::RAW_PNG_CACHE_VERSION,
            // El estilo forma parte del resultado. Incluirlo invalida
            // automáticamente las imágenes al publicar o sustituir el estilo.
            (string) $config['mapbox_style_owner'],
            (string) $config['mapbox_style_id'],
            number_format(round($request['latitude'], 4), 4, '.', ''),
            number_format(round($request['longitude'], 4), 4, '.', ''),
            $request['zoom'],
            $request['size_name'],
        ));

        return rtrim($cacheDirectory, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . hash('sha256', $cacheKey) . '.png';
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

        $image = file_get_contents($cacheFile);
        if ($image === false || !self::isPng($image)) {
            return null;
        }

        return $image;
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

        if ($result === false || $statusCode < 200 || $statusCode >= 300 ||
            stripos($contentType, 'image/') !== 0 || !self::isPng($responseBody)) {
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

    private static function isPng(string $image): bool
    {
        return strlen($image) >= 24 &&
            substr($image, 0, 8) === "\x89PNG\r\n\x1a\n" &&
            substr($image, 12, 4) === 'IHDR';
    }

    private static function writeCache(string $cacheFile, string $payload): void
    {
        $temporaryFile = $cacheFile . '.' . bin2hex(random_bytes(6)) . '.tmp';
        if (file_put_contents($temporaryFile, $payload, LOCK_EX) === false || !rename($temporaryFile, $cacheFile)) {
            @unlink($temporaryFile);
            throw new PipboyMapHttpException(500, 'cache_unavailable');
        }
    }

    private static function sendMap(string $image, string $attribution): void
    {
        $safeAttribution = str_replace(array("\r", "\n"), '', $attribution);

        http_response_code(200);
        header('Content-Type: image/png');
        header('Content-Length: ' . strlen($image));
        header('Cache-Control: no-store');
        header('X-Content-Type-Options: nosniff');
        header('X-Pipboy-Attribution: ' . $safeAttribution);
        echo $image;
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
