@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ------------------------------------------------------------
rem PackageEngine.bat
rem
rem Usage:
rem   PackageEngine.bat
rem   PackageEngine.bat "C:\Path\To\ProjectRoot"
rem   PackageEngine.bat "C:\Path\To\ProjectRoot" "C:\Path\Out.zip"
rem ------------------------------------------------------------

if "%~1"=="" (
    set "PROJECT_ROOT=%~dp0"
) else (
    set "PROJECT_ROOT=%~f1"
)

if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

for /f "usebackq delims=" %%D in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "[Environment]::GetFolderPath('Desktop')"`) do set "DESKTOP_DIR=%%D"
if not defined DESKTOP_DIR set "DESKTOP_DIR=%USERPROFILE%\Desktop"

for %%I in ("%PROJECT_ROOT%") do set "PROJECT_NAME=%%~nxI"
if not defined PROJECT_NAME set "PROJECT_NAME=KnotEngine"

if "%~2"=="" (
    set "OUTPUT_ZIP=%DESKTOP_DIR%\%PROJECT_NAME%.zip"
) else (
    set "OUTPUT_ZIP=%~f2"
)

set "STAGING=%TEMP%\%PROJECT_NAME%RANDOM%%RANDOM%"
set "COPIED_ANY=0"

echo.
echo ========================================
echo  Package Source
echo ========================================
echo Project Root : "%PROJECT_ROOT%"
echo Output Zip   : "%OUTPUT_ZIP%"
echo.

if exist "%STAGING%" rmdir /s /q "%STAGING%"
mkdir "%STAGING%" >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Failed to create staging directory:
    echo         "%STAGING%"
    echo.
    exit /b 1
)

echo [STEP] Copying Docs and Scripts...
call :CopyDir "Docs"
call :CopyDir "Scripts"

echo [STEP] Copying engine source...
call :CopyDir "KnotEngine\Source"
call :CopyDir "KnotEngine\Shaders"
call :CopyDir "KnotEngine\Settings"

echo [STEP] Copying KnotEngine project files...
call :CopyFile "KnotEngine\KnotEngine.vcxproj"
call :CopyFile "KnotEngine\KnotEngine.vcxproj.filters"
call :CopyFile "KnotEngine\KnotEngine.vcxproj.user"
call :CopyFile "KnotEngine\main.cpp"
call :CopyFile "KnotEngine\pch.cpp"
call :CopyFile "KnotEngine\pch.h"

echo [STEP] Copying root solution and scripts...
call :CopyRootPattern "*.sln"
call :CopyRootPattern "*.bat"

if "%COPIED_ANY%"=="0" (
    echo.
    echo [ERROR] No files were copied.
    echo         Check project root: "%PROJECT_ROOT%"
    rmdir /s /q "%STAGING%" >nul 2>nul
    echo.
    exit /b 1
)

if exist "%OUTPUT_ZIP%" del /f /q "%OUTPUT_ZIP%"

for %%I in ("%OUTPUT_ZIP%") do (
    if not exist "%%~dpI" mkdir "%%~dpI" >nul 2>nul
)

echo [STEP] Creating zip...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGING%\*' -DestinationPath '%OUTPUT_ZIP%' -Force"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to create zip.
    rmdir /s /q "%STAGING%" >nul 2>nul
    echo.
    exit /b 1
)

rmdir /s /q "%STAGING%" >nul 2>nul

echo.
echo [OK] Done.
echo Created:
echo "%OUTPUT_ZIP%"
echo.

explorer.exe /select,"%OUTPUT_ZIP%"

exit /b 0


:CopyDir
set "REL=%~1"
set "SRC=%PROJECT_ROOT%\%REL%"
set "DST=%STAGING%\%REL%"

if not exist "%SRC%\" (
    echo   [WARN] Missing directory: %REL%
    exit /b 0
)

mkdir "%DST%" >nul 2>nul

robocopy "%SRC%" "%DST%" /E /XD .git .vs x64 Bin Binaries Intermediate Saved DerivedDataCache .pytest_cache __pycache__ /XF *.obj *.pdb *.ilk *.exe *.dll *.lib *.exp *.log *.tmp *.user /NFL /NDL /NJH /NJS /NC /NS /NP >nul

if %ERRORLEVEL% GEQ 8 (
    echo.
    echo [ERROR] Failed to copy directory:
    echo         "%REL%"
    rmdir /s /q "%STAGING%" >nul 2>nul
    echo.
    exit /b 1
)

set "COPIED_ANY=1"
exit /b 0


:CopyFile
set "REL=%~1"
set "SRC=%PROJECT_ROOT%\%REL%"
set "DST=%STAGING%\%REL%"

if not exist "%SRC%" (
    echo   [WARN] Missing file: %REL%
    exit /b 0
)

for %%I in ("%DST%") do mkdir "%%~dpI" >nul 2>nul

copy /y "%SRC%" "%DST%" >nul
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to copy file:
    echo         "%REL%"
    rmdir /s /q "%STAGING%" >nul 2>nul
    echo.
    exit /b 1
)

set "COPIED_ANY=1"
exit /b 0


:CopyRootPattern
set "PATTERN=%~1"
set "FOUND=0"

for %%F in ("%PROJECT_ROOT%\%PATTERN%") do (
    if exist "%%~fF" (
        set "FOUND=1"
        copy /y "%%~fF" "%STAGING%\%%~nxF" >nul
        if errorlevel 1 (
            echo.
            echo [ERROR] Failed to copy root file:
            echo         "%%~nxF"
            rmdir /s /q "%STAGING%" >nul 2>nul
            echo.
            exit /b 1
        )
        set "COPIED_ANY=1"
    )
)

if "%FOUND%"=="0" echo   [WARN] Missing root pattern: %PATTERN%
exit /b 0
