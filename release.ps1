param(
    [string]$Version = "1.0"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$buildDir = "$root\cmake-build-release"
$staging = "$root\build\msi_fb-$Version"
$outZip = "$root\build\msi_fb-$Version.zip"

Write-Host "=== Building MSI_FB v$Version ===" -ForegroundColor Cyan

# Configure
Write-Host "[1/4] Configuring..."
cmake -S $root -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

# Build
Write-Host "[2/4] Building..."
cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

# Stage
Write-Host "[3/4] Staging release files..."
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Path $staging -Force | Out-Null

Copy-Item "$buildDir\msi_fb.exe" "$staging\"
Copy-Item "$root\LpcACPIEC.bin" "$staging\"
Copy-Item "$root\install.bat" "$staging\"
Copy-Item "$root\install.ps1" "$staging\"
Copy-Item "$root\README.md" "$staging\"
if (Test-Path "$root\PawnIO_setup.exe") {
    Copy-Item "$root\PawnIO_setup.exe" "$staging\"
} else {
    Write-Host "WARNING: PawnIO_setup.exe not found in project root. Add it manually to the zip." -ForegroundColor Yellow
}

# Package
Write-Host "[4/4] Creating zip..."
if (Test-Path $outZip) { Remove-Item -Force $outZip }
Compress-Archive -Path "$staging\*" -DestinationPath $outZip

Write-Host ""
Write-Host "Done! Output: $outZip" -ForegroundColor Green
Write-Host "Contents:"
Get-ChildItem $staging -Recurse | ForEach-Object {
    $rel = $_.FullName.Substring($staging.Length + 1)
    if ($_.PSIsContainer) { Write-Host "  $rel/" }
    else { Write-Host "  $rel  ($([math]::Round($_.Length/1KB)) KB)" }
}
