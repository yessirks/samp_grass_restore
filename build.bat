@echo off
rem Builds GrassRestore.asi (32-bit) with Zig's bundled clang and copies it into the game folder.
setlocal
cd /d "%~dp0"
set GAME=%~dp0..\Grand Theft Auto San Andreas
where zig >nul 2>nul
if errorlevel 1 (
  echo zig.exe not found on PATH. Install it with:  winget install -e --id zig.zig
  exit /b 1
)
if not exist build mkdir build
zig c++ -target x86-windows-gnu -std=c++17 -shared -O2 -fno-exceptions -fno-rtti -s -Wall ^
  -o build\GrassRestore.asi src\main.cpp -luser32 -lkernel32
if errorlevel 1 exit /b 1
del /q build\*.lib build\*.pdb 2>nul
echo Built build\GrassRestore.asi
if exist "%GAME%\gta_sa.exe" (
  tasklist /fi "imagename eq gta_sa.exe" | find /i "gta_sa.exe" >nul
  if not errorlevel 1 (
    echo gta_sa.exe is running - close the game, then run build.bat again to install.
  ) else (
    copy /y build\GrassRestore.asi "%GAME%\GrassRestore.asi" >nul
    if not exist "%GAME%\GrassRestore.ini" copy /y GrassRestore.ini "%GAME%\GrassRestore.ini" >nul
    echo Installed into "%GAME%"
  )
)
