@echo off
setlocal

set ROOT=%~dp0
set DIST=%ROOT%dist

if not exist "%DIST%" mkdir "%DIST%"

windres "%ROOT%ClickBait.rc" -O coff -o "%DIST%\ClickBait.res"
if errorlevel 1 exit /b 1

g++ ^
  -std=gnu++14 ^
  -O2 ^
  -s ^
  -mwindows ^
  "%ROOT%ClickBait.cpp" ^
  "%DIST%\ClickBait.res" ^
  -o "%DIST%\ClickBait.exe" ^
  -lgdiplus ^
  -lshell32 ^
  -lshlwapi ^
  -lgdi32 ^
  -luser32

if errorlevel 1 exit /b 1

echo Built "%DIST%\ClickBait.exe"
