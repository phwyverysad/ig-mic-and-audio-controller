@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"


echo =======================================================
echo Compiling Instagram Mic and Audio Controller (C++ GUI)
echo =======================================================

:: ตรวจสอบตำแหน่ง vcvarsall.bat
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
) else (
    where cl.exe >nul 2>nul
    if %errorlevel% equ 0 (
        set "VCVARS=FOUND_IN_PATH"
    )
)

if "%VCVARS%"=="" (
    echo [ERROR] MSVC Compiler not found! Please install Visual Studio with C++ tools.
    exit /b 1
)

if not "%VCVARS%"=="FOUND_IN_PATH" (
    echo Setting up MSVC environment...
    call "%VCVARS%" x64 >nul
)

echo Compiling source files...

cl.exe /O2 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    config.cpp key_names.cpp ws_server.cpp hotkey_hook.cpp gui.cpp native_host.cpp extension_installer.cpp main.cpp ^
    /link user32.lib gdi32.lib comctl32.lib shell32.lib ws2_32.lib crypt32.lib advapi32.lib ^
    /subsystem:windows /entry:wWinMainCRTStartup /out:"IG Audio Controller.tmp.exe"

if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    exit /b %errorlevel%
)

if exist "IG Audio Controller.old.exe" del /f /q "IG Audio Controller.old.exe" 2>nul
taskkill /f /im "IG Audio Controller.exe" >nul 2>nul
taskkill /f /im InstagramHotkeyController.exe >nul 2>nul
ping -n 2 127.0.0.1 >nul
if exist "IG Audio Controller.exe" ren "IG Audio Controller.exe" "IG Audio Controller.old.exe" 2>nul
move /y "IG Audio Controller.tmp.exe" "IG Audio Controller.exe" >nul 2>nul
if exist "IG Audio Controller.old.exe" del /f /q "IG Audio Controller.old.exe" 2>nul
if exist InstagramHotkeyController.exe del /f /q InstagramHotkeyController.exe 2>nul

if %errorlevel% equ 0 (
    echo.
    echo [SUCCESS] Compilation completed successfully!
    echo Output: IG Audio Controller.exe
    del *.obj >nul 2>nul
    exit /b 0
) else (
    echo.
    echo [ERROR] Compilation failed!
    exit /b %errorlevel%
)
