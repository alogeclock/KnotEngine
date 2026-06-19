@echo off
setlocal

set NO_PAUSE=0
for %%A in (%*) do (
    if /I "%%~A"=="--no-pause" set NO_PAUSE=1
)

"%~dp0\Scripts\python\python.exe" "%~dp0\Scripts\GenerateDependencies.py" %*
set RESULT=%ERRORLEVEL%

if "%NO_PAUSE%"=="0" pause
exit /b %RESULT%
