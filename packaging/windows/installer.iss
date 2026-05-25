; Inno Setup script for xyz-profiler.
; Invoked from CI as:
;   ISCC /DAppVersion=x.y.z /DStagedRoot=<dir> /DAppIconFile=<path-to-ico> installer.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StagedRoot
  #error "StagedRoot must be provided via /D"
#endif
#ifndef AppIconFile
  #error "AppIconFile must be provided via /D"
#endif

#define AppName        "xyz-profiler"
#define AppPublisher   "RH"
#define AppPublisherURL "https://hadler.me"
#define AppExeName     "xyz-profiler.exe"
; Stable AppId — keep this constant across releases so upgrades replace
; the previous install instead of producing a second Apps & Features entry.
#define AppId          "{{A7F3D9E2-5B81-4C7A-9F4D-2E8A6C3B1D75}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppPublisherURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputBaseFilename=xyz-profiler-{#AppVersion}-windows-x64-setup
OutputDir=.
SetupIconFile={#AppIconFile}
UninstallDisplayIcon={app}\{#AppExeName}
; Show GPL-3.0 text as the installer license page. Path is relative to
; this .iss file's location (packaging/windows/), so the repo-root
; LICENSE is two dirs up.
LicenseFile=..\..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german";  MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StagedRoot}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}";                              Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}";        Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";                        Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
