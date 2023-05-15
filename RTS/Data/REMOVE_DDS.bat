@echo off
setlocal

set "rootDir=%~dp0"

echo Deleting .dds files...
for /r "%rootDir%" %%F in (*.dds) do (
    echo Deleting "%%F"
    del "%%F" /q
)

echo All .dds files deleted.

endlocal