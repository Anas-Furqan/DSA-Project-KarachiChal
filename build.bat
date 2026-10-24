@echo off
rem KarachiChal build script for Windows (MinGW-w64 g++ with posix threads)
rem   build.bat         -> build server  (build\karachichal.exe)
rem   build.bat test    -> build + run tests
rem   build.bat run     -> build + run server on http://localhost:8080
rem   build.bat clean
rem Set CXX to use a specific compiler, e.g.  set CXX=C:\mingw64\bin\g++.exe

setlocal EnableDelayedExpansion
if "%CXX%"=="" set CXX=g++
set FLAGS=-std=c++17 -O2 -Wall -Wextra -Ibackend/include -Ibackend
set LIBS=-static -lws2_32

if /I "%1"=="clean" (
    if exist build rmdir /s /q build
    echo cleaned
    exit /b 0
)

if not exist build mkdir build

set LIB_SRC=
for %%f in (backend\ds\*.cpp backend\core\*.cpp) do set LIB_SRC=!LIB_SRC! %%f

if /I "%1"=="test" goto :test

set SERVER_SRC=%LIB_SRC%
for %%f in (backend\api\*.cpp) do set SERVER_SRC=!SERVER_SRC! %%f
set SERVER_SRC=%SERVER_SRC% backend\main.cpp

echo building server...
"%CXX%" %FLAGS% %SERVER_SRC% -o build\karachichal.exe %LIBS%
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
echo built build\karachichal.exe
if /I "%1"=="run" build\karachichal.exe
exit /b 0

:test
echo building tests...
"%CXX%" %FLAGS% %LIB_SRC% tests\test_main.cpp -o build\karachichal_tests.exe %LIBS%
if errorlevel 1 (
    echo BUILD FAILED
    exit /b 1
)
build\karachichal_tests.exe
exit /b %errorlevel%
