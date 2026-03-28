@echo off
echo Compiling Fixed Disaster Response Simulation...
g++ -std=c++11 -O2 -o disaster_sim.exe disaster_response_fixed.cpp
if %errorlevel% == 0 (
    echo ✅ COMPILATION SUCCESSFUL!
    echo 🚀 Starting Advanced Disaster Response Simulation...
    disaster_sim.exe
) else (
    echo ❌ Compilation failed!
    echo Please check the code for errors.
)
pause