param(
    [Parameter(Mandatory = $true)]
    [string]$InputGif,

    [Parameter(Mandatory = $true)]
    [string]$OutputHeader,

    [string]$Symbol = "referencePipboyGif"
)

$ErrorActionPreference = "Stop"

$inputPath = (Resolve-Path -LiteralPath $InputGif).Path
$outputDirectory = Split-Path -Parent $OutputHeader
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}

$bytes = [System.IO.File]::ReadAllBytes($inputPath)
$sourceName = Split-Path -Leaf $inputPath
$builder = [System.Text.StringBuilder]::new()

[void]$builder.AppendLine("// Archivo generado desde $sourceName. No editar a mano.")
[void]$builder.AppendLine("#pragma once")
[void]$builder.AppendLine()
[void]$builder.AppendLine("#include <Arduino.h>")
[void]$builder.AppendLine()
[void]$builder.AppendLine("const uint8_t $Symbol`[] PROGMEM = {")

for ($offset = 0; $offset -lt $bytes.Length; $offset += 12) {
    $count = [Math]::Min(12, $bytes.Length - $offset)
    $values = for ($index = 0; $index -lt $count; $index++) {
        "0x{0:X2}" -f $bytes[$offset + $index]
    }
    [void]$builder.Append("  ")
    [void]$builder.Append(($values -join ", "))
    if ($offset + $count -lt $bytes.Length) {
        [void]$builder.Append(",")
    }
    [void]$builder.AppendLine()
}

[void]$builder.AppendLine("};")
[void]$builder.AppendLine("constexpr size_t $Symbol`Size = sizeof($Symbol);")

[System.IO.File]::WriteAllText(
    $OutputHeader,
    $builder.ToString(),
    [System.Text.UTF8Encoding]::new($false)
)

Write-Output "Generado: $OutputHeader ($($bytes.Length) bytes)"
