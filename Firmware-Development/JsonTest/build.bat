@echo off
REM Script to build EDK2 HelloWorld project in a separate folder on Windows

REM Set up the EDK2 environment
REM Modify the path to your EDK2 base directory and toolchain

set EDK2_BASE_PATH=D:\edk2
set TOOLCHAIN=VS2019
set ARCH=X64
set BUILD_TARGET=DEBUG

REM Define the HelloWorld package
set PROJ_DIR=%CD%
set DSC_FILE=%PROJ_DIR%\JsonTest.dsc
echo %DSC_FILE%

REM Extract the project name from PROJ_DIR
for %%F in ("%PROJ_DIR%") do set PROJECT_NAME=%%~nxF
set PROJECT_PATH=%EDK2_BASE_PATH%\%PROJECT_NAME%

REM Navigate to the EDK2 base directory
cd /d %EDK2_BASE_PATH%

REM Remove the project directory if it exists
if exist "%PROJECT_PATH%" (
    echo Removing existing project directory...
    rmdir /s /q "%PROJECT_PATH%"
)

REM Initialize the EDK2 build environment
call edksetup.bat

REM Copy the project files to the EDK2 directory (you may need to adjust the source path)
echo Copying project files...
xcopy /e /i /y "%PROJ_DIR%\*" "%PROJECT_PATH%\" >nul

REM Run build command in a separate command prompt
cmd /c build -p %DSC_FILE% -a %ARCH% -t %TOOLCHAIN% -b %BUILD_TARGET% -D DEBUG

REM Navigate back to the original folder
cd /d %PROJ_DIR%
