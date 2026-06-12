@echo off
setlocal

set NO_PAUSE=0
if /I "%~1"=="--no-pause" set NO_PAUSE=1

"%~dp0\Scripts\python\python.exe" "%~dp0\Scripts\GenerateProjects.py" %*
set RESULT=%ERRORLEVEL%

if "%NO_PAUSE%"=="0" pause
exit /b %RESULT%
