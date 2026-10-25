; Inno Setup Script for Prime Download Manager
; Professional Standalone Single-File Executable Installer

#ifndef MyAppVersion
#define MyAppVersion "3.0.47"
#endif

#define MyAppName "Prime Download Manager"
#define MyAppPublisher "@Mehedi2R"
#define MyAppExeName "PrimeDownloadManager.exe"
#define MyAppIcon "src-qt\resources\icons\app_logo.ico"

[Setup]
AppId={{9F5796E4-2DB6-4FA2-85C2-4E26E51CB732}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL="https://github.com/Mehedi2R"
AppSupportURL="https://github.com/Mehedi2R"
AppUpdatesURL="https://github.com/Mehedi2R"
DefaultDirName={localappdata}\PrimeDownloadManager
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=PrimeDownloadManager_v{#MyAppVersion}
SetupIconFile={#MyAppIcon}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=force
CloseApplicationsFilter=PrimeDownloadManager.exe
RestartApplications=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Setup
VersionInfoCopyright=Copyright (C) 2026 {#MyAppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "Start Prime Download Manager automatically with Windows"; GroupDescription: "System Startup:"

#ifndef AppBuildDir
#define AppBuildDir "build\src-qt\Release"
#endif

[Files]
; 1. Main Application & Runtime Files (including built-in bin\ffmpeg.exe and bin\yt-dlp.exe)
Source: "{#AppBuildDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.ilk,*.exp,*.lib,*.obj,*.log"
Source: "src-qt\resources\icons\app_logo.ico"; DestDir: "{app}"; Flags: ignoreversion

; 2. Browser Extension bundled into App directory as well as User Documents
Source: "PDM Extension\*"; DestDir: "{app}\PDM Extension"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "PDM Extension\*"; DestDir: "{userdocs}\PDM Extension"; Flags: ignoreversion recursesubdirs createallsubdirs uninsneveruninstall

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_logo.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_logo.ico"; Tasks: desktopicon

[Registry]
; Windows Background System Startup (Tray)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "PrimeDownloadManager"; ValueData: """{app}\{#MyAppExeName}"" --background"; Flags: uninsdeletevalue; Tasks: autostart

; Browser Extension Integration Registry Keys (Chromium Browsers)
Root: HKCU; Subkey: "Software\Google\Chrome\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Google\Chrome\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Microsoft\Edge\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Microsoft\Edge\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\BraveSoftware\Brave-Browser\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\BraveSoftware\Brave-Browser\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Opera Software\Opera Stable\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "path"; ValueData: "{userdocs}\PDM Extension"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Opera Software\Opera Stable\Extensions\olhnegddnkahnojcnnilpbmpomkclggm"; ValueType: string; ValueName: "version"; ValueData: "{#MyAppVersion}"; Flags: uninsdeletekeyifempty

[InstallDelete]
Type: files; Name: "{app}\{#MyAppExeName}.old"

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM {#MyAppExeName}"; Flags: runhidden skipifdoesntexist; RunOnceId: "KillAppBeforeUninstall"

[UninstallDelete]
Type: files; Name: "{app}\*.log"
Type: filesandordirs; Name: "{app}\PDM Extension"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
procedure StopRunningApp();
var
  ResultCode: Integer;
  WaitCount: Integer;
begin
  if CheckForMutexes('Local\PrimeDownloadManager_SingleInstance_Mutex_v3') then
  begin
    // Attempt graceful termination first
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/IM {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    WaitCount := 0;
    while CheckForMutexes('Local\PrimeDownloadManager_SingleInstance_Mutex_v3') and (WaitCount < 10) do
    begin
      Sleep(100);
      WaitCount := WaitCount + 1;
    end;

    // Force termination if process is still active
    if CheckForMutexes('Local\PrimeDownloadManager_SingleInstance_Mutex_v3') then
    begin
      Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM {#MyAppExeName}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      WaitCount := 0;
      while CheckForMutexes('Local\PrimeDownloadManager_SingleInstance_Mutex_v3') and (WaitCount < 15) do
      begin
        Sleep(100);
        WaitCount := WaitCount + 1;
      end;
    end;
    Sleep(200);
  end;
end;

function InitializeSetup(): Boolean;
begin
  StopRunningApp();
  Result := True;
end;

function InitializeUninstall(): Boolean;
begin
  StopRunningApp();
  Result := True;
end;
