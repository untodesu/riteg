@echo off
cmake -B build32 -A Win32
if %errorlevel% neq 0 exit /b %errorlevel%
cmake --build build32 --config Release --parallel
if %errorlevel% neq 0 exit /b %errorlevel%
