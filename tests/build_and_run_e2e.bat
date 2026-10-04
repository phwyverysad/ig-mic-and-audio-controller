@echo off
setlocal
cd /d "%~dp0"

set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
) else (
    where cl.exe >nul 2>nul
    if %errorlevel% equ 0 set "VCVARS=FOUND_IN_PATH"
)

if "%VCVARS%"=="" (
    echo [ERROR] MSVC not found
    exit /b 1
)
if not "%VCVARS%"=="FOUND_IN_PATH" (
    call "%VCVARS%" x64 >nul
)

cl.exe /O2 /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    e2e_server_runner.cpp ..\cpp_hotkey\ws_server.cpp ^
    /link ws2_32.lib crypt32.lib advapi32.lib user32.lib /out:e2e_server_runner.exe >nul

if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile e2e_server_runner
    exit /b %errorlevel%
)

node test_e2e_client.js
set TEST_STATUS=%errorlevel%
del *.obj e2e_server_runner.exe 2>nul
exit /b %TEST_STATUS%
