md PACKAGE
md PACKAGE\data
xcopy /y ..\build\x64\Debug\GameClient.exe PACKAGE
xcopy /y ..\build\x64\Debug\GameClient.pdb PACKAGE
xcopy /y *.dll PACKAGE
xcopy /y /s data PACKAGE\data
pause