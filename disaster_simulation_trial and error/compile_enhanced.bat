@echo off
echo Compiling Enhanced Disaster Response Simulation...
g++ -std=c++11 -O2 -o disaster_enhanced.exe disaster_response_enhanced.cpp
if %errorlevel% == 0 (
    echo ✅ COMPILATION SUCCESSFUL!
    echo 🚀 Starting Enhanced Disaster Response Simulation...
    disaster_enhanced.exe
) else (
    echo ❌ Compilation failed!
    echo Please check the code for errors.
)
pause