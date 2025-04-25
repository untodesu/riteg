@echo off
build.windows.x64.bat
if %errorlevel% neq 0 exit /b %errorlevel%
cpack --config build64/CPackConfig.cmake -G ZIP
if %errorlevel% neq 0 exit /b %errorlevel%
