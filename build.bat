@echo off
setlocal EnableDelayedExpansion
cd "%~dp0"

for /f "usebackq tokens=*" %%a in (`call "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
   set "VSINSTALLPATH=%%a"
)

if not defined VSINSTALLPATH (
   echo No Visual Studio installation detected.
   exit /b 0
)

set out=out
set sources=..\..\aot.c
set cflags=/nologo /O2 /Oi /MD /std:c11 /Wall /WX /wd5045 /wd4710 /wd4191 /D _NDEBUG /D UNICODE /D _UNICODE
set libs=user32.lib comctl32.lib shcore.lib ntdll.lib Kernel32.lib vcruntime.lib ucrt.lib Shlwapi.lib Comctl32.lib

set "build_target=/?"

if exist "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
	rmdir /s /q %out%
	mkdir %out%
	pushd %out%

	call :%%build_target%% x86 aot 3>&1 >nul
	call :%%build_target%% x64 aot 3>&1 >nul

	popd
)

if "%0" == ":%build_target%" (
	echo Build Target %2-%1 @call:
	call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" %1
	echo %1 dll
	rmdir /s /q %1 2>nul
	mkdir %1
	pushd %1
	cl %cflags% /D "_WINDLL" /LD %sources% /link /MACHINE:%1 %libs% /IMPLIB:%2%1.lib /OUT:%2%1.dll /ENTRY:DllMain
	echo %1 exe
	cl %cflags% /Tc %sources% %libs% %2%1.lib /link /MACHINE:%1
	popd
)>&3

exit /b 0