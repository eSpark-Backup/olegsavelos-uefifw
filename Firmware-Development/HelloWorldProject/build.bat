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
set DSC_FILE=%PROJ_DIR%\HelloWorldProject.dsc
echo %DSC_FILE%

REM Navigate to the EDK2 base directory
cd /d %EDK2_BASE_PATH%

REM Initialize the EDK2 build environment
call edksetup.bat

REM Build HelloWorld project
build -p %DSC_FILE% -a %ARCH% -t %TOOLCHAIN% -b %BUILD_TARGET% -D DEBUG

REM Navigate back to the original folder
cd /d %PROJ_DIR%

echo Build completed!
pause
