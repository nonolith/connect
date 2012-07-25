!addplugindir lib/win/nsi
!include "MUI2.nsh"

;SetCompressor /SOLID /FINAL lzma

Name "Nonolith Connect" ; Installer Name

RequestExecutionLevel highest ; Hope that the user is an admin and can install stuff

OutFile "nonolith-connect-setup.exe" ; Installer file name

; Set default installation directory, or look for a previous installation
InstallDir "$PROGRAMFILES\Nonolith Labs\Connect"
InstallDirRegKey HKCU "Software\Nonolith Labs\Connect" ""

!addplugindir ..\windows
!include "nsProcess.nsh"

!define chromepath "$LOCALAPPDATA\Google\Chrome\Application\chrome.exe"
!define chromeapp "--app=http://apps.nonolithlabs.com/#connect"

; Order the installation wizard pages
;!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Order the uninstallation wizard pages
;!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Installer language
!insertmacro MUI_LANGUAGE "English"

Section ;"Required"
	DetailPrint "Detecting existing instances"
	${nsProcess::KillProcess} "nonolith-connect.exe"  $R0
	sleep 2000

	; Copy all files
	SetOutPath $INSTDIR
	File ..\nonolith-connect.exe
	File ..\..\lib\win\wdi-simple.exe
	File nonolith.ico

	; Generate uninstaller, see Uninstall section
	WriteUninstaller $INSTDIR\uninstaller.exe

	; Write information for Windows Add/Remove Software
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\nonolith-connect" "DisplayName" "Nonolith Connect"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\nonolith-connect" "UninstallString" "$\"$INSTDIR\uninstaller.exe$\""
	WriteRegStr HKCU "Software\nonolith-connect" "" $INSTDIR

SectionEnd

Section "Install USB Drivers" SecUSB
	SetOutPath $INSTDIR
	
	DetailPrint "Installing USB Driver for CEE (Running $INSTDIR\wdi-simple.exe)"
	nsExec::ExecToLog '"$INSTDIR\wdi-simple.exe" -t 0 --name "Nonolith Labs CEE" --vid 0x59e3 --pid 0xCEE1 --progressbar=$HWNDPARENT'
	
	DetailPrint "Installing USB Driver for Firmware Update (Running $INSTDIR\wdi-simple.exe)"
	nsExec::ExecToLog '"$INSTDIR\wdi-simple.exe" -t 0 --name "Nonolith Labs CEE Firmware Update" --vid 0x59e3 --pid 0xBBBB --progressbar=$HWNDPARENT'
SectionEnd

Section "Run Nonolith Connect on startup" SecStartup

	CreateShortCut "$SMPROGRAMS\Startup\Nonolith Connect.lnk" "$INSTDIR\nonolith-connect.exe" "" "$INSTDIR\nonolith.ico"

SectionEnd

Section "Start Menu Entry" SecStartMenu

	IfFileExists "${chromepath}" +2 0
	MessageBox MB_OK "Google Chrome is not installed or was not found. You need Google Chrome to use Pixelpulse."
	
	SetOutPath "$SMPROGRAMS\Nonolith Labs"
	CreateShortCut "$SMPROGRAMS\Nonolith Labs\Nonolith Pixelpulse.lnk" "${chromepath}" "${chromeapp}" "$INSTDIR\nonolith.ico"
	CreateShortCut "$SMPROGRAMS\Nonolith Labs\Start Nonolith Connect.lnk" $INSTDIR\nonolith-connect.exe "" "$INSTDIR\nonolith.ico"
	CreateShortCut "$SMPROGRAMS\Nonolith Labs\Uninstall Nonolith Connect.lnk" $INSTDIR\uninstaller.exe
	
SectionEnd

Section "Desktop Shortcut" SecDesktopShortcut

	SetOutPath $DESKTOP
	CreateShortCut "$DESKTOP\Nonolith Pixelpulse.lnk" "${chromepath}" "${chromeapp}" "$INSTDIR\nonolith.ico"

SectionEnd

Section
	
	Exec "$INSTDIR\nonolith-connect.exe"
	
SectionEnd

; Describe the sections here
LangString DESC_DesktopShortcut ${LANG_ENGLISH} "A shortcut to Nonolith Pixelpulse on your desktop"
LangString DESC_StartMenu ${LANG_ENGLISH} "A shortcut to Nonolith Pixelpulse on in your start menu"
LangString DESC_Startup ${LANG_ENGLISH} "Start the small Nonolith Connect server automatically"
LangString DESC_USB ${LANG_ENGLISH} "USB Drivers for CEE"

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecDesktopShortcut} $(DESC_DesktopShortcut)
!insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} $(DESC_StartMenu)
!insertmacro MUI_DESCRIPTION_TEXT ${SecStartup} $(DESC_Startup)
!insertmacro MUI_DESCRIPTION_TEXT ${SecUSB} $(DESC_USB)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall" ; Uninstaller actions
	DetailPrint "Closing server process"
	${nsProcess::KillProcess} "nonolith-connect.exe" $R0
	sleep 5000

	; Remove installation completely
	Delete $INSTDIR\*.*
	RMDir /r $INSTDIR

	; Remove created shortcuts
	Delete "$SMPROGRAMS\Nonolith Labs\Start Nonolith Connect.lnk"
	Delete "$SMPROGRAMS\Nonolith Labs\Nonolith Pixelpulse.lnk"
	Delete "$SMPROGRAMS\Startup\Nonolith Connect.lnk"
	RMDir /r "$SMPROGRAMS\Nonolith Labs"
	Delete "$DESKTOP\Nonolith Pixelpulse.lnk"

	; Remove from Windows's Add/Remove Programs
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\nonolith-connect"

SectionEnd
