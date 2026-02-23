; DimaChordJoystick-Setup.iss
; Inno Setup 6.7.1 installer for ChordJoystick VST3 v1.0.0
; Place this file in the installer/ subdirectory of the project root.

[Setup]
AppName=ChordJoystick
AppVersion=1.0.0
AppPublisher=Dima Xhavid
AppPublisherURL=https://gumroad.com
OutputBaseFilename=DimaChordJoystick-Setup
OutputDir=Output
DefaultDirName={commoncf64}\VST3\ChordJoystick.vst3
LicenseFile=..\LICENSE.txt
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
DisableDirPage=yes
Compression=lzma
SolidCompression=yes
UninstallDisplayName=ChordJoystick VST3

[Files]
; Copies the entire .vst3 bundle (directory tree) to the standard VST3 system path.
; Source ends in \* so Inno Setup recurses into the directory.
; DestDir creates ChordJoystick.vst3\ at the destination.
Source: "..\build\ChordJoystick_artefacts\Release\VST3\ChordJoystick.vst3\*"; \
    DestDir: "{commoncf64}\VST3\ChordJoystick.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
; Ensure the whole .vst3 bundle directory is removed on uninstall.
Type: filesandordirs; Name: "{commoncf64}\VST3\ChordJoystick.vst3"

; -- Code-signing placeholder --------------------------------------------------
; Uncomment and fill in when a certificate is acquired:
; [Code]
; procedure CurStepChanged(CurStep: TSetupStep);
; var ResultCode: Integer;
; begin
;   if CurStep = ssPostInstall then begin
;     Exec('signtool.exe',
;       'sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /a ' +
;       '"' + ExpandConstant('{commoncf64}\VST3\ChordJoystick.vst3\Contents\x86_64-win\ChordJoystick.vst3"'),
;       '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
;   end;
; end;
