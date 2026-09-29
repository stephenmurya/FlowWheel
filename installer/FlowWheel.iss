#define MyAppName "FlowWheel"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "FlowWheel"
#define MyAppExeName "FlowWheel.exe"

[Setup]
AppId={{D6DDA451-5C43-4B3B-A9B0-4E20C17B4F06}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\FlowWheel
DefaultGroupName={#MyAppName}
OutputDir=..\artifacts\installer
OutputBaseFilename=FlowWheel-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern

[Files]
Source: "{#SourceDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch FlowWheel"; Flags: nowait postinstall skipifsilent
