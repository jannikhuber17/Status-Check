@echo off
REM ============================================================
REM  Staffel Viper Ready Check – Build Script
REM  Qt 5.15.2 MSVC 2019 x64 (matching TS3's own Qt5)
REM  Requires: VS 2022 Build Tools (C++ workload)
REM ============================================================

set QT5_DIR=C:\Qt\5.15.2\msvc2019_64
set CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe
set BUILD_DIR=build
set PACKAGE_NAME=staffel_viper_readycheck

REM ---- Locate MSBuild ----
REM Try vswhere first (-products * covers BuildTools AND full VS installs)
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
set MSBUILD=

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSBUILD=%%i
)

REM Fallback: known BuildTools path (C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools)
if "%MSBUILD%"=="" (
    set MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe
)

if not exist "%MSBUILD%" (
    echo [ERROR] MSBuild.exe nicht gefunden.
    echo  Erwartet unter: %MSBUILD%
    echo  Bitte VS 2022 Build Tools mit C++ Workload installieren.
    pause & exit /b 1
)
echo MSBuild: %MSBUILD%

REM ---- 1. CMake configure (VS generator, x64) ----
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

"%CMAKE%" -S . -B %BUILD_DIR% ^
    -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="%QT5_DIR%"

if errorlevel 1 ( echo [ERROR] CMake configure failed. & pause & exit /b 1 )

REM ---- 2. Build Release (via MSBuild directly) ----
"%MSBUILD%" "%BUILD_DIR%\staffel_viper_readycheck.sln" ^
    /p:Configuration=Release /p:Platform=x64 /m /nologo

if errorlevel 1 ( echo [ERROR] Build failed. & pause & exit /b 1 )

REM ---- 3. Package (nur unsere DLL, keine Qt-DLLs nötig) ----
"%CMAKE%" --build %BUILD_DIR% --config Release --target package_ts3plugin
if errorlevel 1 ( echo [ERROR] Packaging failed. & pause & exit /b 1 )

echo.
echo ======================================================
echo  Fertig!
echo  DLL:   %BUILD_DIR%\bin\Release\%PACKAGE_NAME%.dll
echo  Paket: %BUILD_DIR%\%PACKAGE_NAME%.ts3_plugin
echo  Qt5-DLLs werden von TS3 selbst bereitgestellt.
echo ======================================================
pause
