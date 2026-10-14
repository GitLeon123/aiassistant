; Compile with scripts/build-installer.ps1 or the aiassistant_installer CMake target.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\msvc-x64\Release"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif
#ifndef WebViewBootstrapper
  #define WebViewBootstrapper "..\build\installer-deps\MicrosoftEdgeWebview2Setup.exe"
#endif
#ifndef ProductId
  #define ProductId "{{CF8BCBA9-D651-4CC0-ABDC-F0F04ADFB740}"
#endif

[Setup]
AppId={#ProductId}
AppName=AIAssistant
AppVersion={#AppVersion}
AppPublisher=AIAssistant
DefaultDirName={localappdata}\Programs\AIAssistant
DefaultGroupName=AIAssistant
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.18362
OutputDir={#OutputDir}
OutputBaseFilename=AIAssistant-{#AppVersion}-Setup
SetupIconFile=..\resources\app.ico
UninstallDisplayIcon={app}\AIAssistant.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Tasks]
Name: "autostart"; Description: "Start AIAssistant when I sign in to Windows"; Flags: unchecked
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#BuildDir}\AIAssistant.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\ui\*"; DestDir: "{app}\ui"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#WebViewBootstrapper}"; Flags: dontcopy

[Icons]
Name: "{group}\AIAssistant"; Filename: "{app}\AIAssistant.exe"
Name: "{group}\Uninstall AIAssistant"; Filename: "{uninstallexe}"
Name: "{autodesktop}\AIAssistant"; Filename: "{app}\AIAssistant.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "AIAssistant"; ValueData: """{app}\AIAssistant.exe"" --background"; Tasks: autostart

[Run]
Filename: "{app}\AIAssistant.exe"; Description: "Open AIAssistant and connect an AI"; Flags: nowait postinstall skipifsilent

[Code]
const
  RuntimeKey = 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';

function RuntimeAt(Root: Integer): Boolean;
var
  Version: String;
begin
  Result := RegQueryStringValue(Root, RuntimeKey, 'pv', Version) and
    (Version <> '') and (Version <> '0.0.0.0');
end;

function HasWebViewRuntime: Boolean;
begin
  Result := RuntimeAt(HKCU) or RuntimeAt(HKLM32) or RuntimeAt(HKLM64);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
begin
  Result := '';
  if not HasWebViewRuntime then begin
    WizardForm.StatusLabel.Caption := 'Installing the Microsoft display component…';
    ExtractTemporaryFile('MicrosoftEdgeWebview2Setup.exe');
    if not Exec(ExpandConstant('{tmp}\MicrosoftEdgeWebview2Setup.exe'), '/silent /install', '',
      SW_HIDE, ewWaitUntilTerminated, ExitCode) or not HasWebViewRuntime then
      Result := 'AIAssistant needs Microsoft Edge WebView2 to display its interface. Connect to the internet and try again, or install it from https://developer.microsoft.com/microsoft-edge/webview2/ and rerun Setup.';
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Command: String;
begin
  if (CurStep = ssPostInstall) and not WizardIsTaskSelected('autostart') then
    if RegQueryStringValue(HKCU, RunKey, 'AIAssistant', Command) and
      (CompareText(Command, ExpandConstant('"{app}\AIAssistant.exe" --background')) = 0) then
      RegDeleteValue(HKCU, RunKey, 'AIAssistant');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  if CurUninstallStep = usUninstall then
    if RegQueryStringValue(HKCU, RunKey, 'AIAssistant', Command) and
      (CompareText(Command, ExpandConstant('"{app}\AIAssistant.exe" --background')) = 0) then
      RegDeleteValue(HKCU, RunKey, 'AIAssistant');
  { Settings, Credential Manager keys and the shared WebView2 Runtime are retained. }
end;
