@echo off

:: replaces calling ->
:: call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall" x64

:: Configure for x64
set VSCMD_ARG_HOST_ARCH=x64
set VSCMD_ARG_TGT_ARCH=x64
set VSCMD_ARG_APP_PLAT=Desktop

:: The version of visual studio you're using (needed for the following scripts to work)
:: Fun fact: if you remove the trailing slash, things stop working. Nice, right?
set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\
:: If you're using the Visual Studio Build Tools instead of the IDE:
:: set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\

:: Setup the MSVC compiler and the Windows SDK
call "%VSINSTALLDIR%\Common7\Tools\vsdevcmd\ext\vcvars.bat"
call "%VSINSTALLDIR%\Common7\Tools\vsdevcmd\core\winsdk.bat"

:: The previous scripts in the past would correctly set the INCLUDE env var. But at some point, they
:: stopped doing so (somewhere between 17.2 and 17.3...) That env var is now set by the main script
:: which the whole point of this is to avoid calling. So here we are... until it breaks again...
if not defined INCLUDE set INCLUDE=%__VSCMD_VCVARS_INCLUDE%%__VSCMD_WINSDK_INCLUDE%%__VSCMD_NETFX_INCLUDE%%INCLUDE%
