@echo off
REM Run CMake to generate Visual Studio 2022 project files

REM Change to the directory containing the CMakeLists.txt file
cd /d "%~dp0"

REM Set the CMake command
set CMAKE_CMD=cmake -G "Visual Studio 17 2022" -A x64 -B ./build .

REM Echo and run the CMake command
echo %CMAKE_CMD%
%CMAKE_CMD%

REM Pause to see any output or errors
pause
