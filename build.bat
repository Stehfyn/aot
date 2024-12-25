@echo off
setlocal enabledelayedexpansion
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
set cflags=/nologo /O2 /Oi /MD /std:c11 /Wall /WX /wd4702 /wd5045 /wd4710 /wd4191 /wd4820 /D _NDEBUG /D UNICODE /D _UNICODE
set libs=user32.lib comctl32.lib shcore.lib ntdll.lib Kernel32.lib vcruntime.lib ucrt.lib Shlwapi.lib Comctl32.lib

set "build_target=/?"

if exist "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
	rmdir /s /q %out%
	mkdir %out%
	pushd %out%
	mkdir bin

	call :%%build_target%% aot x86 3>&1 >nul
	call :%%build_target%% aot x64 3>&1 >nul
	popd
)

if "%0" == ":%build_target%" (
	echo Build Target %1-%2 @call:
	call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" %2
	
	rmdir /s /q %2 2>nul
	mkdir %2
	pushd %2

	echo. && echo %2 dll
	cl %cflags% /D "_WINDLL" /LD %sources% /link /MACHINE:%2 %libs% /IMPLIB:%1%2.lib /OUT:%1%2.dll /ENTRY:DllMain
	copy %1%2.dll ..\bin\%1%2.dll

	echo. && echo %2 sentinel 
	cl %cflags% /D "_SENTINEL" /Tc %sources% %libs% %1%2.lib /link /MACHINE:%2 /OUT:%1%2-sentinel.exe
	copy %1%2-sentinel.exe ..\bin\%1%2-sentinel.exe

	echo. && echo %2 hook 
	cl %cflags% /Tc %sources% %libs% %1%2.lib /link /MACHINE:%2 /OUT:%1%2.exe
	copy %1%2.exe ..\bin\%1%2.exe

	echo. && echo %2 resources
	copy ..\..\aot.c aot.c
	copy aot.c aot.rc
	rc -d _RES aot.rc

	echo. && echo %2 host 
	cl %cflags% /D "_HOST" /Tc %sources% aot.res %libs% %1%2.lib /link /MACHINE:%2 /OUT:%1%2-host.exe
	copy %1%2-host.exe ..\bin\%1%2-host.exe

	popd
	
)>&3

exit /b 0