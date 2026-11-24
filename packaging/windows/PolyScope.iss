; Inno Setup script. AppVersion and StageDir are passed on the iscc command line (/DAppVersion=... /DStageDir=...).
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #define StageDir "..\..\stage"
#endif

[Setup]
AppId={{6F0C6B52-3B5E-4C1E-9D43-7A1B1E2C9A10}
AppName=PolyScope
AppVersion={#AppVersion}
AppPublisher=PolyScope
DefaultDirName={autopf}\PolyScope
DefaultGroupName=PolyScope
UninstallDisplayIcon={app}\PolyScope.exe
SetupIconFile=PolyScope.ico
OutputDir=..\..
OutputBaseFilename=PolyScope-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-user install by default so no admin prompt; the user can still pick "all users".
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion createallsubdirs

[Icons]
Name: "{group}\PolyScope"; Filename: "{app}\PolyScope.exe"
Name: "{autodesktop}\PolyScope"; Filename: "{app}\PolyScope.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\PolyScope.exe"; Description: "Launch PolyScope"; Flags: nowait postinstall skipifsilent
