[CmdletBinding()]
param(
    [ValidateSet("x64", "win32")]
    [string]$Architecture = "x64",

    [string]$Binary,

    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$RuntimeDirectory = if ($Architecture -eq "x64") { "win64" } else { "win32" }

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $RepoRoot "dist"
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

if ([string]::IsNullOrWhiteSpace($Binary)) {
    $Binary = Join-Path $RepoRoot "release/$RuntimeDirectory/Tapehead.exe"
}
$Binary = [System.IO.Path]::GetFullPath($Binary)
if (-not (Test-Path -LiteralPath $Binary -PathType Leaf)) {
    throw "Tapehead executable not found: $Binary"
}

$ArchitectureLabel = if ($Architecture -eq "x64") { "x64" } else { "x86" }
$PackageName = "Tapehead-Windows-$ArchitectureLabel"
$StagingDirectory = Join-Path $OutputDirectory $PackageName
$ArchivePath = Join-Path $OutputDirectory "$PackageName.zip"

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
if (Test-Path -LiteralPath $StagingDirectory) {
    Remove-Item -LiteralPath $StagingDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $StagingDirectory | Out-Null

Copy-Item -LiteralPath $Binary -Destination (Join-Path $StagingDirectory "Tapehead.exe")
& (Join-Path $PSScriptRoot "stage-windows-portable.ps1") `
    -Architecture $Architecture -Destination $StagingDirectory

if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}
Compress-Archive -LiteralPath $StagingDirectory -DestinationPath $ArchivePath `
    -CompressionLevel Optimal

Write-Host "Created '$ArchivePath'."
