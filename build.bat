@echo off
set PATH=C:\raylib\w64devkit\bin;%PATH%
g++ *.cpp -o Engine3D.exe -std=c++17 -O2 -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm && Engine3D.exe