@echo off
REM ============================================================================
REM Sisi\GNSSLib\build.bat
REM Windows build script for GNSSLib (MSVC + Ninja)
REM ============================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0"

REM ---- Configuration ---------------------------------------------------------

if "%BUILD_TYPE%"==""  set BUILD_TYPE=Release
if "%BUILD_DIR%"==""   set BUILD_DIR=build
if "%INSTALL_PREFIX%"=="" set INSTALL_PREFIX=%cd%\out\install
if "%USE_NATIVE_OPT%"=="" set USE_NATIVE_OPT=OFF

REM ---- Sanity check ----------------------------------------------------------

if not exist "..\SignalSim\src\Coordinate.cpp" (
    echo [ERROR] SignalSim sources not found at ..\SignalSim\src
    echo         Expected layout:
    echo           Sisi\
    echo           +-- GNSSLib\        (this directory^)
    echo           +-- RinexActualizer\
    echo           +-- SignalSim\
    exit /b 1
)

REM ---- Clean -----------------------------------------------------------------

echo [INFO] Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

REM ---- Configure -------------------------------------------------------------

echo [INFO] Configuring CMake (build type: %BUILD_TYPE%)...
cmake -S . -B "%BUILD_DIR%" ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
    -DGNSSLIB_BUILD_SHARED=ON ^
    -DGNSSLIB_BUILD_STATIC=ON ^
    -DGNSSLIB_BUILD_EXAMPLES=ON ^
    -DGNSSLIB_BUILD_TOOLS=ON ^
    -DUSE_NATIVE_OPT=%USE_NATIVE_OPT%

if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    echo         Make sure you are running from a "Developer PowerShell for VS 2022"
    echo         or that cmake + ninja are on PATH.
    exit /b 1
)

REM ---- Build -----------------------------------------------------------------

echo [INFO] Building...
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel

if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

REM ---- Test ------------------------------------------------------------------

if exist "%BUILD_DIR%\bin\basic_usage.exe" (
    echo [INFO] Running example...
    pushd "%BUILD_DIR%\bin"
    basic_usage.exe
    popd
)


REM ---- Install ---------------------------------------------------------------

echo [INFO] Installing to %INSTALL_PREFIX%...
cmake --install "%BUILD_DIR%" --config %BUILD_TYPE%

REM ---- Summary ---------------------------------------------------------------

echo.
echo ==============================================
echo  GNSSLib build complete
echo ==============================================
echo  Build type      : %BUILD_TYPE%
echo  Build directory : %cd%\%BUILD_DIR%
echo  Libraries       : %BUILD_DIR%\lib
echo  Executables     : %BUILD_DIR%\bin
echo  Install prefix  : %INSTALL_PREFIX%
echo ==============================================
echo.

endlocal