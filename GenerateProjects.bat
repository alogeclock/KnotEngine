@echo off
setlocal

set NO_PAUSE=0
set NO_OPEN=0
for %%A in (%*) do (
    if /I "%%~A"=="--no-pause" set NO_PAUSE=1
    if /I "%%~A"=="--no-open" set NO_OPEN=1
)

"%~dp0\Scripts\python\python.exe" "%~dp0\Scripts\GenerateProjects.py" %*
set RESULT=%ERRORLEVEL%

if "%NO_PAUSE%"=="1" goto Finish
if not "%RESULT%"=="0" goto Pause
if "%NO_OPEN%"=="1" goto Pause
goto Finish

:Pause
pause

:Finish
exit /b %RESULT%
