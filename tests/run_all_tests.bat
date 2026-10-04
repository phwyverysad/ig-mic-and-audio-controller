@echo off
setlocal
cd /d "%~dp0"

echo =======================================================
echo RUNNING COMPREHENSIVE AUTOMATED TEST SUITE
echo =======================================================

echo.
echo [1/6] Running No-Emoji Verification Test...
node test_no_emojis.js
if %errorlevel% neq 0 (
    echo [FAILED] Emoji test failed!
    exit /b 1
)

echo.
echo [2/6] Running Chrome Extension Verification Test...
node test_extension.js
if %errorlevel% neq 0 (
    echo [FAILED] Extension test failed!
    exit /b 1
)

echo.
echo [3/6] Running C++ Core Unit Tests...
call run_cpp_test.bat
if %errorlevel% neq 0 (
    echo [FAILED] C++ Core tests failed!
    exit /b 1
)

echo.
echo [4/6] Running End-to-End WebSocket Integration Test...
call build_and_run_e2e.bat
if %errorlevel% neq 0 (
    echo [FAILED] E2E Integration test failed!
    exit /b 1
)

echo.
echo [5/6] Running Push-to-Talk and Mic Logic Verification Test...
node test_ptt_logic.js
if %errorlevel% neq 0 (
    echo [FAILED] PTT & Mic logic test failed!
    exit /b 1
)

echo.
echo [6/7] Running Real-time SPA and No-Refresh Verification Test...
node test_realtime_spa.js
if %errorlevel% neq 0 (
    echo [FAILED] Real-time SPA test failed!
    exit /b 1
)

echo.
echo [7/7] Running Extension Auto-Installer Verification Test...
node test_extension_installer.js
if %errorlevel% neq 0 (
    echo [FAILED] Extension Auto-Installer test failed!
    exit /b 1
)

echo.
echo =======================================================
echo [SUCCESS] ALL AUTOMATED TESTS PASSED WITH ZERO ERRORS!
echo =======================================================
exit /b 0
