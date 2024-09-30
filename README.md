# EDK2 Setup Guide
## Setup Visual Studio 2022
Download and install Visual Studio 2022. Make sure to select the following components during the installation process:

- Desktop development with C++. Make sure Windows SDK is selected.

## Setup VS Code
- Download and install [VS Code](https://code.visualstudio.com/).
- Open VS Code. Go to Extension.
- Download Edk2code extenstion.

## Setup EDK2 environment
- Install [Git](https://git-scm.com/)
- Install [Python](https://www.python.org/downloads/release/python-3126/)
- Install [NASM](https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/). Make sure to install it on `C:/NASM`.

## Clone the EDK2 Repository
Clone the EDK2 repository to your local machine.
```
git clone https://github.com/tianocore/EDK2.git
git submodule update --init --recursive
```

## Build or Install the EDK2
1. Navigate to the edk2 directory and open VS Code in that directory.
2. Run this command `edksetup.bat Rebuild` in the Command Prompt terminal.
3. If a python error is faced. 
- Run this command to get the python installation directory.

    `python -c "import sys; print(sys.executable)"`
If the output is `C:/Python312/python.exe` then Python path is PythonPath=`C:/Python312`
- Run `set PYTHON_HOME={PythonPath}`
- Goto step 2 again.

4. If the installation is successfull. Goto Conf/target.txt.
5. Set TARGET_ARCH=X64, TOOLCHAIN_TAG=VS2019
6. Run `build`.

## Run UEFI Application

- Goto Build/EmulatorX64/DEBUG_VS2019/X64.
- Run WinHost.exe
- Type `fs0:` on shell terminal.
- Run `HelloWorld.efi`

OR

- Copy the HelloWorld.efi from Build/EmulatorX64/DEBUG_VS2019/X64.
- Place the file in a FAT32 formatted USB Drive inside the folder `EFI/Boot/HelloWorld.efi`
- Restart the PC and boot from this drive.
