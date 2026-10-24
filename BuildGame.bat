
@echo off
setlocal

set "ROOT=%~dp0"
set "GAME_PROJECT=%ROOT%Source\Game\HitoriGame.vcxproj"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%GAME_PROJECT%" (
    echo [ERROR] HitoriGame project not found:
    echo "%GAME_PROJECT%"
    exit /b 1
)

if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio Locator not found.
    exit /b 2
)

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%I"
    goto :FoundMSBuild
)

echo [ERROR] MSBuild not found.
exit /b 3

:FoundMSBuild


echo [BUILD] HitoriGame - Release x64

"%MSBUILD%" "%GAME_PROJECT%" /t:Build /p:Configuration=Release /p:Platform=x64 /m
set "BUILD_RESULT=%ERRORLEVEL%"

if not "%BUILD_RESULT%"=="0" (
    echo [ERROR] Build failed. Exit code: %BUILD_RESULT%
    exit /b %BUILD_RESULT%
)

echo [SUCCESS] HitoriGame build completed.

if not exist "%ROOT%Assets" (
    echo [ERROR] Assets folder not found.
    exit /b 7
)

if not exist "%ROOT%Resources" (
    echo [ERROR] Resources folder not found.
    exit /b 8
)

set "GAME_EXE="

for /r "%ROOT%Build\Bin" %%F in (HitoriGame.exe) do (
    echo(%%~dpF | findstr /i "Release" >nul
    if not errorlevel 1 set "GAME_EXE=%%~fF"
)

if not defined GAME_EXE (
    echo [ERROR] Release HitoriGame.exe not found under Build\Bin.
    exit /b 9
)

set "PACKAGE_DIR=%ROOT%Build\Package\HitoriGame"

echo [PACKAGE] Preparing output folder...

if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"

if not exist "%PACKAGE_DIR%" (
    echo [ERROR] Failed to create package folder.
    exit /b 10
)

copy /Y "%GAME_EXE%" "%PACKAGE_DIR%\HitoriGame.exe" >nul
if errorlevel 1 (
    echo [ERROR] Failed to copy HitoriGame.exe.
    exit /b 11
)

xcopy "%ROOT%Assets" "%PACKAGE_DIR%\Assets\" /E /I /Y >nul
if errorlevel 1 (
    echo [ERROR] Failed to copy Assets.
    exit /b 12
)

xcopy "%ROOT%Resources" "%PACKAGE_DIR%\Resources\" /E /I /Y >nul
if errorlevel 1 (
    echo [ERROR] Failed to copy Resources.
    exit /b 13
)

set "SCENE_SOURCE=%ROOT%Intermediate\PackageStaging\Startup.scene"

if not exist "%SCENE_SOURCE%" (
    echo [ERROR] Staged scene not found: "%SCENE_SOURCE%"
    exit /b 14
)

if not exist "%PACKAGE_DIR%\Scene" (
    mkdir "%PACKAGE_DIR%\Scene"
)

copy /Y "%SCENE_SOURCE%" "%PACKAGE_DIR%\Scene\Startup.scene" >nul

if errorlevel 1 (
    echo [ERROR] Failed to copy Startup.scene.
    exit /b 15
)

echo [SUCCESS] Startup scene copied.
echo [SUCCESS] Package created:
echo %PACKAGE_DIR%

exit /b 0
