[CmdletBinding()]
param(
    [string] $CredentialsPath = (Join-Path $PSScriptRoot '..\server\pipboy-map-api\deploy.credentials.json')
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$packageRoot = Join-Path $projectRoot 'server\pipboy-map-api'
$credentialsFile = Resolve-Path -LiteralPath $CredentialsPath -ErrorAction Stop
$curl = Get-Command curl.exe -ErrorAction Stop

try {
    $credentials = Get-Content -LiteralPath $credentialsFile -Raw | ConvertFrom-Json
} catch {
    throw 'El archivo de credenciales no contiene JSON válido.'
}

$requiredFields = @('protocol', 'host', 'port', 'username', 'password', 'remote_root')
foreach ($field in $requiredFields) {
    if ([string]::IsNullOrWhiteSpace([string] $credentials.$field) -or [string] $credentials.$field -eq 'CHANGE_ME') {
        throw "Falta un valor válido para '$field' en el archivo de credenciales."
    }
}

$protocol = ([string] $credentials.protocol).ToLowerInvariant()
if ($protocol -notin @('ftp', 'ftps', 'sftp')) {
    throw "El protocolo debe ser ftp, ftps o sftp; se recibió '$protocol'."
}

$port = 0
if (-not [int]::TryParse([string] $credentials.port, [ref] $port) -or $port -lt 1 -or $port -gt 65535) {
    throw 'El puerto debe estar entre 1 y 65535.'
}

$configFile = Join-Path $packageRoot 'config.php'
if (-not (Test-Path -LiteralPath $configFile -PathType Leaf)) {
    throw 'Falta server\pipboy-map-api\config.php. Copia config.example.php, agrega las claves y vuelve a ejecutar.'
}

$files = @(
    $configFile,
    (Join-Path $packageRoot 'public\map.php'),
    (Join-Path $packageRoot 'src\MapEndpoint.php')
)

foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Falta el archivo de despliegue requerido: $file"
    }
}

function ConvertTo-RemotePath([string] $path) {
    $normalized = ($path -replace '\\', '/').Trim('/')
    return (($normalized -split '/') | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/'
}

$remoteRoot = ([string] $credentials.remote_root).Trim().Trim('/')
if ($remoteRoot -eq '') {
    throw 'remote_root no puede estar vacío.'
}

foreach ($localFile in $files) {
    $relativePath = [IO.Path]::GetRelativePath($packageRoot, $localFile).Replace('\', '/')
    $remotePath = ConvertTo-RemotePath "$remoteRoot/$relativePath"

    switch ($protocol) {
        'sftp' { $url = "sftp://$($credentials.host):$port/$remotePath" }
        'ftps' { $url = "ftp://$($credentials.host):$port/$remotePath" }
        default { $url = "ftp://$($credentials.host):$port/$remotePath" }
    }

    $arguments = @(
        '--fail', '--show-error', '--silent', '--ftp-create-dirs',
        '--user', ("{0}:{1}" -f $credentials.username, $credentials.password)
    )

    if ($protocol -eq 'ftps') {
        $arguments += '--ssl-reqd'
    }

    $arguments += @('--upload-file', $localFile, $url)

    Write-Host "Subiendo $relativePath..."
    & $curl.Source @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Falló la subida de $relativePath (cURL exit code $LASTEXITCODE)."
    }
}

Write-Host 'Despliegue completado. Configura el document root del subdominio en <remote_root>/public y verifica HTTPS.'
