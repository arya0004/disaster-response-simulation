@echo off
echo Compiling Enhanced Disaster Response Simulation...
g++ -std=c++11 -O2 -o dis_res.exe dis_res.cpp
if %errorlevel% == 0 (
    echo ✅ COMPILATION SUCCESSFUL!
    echo 🚀 Starting Enhanced Disaster Response Simulation...
    dis_res.exe
) else (
    echo ❌ Compilation failed!
    echo Please check the code for errors.
)
pause