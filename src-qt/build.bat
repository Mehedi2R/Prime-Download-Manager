@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo    Building Prime Download Manager (Qt 6 C++ Edition)
echo ========================================================

:: Terminate running instance if any to prevent file locking
taskkill /F /IM PrimeDownloadManager.exe 2>nul

:: 1. Find Visual Studio Compiler Environment
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_PATH=%%i"
)

if not defined VS_PATH (
    echo [ERROR] Visual Studio C++ Compiler was not detected.
    exit /b 1
)

echo [INFO] Found Visual Studio at: !VS_PATH!
call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"

:: 2. Find Qt 6
if exist "C:\Qt\6.8.0\msvc2022_64" (
    set "QT_DIR=C:\Qt\6.8.0\msvc2022_64"
) else if defined CMAKE_PREFIX_PATH (
    set "QT_DIR=%CMAKE_PREFIX_PATH%"
) else (
    echo [WARNING] Qt 6 installation directory was not found in C:\Qt\6.8.0\msvc2022_64.
)

echo [INFO] Using Qt 6 from: !QT_DIR!

:: 3. Create build directory
if not exist "build" mkdir "build"
cd build

echo [INFO] Running CMake...
cmake -A x64 -DCMAKE_PREFIX_PATH="!QT_DIR!" ..

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo [INFO] Compiling Project (Release)...
cmake --build . --config Release

if %ERRORLEVEL% EQU 0 (
    echo [INFO] Deploying Qt runtime DLLs...
    if exist "!QT_DIR!\bin\windeployqt.exe" (
        "!QT_DIR!\bin\windeployqt.exe" --release "Release\PrimeDownloadManager.exe"
    )
    
    :: Copy media tools to bin directory if present
    if not exist "Release\bin" mkdir "Release\bin"
    if exist "..\bin\yt-dlp.exe" (
        copy /y "..\bin\yt-dlp.exe" "Release\bin\yt-dlp.exe" >nul
    )
    if exist "..\bin\ffmpeg.exe" (
        copy /y "..\bin\ffmpeg.exe" "Release\bin\ffmpeg.exe" >nul
    )

    :: Remove unnecessary windeployqt bloat
    del /q "Release\opengl32sw.dll" 2>nul
    del /q "Release\d3dcompiler_47.dll" 2>nul
    del /q "Release\dxcompiler.dll" 2>nul
    del /q "Release\dxil.dll" 2>nul
    del /q "Release\vc_redist.x64.exe" 2>nul
    del /q "Release\tls\qcertonlybackend.dll" 2>nul
    del /q "Release\imageformats\qgif.dll" 2>nul
    del /q "Release\imageformats\qjpeg.dll" 2>nul
    if exist "Release\generic" rmdir /s /q "Release\generic" 2>nul
    if exist "Release\networkinformation" rmdir /s /q "Release\networkinformation" 2>nul
    if exist "Release\styles" rmdir /s /q "Release\styles" 2>nul
    if exist "Release\translations" rmdir /s /q "Release\translations" 2>nul

    echo ========================================================
    echo  Build Successful! Standalone App generated in:
    echo  src-qt\build\Release\PrimeDownloadManager.exe
    echo ========================================================
) else (
    echo [ERROR] Compilation failed.
)
