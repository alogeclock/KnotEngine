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

if /I "%CONFIGURATION%"=="Debug" goto BuildOne
if /I "%CONFIGURATION%"=="Development" goto BuildOne
if /I "%CONFIGURATION%"=="Shipping" goto BuildOne
if /I "%CONFIGURATION%"=="All" goto BuildAll

echo [ERROR] Unknown configuration: %CONFIGURATION%
echo Usage: BuildEngine.bat [Debug^|Development^|Shipping^|All] [--no-pause]
set "RESULT=2"
goto Finish

:BuildOne
call "%~dp0GenerateProjects.bat" --no-pause --build --config "%CONFIGURATION%"
set "RESULT=%ERRORLEVEL%"
goto Finish

:BuildAll
call "%~dp0GenerateProjects.bat" --no-pause --build-all
set "RESULT=%ERRORLEVEL%"

:Finish
if "%NO_PAUSE%"=="0" pause
exit /b %RESULT%
