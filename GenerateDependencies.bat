@echo off
setlocal

"%~dp0\Build\Scripts\python\python.exe" "%~dp0\Scripts\GenerateDependencies.py" %*
set RESULT=%ERRORLEVEL%

if not "%~1"=="--no-pause" pause
exit /b %RESULT%
