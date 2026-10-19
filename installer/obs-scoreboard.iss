; Windows installer for obs-scoreboard (Inno Setup 6).
;
; It installs the tree `cmake --install` lays out -- the same one the release
; zip carries -- into the one folder OBS searches on Windows, in BOTH layouts:
;   C:\ProgramData\obs-studio\plugins\obs-scoreboard\
;     obs-scoreboard.dll            OBS 33+ layout (added here)
;     bin\64bit\obs-scoreboard.dll  legacy layout, OBS 32 and earlier
;     data\                          shared by both
; OBS 33 loads the first and skips the second as a duplicate; OBS 32 and
; earlier only look at the second. Portable OBS is not covered: use the zip.
;
; Built by CI (the Windows job in .github/workflows/build-project.yaml):
;   ISCC /DAppVersion=1.0.1-beta2 /DAppNumVersion=1.0.1 /DPkgDir=<install tree>
;        /O<output dir> /F<output name> installer\obs-scoreboard.iss

#ifndef AppVersion
  #error Pass the full plugin version: /DAppVersion=x.y.z[-betaN]
#endif
#ifndef AppNumVersion
  #error Pass the numeric version for the file properties: /DAppNumVersion=x.y.z
#endif
#ifndef PkgDir
  #error Pass the install tree holding obs-scoreboard\: /DPkgDir=<dir>
#endif

[Setup]
; Never change the AppId: it is how an upgrade finds the previous install.
AppId={{ADB6AEBC-21CC-42A6-9664-1199C43E4AD8}
AppName=ScoreBoard for OBS
AppVersion={#AppVersion}
AppVerName=ScoreBoard for OBS {#AppVersion}
AppPublisher=Angelo Ruggieri
AppPublisherURL=https://github.com/angeloruggieridj/obs-scoreboard
AppSupportURL=https://github.com/angeloruggieridj/obs-scoreboard/issues
AppUpdatesURL=https://github.com/angeloruggieridj/obs-scoreboard/releases
VersionInfoVersion={#AppNumVersion}
DefaultDirName={commonappdata}\obs-studio\plugins\obs-scoreboard
DisableDirPage=yes
DisableProgramGroupPage=yes
; ProgramData subfolders can belong to another account; one clear elevation
; prompt beats an install that fails halfway.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; OUTSIDE the plugin folder: a reinstall that replaces the plugin folder whole
; must not delete the uninstaller. OBS never looks in this folder.
UninstallFilesDir={commonappdata}\obs-scoreboard\uninstall
UninstallDisplayName=ScoreBoard for OBS
; OBS is checked for explicitly below; the Restart Manager would only add a
; second, vaguer prompt about the same thing.
CloseApplications=no
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[CustomMessages]
english.ObsRunning=OBS Studio is running. Close it, then choose Retry: a plugin that OBS has loaded cannot be replaced.
italian.ObsRunning=OBS Studio è in esecuzione. Chiudilo, poi scegli Riprova: un plugin caricato da OBS non può essere sostituito.

[Messages]
english.FinishedLabel=Setup has installed [name] into OBS's plugin folder. Start OBS Studio and open it from Docks.
italian.FinishedLabel=[name] è stato installato nella cartella dei plugin di OBS. Avvia OBS Studio e aprilo dal menu Pannelli.

[InstallDelete]
; Upgrades replace the data folder whole, so a file a newer version dropped
; does not linger from an older one.
Type: filesandordirs; Name: "{app}\data"

[Files]
Source: "{#PkgDir}\obs-scoreboard\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; The OBS 33 layout: the same binary, directly in the plugin folder.
Source: "{#PkgDir}\obs-scoreboard\bin\64bit\obs-scoreboard.dll"; DestDir: "{app}"; Flags: ignoreversion

[UninstallDelete]
; Also what a zip install or the in-app updater left in the plugin folder
; (update backups included). Settings live in OBS's plugin_config folder and
; recordings in the session folder, not here, and are kept.
Type: filesandordirs; Name: "{app}"
Type: dirifempty; Name: "{commonappdata}\obs-scoreboard"

[Code]
function IsObsRunning(): Boolean;
var
  Locator, Wmi, Processes: Variant;
begin
  Result := False;
  try
    { Pascal Script cannot call a method on a call's result: one step each. }
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Wmi := Locator.ConnectServer('.', 'root\CIMV2');
    Processes := Wmi.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name = ''obs64.exe''');
    Result := Processes.Count > 0;
  except
    { WMI unavailable: do not block the install on a check that cannot run. }
  end;
end;

{ Retry until OBS is closed, or give up. Silent installs get IDCANCEL, so an
  unattended run fails cleanly instead of looping. }
function WaitForObsClosed(): Boolean;
begin
  Result := True;
  while IsObsRunning() do
    if SuppressibleMsgBox(CustomMessage('ObsRunning'), mbError, MB_RETRYCANCEL, IDCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
end;

function InitializeSetup(): Boolean;
begin
  Result := WaitForObsClosed();
end;

function InitializeUninstall(): Boolean;
begin
  Result := WaitForObsClosed();
end;

