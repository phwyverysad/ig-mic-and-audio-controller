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
    test_cpp_core.cpp ..\cpp_hotkey\config.cpp ..\cpp_hotkey\key_names.cpp ..\cpp_hotkey\ws_server.cpp ..\cpp_hotkey\native_host.cpp ^
    /link ws2_32.lib crypt32.lib advapi32.lib user32.lib /out:test_cpp_core.exe

if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile test_cpp_core
    exit /b %errorlevel%
)

test_cpp_core.exe
set TEST_STATUS=%errorlevel%
del *.obj test_cpp_core.exe 2>nul
exit /b %TEST_STATUS%
