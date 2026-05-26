[Setup]
AppName=加权平均数的串口调试助手MAX
AppVersion=4.0.0
AppPublisher=串口调试助手
DefaultDirName={autopf}\串口调试助手MAX
DefaultGroupName=串口调试助手MAX
OutputDir=D:\studystudystudy\QT\output
OutputBaseFilename=串口调试助手MAX_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
SetupIconFile=
UninstallDisplayIcon={app}\test.exe
PrivilegesRequired=lowest

[Files]
Source: "D:\studystudystudy\QT\package\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\串口调试助手MAX"; Filename: "{app}\test.exe"
Name: "{group}\卸载串口调试助手MAX"; Filename: "{uninstallexe}"
Name: "{commondesktop}\串口调试助手MAX"; Filename: "{app}\test.exe"

[Run]
Filename: "{app}\test.exe"; Description: "启动串口调试助手MAX"; Flags: postinstall nowait skipifsilent
