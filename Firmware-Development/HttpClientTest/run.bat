@echo off
rem Set up the Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

rem Set the workspace variable
set WORKSPACE=%cd%

rem Set the EDK2 path
set EDK2_PATH=D:\EDK2

rem Update the packages path
set PACKAGES_PATH=%WORKSPACE%;%EDK2_PATH%

rem Reconfigure EDK2
call %EDK2_PATH%\edksetup.bat reconfig

rem Build the project
build -a X64 -b RELEASE -t VS2019 -p HttpClientTest.dsc

rem Pause to view output
pause
