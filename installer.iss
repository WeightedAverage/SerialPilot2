; 串口调试助手MAX 安装脚本
; 用 Inno Setup 7 编译

#define MyAppName "串口调试助手MAX"
#define MyAppVersion "4.0.0"
#define MyAppPublisher "WeightedAverage"
#define MyAppExeName "SerialPilot2.exe"
#define QtDir    "E:/Esoft/QT/Qt5.14.2/5.14.2/mingw73_64"
#define BuildDir "D:/studystudystudy/QT/SerialPilot2/SerialPilot2/build-SerialPilot2-Desktop_Qt_5_14_2_MinGW_64_bit-Debug/debug"
#define PkgDir   "D:/studystudystudy/QT/SerialPilot2/SerialPilot2/package"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=D:\SerialPilot2_output
OutputBaseFilename=SerialPilot2_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
VersionInfoVersion={#MyAppVersion}.0
WizardStyle=modern

[Files]
; 主程序
Source: "{#BuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt 核心 DLL
Source: "{#QtDir}\bin\Qt5Core.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtDir}\bin\Qt5Gui.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtDir}\bin\Qt5Widgets.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtDir}\bin\Qt5SerialPort.dll"; DestDir: "{app}"; Flags: ignoreversion

; MinGW 运行时
Source: "{#QtDir}\bin\libgcc_s_seh-1.dll";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtDir}\bin\libstdc++-6.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtDir}\bin\libwinpthread-1.dll";   DestDir: "{app}"; Flags: ignoreversion

; Qt 平台插件 (必须)
Source: "{#QtDir}\plugins\platforms\qwindows.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion

; 翻译文件
Source: "{#PkgDir}\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载{#MyAppName}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\加权平均数的串口调试助手MAX"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动{#MyAppName}"; Flags: postinstall nowait skipifsilent
