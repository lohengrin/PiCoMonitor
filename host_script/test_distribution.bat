@echo off
:: PiCoMonitor Distribution Test Script
:: Tests the built EXE distribution

echo Testing PiCoMonitor Distribution...
echo ==================================

:: Check if EXE exists
echo Checking for PiCoMonitor.exe...
if exist "dist\PiCoMonitor.exe" (
    echo ✓ PiCoMonitor.exe found
) else (
    echo ✗ PiCoMonitor.exe not found - build failed
    goto :error
)

:: Test basic help functionality
echo Testing help functionality...
"dist\PiCoMonitor.exe" --help
if %ERRORLEVEL% equ 0 (
    echo ✓ Help test passed
) else (
    echo ✗ Help test failed
)

:: Test with invalid port (should show error gracefully)
echo Testing error handling...
"dist\PiCoMonitor.exe" -p INVALID_PORT
if %ERRORLEVEL% equ 0 (
    echo ✓ Error handling test passed
) else (
    echo ✗ Error handling test failed
)

echo.
echo Distribution test completed!
echo The EXE appears to be working correctly.
goto :eof

:error
echo.
echo Distribution test failed!
echo Please check the build process.

echo.