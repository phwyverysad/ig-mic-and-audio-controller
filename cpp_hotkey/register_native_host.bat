@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Registering Native Messaging Host for Auto-Start
echo ========================================================

set "DIR=%~dp0"
set "EXE=%DIR%IG Audio Controller.exe"
set "JSON=%DIR%com.instagram.hotkey.controller.json"

:: Escape backslashes for JSON
set "ESCAPED_EXE=%EXE:\=\\%"

:: Create or update JSON manifest
(
echo {
echo   "name": "com.instagram.hotkey.controller",
echo   "description": "Instagram Hotkey Controller Host",
echo   "path": "!ESCAPED_EXE!",
echo   "type": "stdio",
echo   "allowed_origins": [
echo     "chrome-extension://ljnimllanldnbhcibpeggokmgpmphdoh/"
echo   ]
echo }
) > "%JSON%"

:: Register in Registry for Chrome, Edge, and Brave
reg add "HKCU\Software\Google\Chrome\NativeMessagingHosts\com.instagram.hotkey.controller" /ve /t REG_SZ /d "%JSON%" /f >nul
reg add "HKCU\Software\Microsoft\Edge\NativeMessagingHosts\com.instagram.hotkey.controller" /ve /t REG_SZ /d "%JSON%" /f >nul
reg add "HKCU\Software\BraveSoftware\Brave-Browser\NativeMessagingHosts\com.instagram.hotkey.controller" /ve /t REG_SZ /d "%JSON%" /f >nul

echo [SUCCESS] Native Messaging Host registered successfully.
echo When you open Instagram in your browser, the controller app will launch automatically.
exit /b 0
