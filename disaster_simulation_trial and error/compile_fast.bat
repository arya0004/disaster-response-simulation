@echo off
echo Compiling Fast Disaster Response Simulation...
g++ -std=c++11 -O3 -o disaster_fast.exe disaster_simulation_fast.cpp
if %errorlevel% == 0 (
    echo ✅ Compilation successful! 
    echo 🚀 Running simulation...
    disaster_fast.exe
) else (
    echo ❌ Compilation failed!
)
pause