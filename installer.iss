; Inno Setup Script for Prime Download Manager 3.0.46
; Professional Single-File Executable Installer

#define MyAppName "Prime Download Manager"
#define MyAppVersion "3.0.46"
#define MyAppPublisher "PrimeApps"
#define MyAppExeName "PrimeDownloadManager.exe"
#define MyAppIcon "src-qt\resources\icons\app_logo.ico"

[Setup]
AppId={{9F5796E4-2DB6-4FA2-85C2-4E26E51CB732}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\PrimeDownloadManager
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=PrimeDownloadManager_Setup
SetupIconFile={#MyAppIcon}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
CloseApplicationsFilter=PrimeDownloadManager.exe
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "Start Prime Download Manager automatically with Windows"; GroupDescription: "System Startup:"

[Files]
; 1. Main Application & Runtime Files (including built-in bin\ffmpeg.exe and bin\yt-dlp.exe)
Source: "src-qt\build\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.log,opengl32sw.dll,d3dcompiler_47.dll,dxcompiler.dll,dxil.dll,vc_redist.x64.exe"
Source: "src-qt\resources\icons\app_logo.ico"; DestDir: "{app}"; Flags: ignoreversion

; 2. Extract Browser Extension directly to %USERPROFILE%\Documents\PDM Extension
Source: "PDM Extension\*"; DestDir: "{userdocs}\PDM Extension"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_logo.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_logo.ico"; Tasks: desktopicon

[Registry]
; Windows Background System Startup (Tray)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "PrimeDownloadManager"; ValueData: """{app}\{#MyAppExeName}"" --background"; Flags: uninsdeletevalue; Tasks: autostart

; Browser Extension Integration Registry Keys
Root: HKCU; Subkey: "Software\Google\Chrome\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Google\Chrome\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Microsoft\Edge\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Microsoft\Edge\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\BraveSoftware\Brave-Browser\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\BraveSoftware\Brave-Browser\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Opera Software\Opera Stable\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Opera Software\Opera Stable\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
