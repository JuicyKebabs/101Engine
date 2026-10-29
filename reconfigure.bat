@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "RESULT=1"

if not exist "%~dp0CMakeLists.txt" (
    echo [ERROR] Place this file next to the project's top-level CMakeLists.txt.
    goto :finish
)

rem Prefer Visual Studio 2026's CMake (VS 2026 requires CMake 4.2 or later).
set "CMAKE_EXE="

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined CMAKE_EXE if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`call "%VSWHERE%" -latest -products * -version "[18.0,19.0)" -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe 2^>nul`) do (
        if not defined CMAKE_EXE if exist "%%I" set "CMAKE_EXE=%%I"
    )
)

for /f "delims=" %%I in ('where cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"

rem Also support standalone CMake installations that are not on PATH.
for %%I in ("%ProgramFiles%\CMake\bin\cmake.exe" "%ProgramFiles(x86)%\CMake\bin\cmake.exe" "%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe") do (
    if not defined CMAKE_EXE if exist "%%~I" set "CMAKE_EXE=%%~I"
)

if not defined CMAKE_EXE (
    echo [ERROR] cmake.exe was not found. Install CMake 4.2 or later and add its bin folder to PATH,
    echo         or install C++ CMake tools for Windows in Visual Studio 2026.
    goto :finish
)

echo Using CMake: "%CMAKE_EXE%"
echo Configuring Visual Studio 2026 x64...
rem Run reconfigure.bat --fresh once when migrating an existing VS 2022 build.
"%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build" -G "Visual Studio 18 2026" -A x64 %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
    echo [ERROR] CMake configuration failed. See the output above.
    goto :finish
)

echo [OK] Configuration complete.

:finish
echo.
pause
exit /b %RESULT%
