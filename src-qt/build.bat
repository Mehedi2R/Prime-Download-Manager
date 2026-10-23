@echo off
setlocal enabledelayedexpansion

:: 0. Always anchor current directory to this script's directory (src-qt)
cd /d "%~dp0"

echo ========================================================
echo    Building Prime Download Manager (Qt 6 C++ Edition)
echo ========================================================
echo.

:: 1. Terminate running instance if any to prevent file locking
taskkill /F /IM PrimeDownloadManager.exe 2>nul
ping 127.0.0.1 -n 2 >nul

:: 2. Find Visual Studio Compiler Environment
set "VS_PATH="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if not defined VS_PATH (
    echo [ERROR] Visual Studio C++ Compiler was not detected on this machine.
    echo Please install Visual Studio Community / Professional with C++ Desktop Development.
    echo.
    pause
    exit /b 1
)

echo [INFO] Found Visual Studio at: !VS_PATH!
call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to initialize MSVC 64-bit environment.
    echo.
    pause
    exit /b %ERRORLEVEL%
)

:: 3. Find Qt 6
set "QT_DIR="
if exist "C:\Qt\6.8.0\msvc2022_64" (
    set "QT_DIR=C:\Qt\6.8.0\msvc2022_64"
) else if defined CMAKE_PREFIX_PATH (
    set "QT_DIR=%CMAKE_PREFIX_PATH%"
) else (
    for /d %%d in (C:\Qt\6.*.*) do (
        for /d %%m in (%%d\msvc*) do (
            if exist "%%m\bin\qmake.exe" set "QT_DIR=%%m"
        )
    )
)

if not defined QT_DIR (
    echo [WARNING] Default Qt 6 directory not found in C:\Qt\6.8.0\msvc2022_64.
) else (
    echo [INFO] Using Qt 6 from: !QT_DIR!
)

:: 4. Ensure embedded ffmpeg and yt-dlp binaries are present in bin/
if not exist "bin" mkdir "bin"

if not exist "bin\ffmpeg.exe" (
    if exist "%LOCALAPPDATA%\PrimeDownloadManager\bin\ffmpeg.exe" (
        echo [INFO] Restoring ffmpeg.exe from local app cache...
        copy /y "%LOCALAPPDATA%\PrimeDownloadManager\bin\ffmpeg.exe" "bin\ffmpeg.exe" >nul
    ) else (
        echo [INFO] Downloading official standalone ffmpeg.exe...
        powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; curl.exe -L 'https://github.com/eugeneware/ffmpeg-static/releases/download/b4.4/win32-x64' -o 'bin\ffmpeg.exe'"
    )
)

if not exist "bin\yt-dlp.exe" (
    if exist "%LOCALAPPDATA%\PrimeDownloadManager\bin\yt-dlp.exe" (
        echo [INFO] Restoring yt-dlp.exe from local app cache...
        copy /y "%LOCALAPPDATA%\PrimeDownloadManager\bin\yt-dlp.exe" "bin\yt-dlp.exe" >nul
    ) else (
        echo [INFO] Downloading official standalone yt-dlp.exe...
        powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; curl.exe -L 'https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe' -o 'bin\yt-dlp.exe'"
    )
)

if exist "bin\ffmpeg.exe" (
    echo [INFO] ffmpeg.exe verified in bin\
) else (
    echo [WARNING] ffmpeg.exe could not be verified in bin\
)

if exist "bin\yt-dlp.exe" (
    echo [INFO] yt-dlp.exe verified in bin\
) else (
    echo [WARNING] yt-dlp.exe could not be verified in bin\
)

:: 5. Create build directory
if not exist "build" mkdir "build"
cd build

echo.
echo [INFO] Running CMake configure...
if defined QT_DIR (
    cmake -A x64 -DCMAKE_PREFIX_PATH="!QT_DIR!" ..
) else (
    cmake -A x64 ..
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake configuration failed.
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [INFO] Compiling Project (Release)...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed.
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [INFO] Deploying Qt runtime DLLs...
if defined QT_DIR (
    if exist "!QT_DIR!\bin\windeployqt.exe" (
        "!QT_DIR!\bin\windeployqt.exe" --release "Release\PrimeDownloadManager.exe"
    )
)

:: 6. Copy embedded media tools to Release\bin directory
if not exist "Release\bin" mkdir "Release\bin"
if exist "..\bin\yt-dlp.exe" (
    copy /y "..\bin\yt-dlp.exe" "Release\bin\yt-dlp.exe" >nul
)
if exist "..\bin\ffmpeg.exe" (
    copy /y "..\bin\ffmpeg.exe" "Release\bin\ffmpeg.exe" >nul
)

:: 7. Clean unnecessary windeployqt bloat
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

cd ..

echo.
echo ========================================================
echo  BUILD SUCCESSFUL!
echo  Executable: src-qt\build\Release\PrimeDownloadManager.exe
echo  Media Tools: src-qt\build\Release\bin\ (ffmpeg, yt-dlp)
echo ========================================================
echo.
pause
