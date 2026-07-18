@echo off
setlocal

echo ============================================
echo   MSI_FB Installer
echo ============================================
echo.

:: Run PawnIO installer
if exist "%~dp0PawnIO_setup.exe" (
    echo [1/2] Installing PawnIO...
    "%~dp0PawnIO_setup.exe"
    if errorlevel 1 (
        echo PawnIO setup failed or was cancelled.
        echo MSI_FB requires PawnIO to function.
        pause
        exit /b 1
    )
    echo PawnIO installed.
) else (
    echo [1/2] PawnIO_setup.exe not found, skipping.
    echo       Make sure PawnIO is already installed.
)

echo.

:: Install MSI_FB
echo [2/2] Installing MSI_FB...
"%~dp0msi_fb.exe" --install
if errorlevel 1 (
    echo MSI_FB install failed.
    pause
    exit /b 1
)

echo.
echo ============================================
echo   MSI_FB installed successfully!
echo   The app will start on next login.
echo   Double-click msi_fb.exe to run it now.
echo ============================================
pause
