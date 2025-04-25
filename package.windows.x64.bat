@echo off
cmake -B build64 -A x64
if %errorlevel% neq 0 exit /b %errorlevel%
cmake --build build64 --config Release --parallel
if %errorlevel% neq 0 exit /b %errorlevel%
cpack --config build64/CPackConfig.cmake -G ZIP
if %errorlevel% neq 0 exit /b %errorlevel%
