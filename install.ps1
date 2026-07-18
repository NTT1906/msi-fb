$ErrorActionPreference = "Stop"
$dir = $PSScriptRoot

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  MSI_FB Installer" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Run PawnIO installer (shows full UI to the user)
$pawnioSetup = Join-Path $dir "PawnIO_setup.exe"
if (Test-Path $pawnioSetup) {
    Write-Host "[1/2] Installing PawnIO..."
    Start-Process -FilePath $pawnioSetup -Wait
    Write-Host "PawnIO installed." -ForegroundColor Green
} else {
    Write-Host "[1/2] PawnIO_setup.exe not found, skipping." -ForegroundColor Yellow
    Write-Host "       Make sure PawnIO is already installed." -ForegroundColor Yellow
}

Write-Host ""

# Install MSI_FB
$msiFb = Join-Path $dir "msi_fb.exe"
Write-Host "[2/2] Installing MSI_FB..."
& $msiFb --install
if ($LASTEXITCODE -ne 0) {
    Write-Host "MSI_FB install failed." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  MSI_FB installed successfully!" -ForegroundColor Green
Write-Host "  The app will start on next login." -ForegroundColor Green
Write-Host "  Double-click msi_fb.exe to run it now." -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
