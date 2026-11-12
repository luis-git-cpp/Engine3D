@echo off
set PATH=C:\raylib\w64devkit\bin;%PATH%
g++ *.cpp -o Engine3D.exe -std=c++17 -O2 -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm -pthread && Engine3D.exe
if errorlevel 1 pause
Engine3D.exe
pause