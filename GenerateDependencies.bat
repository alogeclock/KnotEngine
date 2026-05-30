@echo off
setlocal

"%~dp0KnotEngine\Build\Scripts\python\python.exe" "%~dp0KnotEngine\Build\Scripts\GenerateDependencies.py" %*
set RESULT=%ERRORLEVEL%

if not "%~1"=="--no-pause" pause
exit /b %RESULT%
