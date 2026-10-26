[CmdletBinding()]
param(
    [string] $CredentialsPath
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$packageRoot = Join-Path $projectRoot 'server\pipboy-map-api'

if ([string]::IsNullOrWhiteSpace($CredentialsPath)) {
    $preferredCredentials = Join-Path $packageRoot 'deploy.credentials.json'
    $legacyCredentials = Join-Path $packageRoot 'sftp-config.json'

    if (Test-Path -LiteralPath $preferredCredentials -PathType Leaf) {
        $CredentialsPath = $preferredCredentials
    } elseif (Test-Path -LiteralPath $legacyCredentials -PathType Leaf) {
        $CredentialsPath = $legacyCredentials
    } else {
        throw 'No se encontró deploy.credentials.json ni sftp-config.json en server\pipboy-map-api.'
    }
}

$credentialsFile = Resolve-Path -LiteralPath $CredentialsPath -ErrorAction Stop
$curl = Get-Command curl.exe -ErrorAction Stop

try {
    $credentials = Get-Content -LiteralPath $credentialsFile -Raw | ConvertFrom-Json
} catch {
    throw 'El archivo de credenciales no contiene JSON válido.'
}

$protocolValue = if ($null -ne $credentials.protocol) { [string] $credentials.protocol } else { [string] $credentials.type }
$usernameValue = if ($null -ne $credentials.username) { [string] $credentials.username } else { [string] $credentials.user }
$remoteRootValue = if ($null -ne $credentials.remote_root) { [string] $credentials.remote_root } else { [string] $credentials.remote_path }

$requiredFields = @{
    'protocol/type' = $protocolValue
    'host' = [string] $credentials.host
    'port' = [string] $credentials.port
    'username/user' = $usernameValue
    'password' = [string] $credentials.password
    'remote_root/remote_path' = $remoteRootValue
}
foreach ($field in $requiredFields.Keys) {
    if ([string]::IsNullOrWhiteSpace([string] $requiredFields[$field]) -or [string] $requiredFields[$field] -eq 'CHANGE_ME') {
        throw "Falta un valor válido para '$field' en el archivo de credenciales."
    }
}

$protocol = $protocolValue.ToLowerInvariant()
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

$remoteRoot = $remoteRootValue.Trim().Trim('/')
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
        '--user', ("{0}:{1}" -f $usernameValue, $credentials.password)
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
