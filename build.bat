@echo off
rem KarachiChal build script for Windows (needs MinGW-w64 g++ with posix threads)
rem   build.bat         -> build server  (build\karachichal.exe)
rem   build.bat test    -> build + run tests
rem   build.bat run     -> build + run server on http://localhost:8080
rem   build.bat clean
rem Set CXX to use a specific compiler, e.g.  set CXX=C:\mingw64\bin\g++.exe

setlocal EnableDelayedExpansion
set FLAGS=-std=c++17 -O2 -Wall -Wextra -Ibackend/include -Ibackend
set LIBS=-static -lws2_32

if /I "%1"=="clean" (
    if exist build rmdir /s /q build
    echo cleaned
    exit /b 0
)

if not exist build mkdir build

rem find a g++ that supports std::thread and std::mutex (old MinGW 6.x does not)
> build\thread_check.cpp echo #include ^<thread^>
>> build\thread_check.cpp echo #include ^<mutex^>
>> build\thread_check.cpp echo int main() { std::mutex m; std::lock_guard^<std::mutex^> g(m); std::thread t; return 0; }

set FOUND=
if not "%CXX%"=="" (
    call :check "%CXX%"
    goto :picked
)
for %%c in ("D:\tools\mingw64\bin\g++.exe" "C:\mingw64\bin\g++.exe" "C:\msys64\ucrt64\bin\g++.exe" "C:\msys64\mingw64\bin\g++.exe" "C:\TDM-GCC-64\bin\g++.exe") do (
    if not defined FOUND if exist %%c call :check %%c
)
if not defined FOUND call :check g++

:picked
del /q build\thread_check.* >nul 2>&1
if not defined FOUND (
    echo.
    echo ERROR: no C++17 compiler with thread support was found.
    if not "%CXX%"=="" echo        CXX=%CXX% cannot compile std::thread / std::mutex.
    echo        Old MinGW ^(GCC 6.x, C:\MinGW^) does not support threads, which the HTTP server needs.
    echo        Install MinGW-w64 with posix threads, for example from https://winlibs.com
    echo        ^(GCC 13 or newer, UCRT, posix threads^), unzip it to C:\mingw64 and run build.bat again,
    echo        or point to it with:  set CXX=C:\path\to\mingw64\bin\g++.exe
    exit /b 1
)
set CXX=!FOUND!
echo using compiler: %CXX%

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

:check
"%~1" -std=c++17 build\thread_check.cpp -o build\thread_check.exe >nul 2>&1
if not errorlevel 1 set FOUND=%~1
exit /b 0
