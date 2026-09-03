[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("x64", "win32")]
    [string]$Architecture,

    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$DestinationPath = [System.IO.Path]::GetFullPath($Destination)
$RuntimeDirectory = if ($Architecture -eq "x64") { "win64" } else { "win32" }

New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null

function Copy-PortableFile {
    param(
        [Parameter(Mandatory = $true)] [string]$Source,
        [Parameter(Mandatory = $true)] [string]$Name
    )

    $SourcePath = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Source))
    $TargetPath = [System.IO.Path]::GetFullPath((Join-Path $DestinationPath $Name))
    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "Missing required Tapehead portable file: $SourcePath"
    }

    if (-not [string]::Equals(
        $SourcePath, $TargetPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        Copy-Item -LiteralPath $SourcePath -Destination $TargetPath -Force
    }
}

$RuntimeAssets = @(
    "FT2.CFG",
    "tapehead.ini",
    "palette.pal",
    "fastTracksLogoBadges.bmp",
    "tapeheadSplash.png"
)

foreach ($Asset in $RuntimeAssets) {
    Copy-PortableFile -Source "release/other/$Asset" -Name $Asset
}

Copy-PortableFile -Source "release/$RuntimeDirectory/SDL2.dll" -Name "SDL2.dll"
Copy-PortableFile -Source "LICENSE" -Name "LICENSE.txt"
Copy-PortableFile -Source "release/LICENSES.txt" -Name "LICENSES.txt"
Copy-PortableFile -Source "release/problems.txt" -Name "problems.txt"
Copy-PortableFile -Source "release/PORTABLE_README.txt" -Name "README.txt"

$Executable = Join-Path $DestinationPath "Tapehead.exe"
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Tapehead.exe was not found in the portable destination: $DestinationPath"
}

Write-Host "Tapehead portable runtime staged in '$DestinationPath'."
