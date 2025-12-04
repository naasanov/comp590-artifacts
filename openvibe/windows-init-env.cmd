@echo off

set PATH=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin;%PATH%
set "SCRIPT_PATH=%~dp0"
set platformTarget=x64
set visualStudioTools=
set configurationType=
set cmakeGenerator=
set vcvarsallPath=

:parameter_parse
if /i "%1" == "--platform-target" (
	set PlatformTarget=%2
	SHIFT
	SHIFT

	Goto parameter_parse
) else if not "%1" == "" (
	echo unrecognized option [%1]
	Goto terminate_error
)
REM ########################################################################################################################

if %platformTarget% NEQ x64 (
    if %platformTarget% NEQ x86 (
        echo "Error: Unsupported platform-target %platformTarget%"
        pause
        exit 1
    )
)

rem Check for MSVC 2017 to 2019
if exist "%ProgramFiles(x86)%/Microsoft Visual Studio/Installer/" (
    for /f "usebackq delims=#" %%a in (`"%ProgramFiles(x86)%/Microsoft Visual Studio/Installer/vswhere" -version [15.0^,17.0] -latest -property catalog_productLineVersion`) do (
        if ["%%a"] == ["2019"] (
            set cmakeGenerator="Visual Studio 16 2019"
        ) else if ["%%a"] == ["2017"] (
            set cmakeGenerator="Visual Studio 15 2017"
        )
        for /f "usebackq delims=#" %%a in (`"%ProgramFiles(x86)%/Microsoft Visual Studio/Installer/vswhere" -version [15.0^,17.0] -latest -property installationPath`) do (
            set "visualStudioTools=%%a"
        )

        set "vcvarsallPath=/VC/Auxiliary/Build/vcvarsall.bat"
        set configurationType=%platformTarget%
        if %platformTarget% == x64 (
            set cmakeGenerator= !cmakeGenerator! -A %platformTarget%
        ) else (
            set cmakeGenerator= !cmakeGenerator! -A Win32
        )
    )
)

rem If MSVC not found above
if ["%visualStudioTools%"] == [""] (
    rem Use Visual Studio 2013 (different tools access method).
    set "visualStudioTools=%VS120COMNTOOLS%"
    set "vcvarsallPath=../../VC/vcvarsall.bat"
    set configurationType=%platformTarget%
    if %platformTarget% == x64 (
        set cmakeGenerator="Visual Studio 12 2013" -A %platformTarget%
        if not exist "%VS120COMNTOOLS%../../VC/bin/amd64" (
            set configurationType=x86_amd64
        )
    ) else (
        set cmakeGenerator="Visual Studio 12 2013" -A Win32
    )
)

if exist "%visualStudioTools%%vcvarsallPath%" (
    echo "Found %cmakeGenerator% tools: %visualStudioTools%%vcvarsallPath% %configurationType%"
    call "%visualStudioTools%%vcvarsallPath%" %configurationType%
) else (
    echo,
    echo **************************************************
    echo,
    if exist "%visualStudioTools%" (
        echo ERROR: Could not find C/C++ support in "%visualStudioTools%"
        echo        Please, ensure that you have selected the "Desktop Development with C++" module in your Visual Studio installation.
    ) else (
        echo ERROR: No supported version of Visual Studio were found.
        echo Supported versions are:
        echo  - Visual Studio 15 2017
        echo  - Visual Studio 12 2013
    )
    echo,
    echo **************************************************
    echo,
    pause
    exit 1
)
