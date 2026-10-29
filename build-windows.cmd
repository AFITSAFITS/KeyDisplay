@echo off
rem SPDX-License-Identifier: MIT
setlocal
pushd "%~dp0"
if not defined CXX set "CXX=g++"
"%CXX%" -std=c++17 -Wall -Wextra -Werror -O2 -static -s -mwindows main.cpp -o KeyDisplay.exe -lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid
if errorlevel 1 (popd & exit /b 1)
popd
exit /b 0
