@echo off
build.windows.x32.bat
if %errorlevel% neq 0 exit /b %errorlevel%
cpack --config build32/CPackConfig.cmake -G ZIP
if %errorlevel% neq 0 exit /b %errorlevel%
