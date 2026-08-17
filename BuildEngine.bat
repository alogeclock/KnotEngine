@echo off
setlocal

set "CONFIGURATION="
set "NO_PAUSE=0"

:ParseArguments
if "%~1"=="" goto ArgumentsParsed
if /I "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
) else if not defined CONFIGURATION (
    set "CONFIGURATION=%~1"
) else (
    echo [ERROR] Unexpected argument: %~1
    set "RESULT=2"
    goto Finish
)
shift
goto ParseArguments

:ArgumentsParsed
if not defined CONFIGURATION set "CONFIGURATION=Development"

if /I "%CONFIGURATION%"=="Debug" goto ValidateBuild
if /I "%CONFIGURATION%"=="Development" goto ValidateBuild
if /I "%CONFIGURATION%"=="Shipping" goto ValidateBuild
if /I "%CONFIGURATION%"=="All" goto ValidateBuild

echo [ERROR] Unknown configuration: %CONFIGURATION%
echo Usage: BuildEngine.bat [Debug^|Development^|Shipping^|All] [--no-pause]
set "RESULT=2"
goto Finish

:ValidateBuild
set "CMAKE_EXE=%~dp0KnotEngine\Intermediate\Tools\cmake\bin\cmake.exe"
set "BUILD_DIR=%~dp0KnotEngine\Build\VS2022-x64"

if not exist "%CMAKE_EXE%" (
    echo [ERROR] CMake executable was not found: %CMAKE_EXE%
    echo Run GenerateProjects.bat first.
    set "RESULT=1"
    goto Finish
)

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [ERROR] Generated CMake build files were not found: %BUILD_DIR%
    echo Run GenerateProjects.bat first.
    set "RESULT=1"
    goto Finish
)

if /I "%CONFIGURATION%"=="All" goto BuildAll

:BuildOne
call :RunBuild "%CONFIGURATION%"
set "RESULT=%ERRORLEVEL%"
goto Finish

:BuildAll
for %%C in (Debug Development Shipping) do (
    call :RunBuild "%%C"
    if errorlevel 1 (
        set "RESULT=1"
        goto Finish
    )
)
set "RESULT=0"
goto Finish

:Finish
if "%NO_PAUSE%"=="0" pause
exit /b %RESULT%

:RunBuild
echo ^> "%CMAKE_EXE%" --build "%BUILD_DIR%" --config "%~1"
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config "%~1"
exit /b %ERRORLEVEL%
