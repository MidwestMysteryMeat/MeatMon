# One-command package: builds Release and produces dist/MeatMon.zip
# (engine exe + game/ folder = the entire shippable product).
param(
    [string]$Config = "Release",
    [string]$Preset = "vs2026"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    cmake --preset $Preset
    cmake --build build --config $Config --target meatmon
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    $out = Join-Path $root "dist/MeatMon"
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    New-Item -ItemType Directory -Force $out | Out-Null

    Copy-Item (Join-Path $root "build/apps/game/$Config/meatmon.exe") $out
    Copy-Item -Recurse (Join-Path $root "game") (Join-Path $out "game")
    # Strip the local-only sprite drop-in (copyrighted rips must not ship).
    $drop = Join-Path $out "game/assets/pokemon"
    Get-ChildItem $drop -Exclude README.md -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force

    $zip = Join-Path $root "dist/MeatMon.zip"
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path "$out/*" -DestinationPath $zip
    Write-Host "packaged -> $zip"
}
finally {
    Pop-Location
}
