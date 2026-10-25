# Prime Download Manager 3.0.47 (Qt 6 C++ Edition)

High-performance, modern native download manager built with Qt 6 C++, featuring IDM-style browser integration, multi-segmented downloading (default 8 threads, up to 32), and automatic lossless video stream capture.

## Build & Development

- **IDE:** Qt Creator / Visual Studio 2022
- **Framework:** Qt 6.8+ (Core, Gui, Widgets, Network, Concurrent)
- **Compiler:** MSVC 2022 (x64) / C++20
- **Build System:** CMake / qmake

### Quick Build (PowerShell / CMake)
```powershell
# Build standalone application:
.\build.ps1 -Target app

# Build zero-dependency setup installer:
.\build.ps1 -Target installer
```
Output binary generated in `build/src-qt/Release/PrimeDownloadManager.exe` and installer in `dist/PrimeDownloadManager_Setup.exe`.
