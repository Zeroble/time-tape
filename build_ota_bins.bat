@echo off
setlocal

set "ENV_NAME=esp32-c3-supermini"
if not "%~1"=="" set "ENV_NAME=%~1"

set "PROJECT_DIR=%~dp0"
set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"

if not exist "%PIO_EXE%" (
    where /q platformio
    if %ERRORLEVEL%==0 (
        set "PIO_EXE=platformio"
    ) else (
        where /q pio
        if %ERRORLEVEL%==0 (
            set "PIO_EXE=pio"
        ) else (
            echo [ERROR] PlatformIO executable not found.
            echo         Install PlatformIO or add it to PATH.
            exit /b 1
        )
    )
)

pushd "%PROJECT_DIR%" >nul

echo [1/2] Building firmware.bin (env: %ENV_NAME%)...
call "%PIO_EXE%" run -e "%ENV_NAME%"
if errorlevel 1 goto :fail

echo [2/2] Building littlefs.bin (env: %ENV_NAME%)...
call "%PIO_EXE%" run -e "%ENV_NAME%" -t buildfs
if errorlevel 1 goto :fail

set "OUT_DIR=.pio\build\%ENV_NAME%"
echo.
echo Build complete.
echo firmware : "%CD%\%OUT_DIR%\firmware.bin"
echo littlefs : "%CD%\%OUT_DIR%\littlefs.bin"
popd >nul
exit /b 0

:fail
set "ERR=%ERRORLEVEL%"
echo.
echo Build failed (code: %ERR%).
popd >nul
exit /b %ERR%

