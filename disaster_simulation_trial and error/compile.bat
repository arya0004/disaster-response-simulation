@echo off
echo Compiling Disaster Response Simulation...
g++ -std=c++11 -O2 -o disaster_sim.exe disaster_response.cpp
if %errorlevel% == 0 (
    echo ✅ COMPILATION SUCCESSFUL!
    echo 🚀 Starting Advanced Disaster Response Simulation...
    disaster_sim.exe
) else (
    echo ❌ Compilation failed!
    echo Please make sure you have MinGW installed and in PATH.
)
pause