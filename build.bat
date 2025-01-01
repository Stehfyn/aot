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
set cflags=/nologo /O2 /Oi /MT /std:c11 /Wall /WX /wd4702 /wd5045 /wd4710 /wd4191 /wd4820 /wd4255 /D _NDEBUG /D UNICODE /D _UNICODE
set libs=user32.lib comctl32.lib ntdll.lib Kernel32.lib libcmt.lib Shlwapi.lib Comctl32.lib shell32.lib runtimeobject.lib Ole32.lib Advapi32.lib

set "build_hook=/?"

if exist "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
	rmdir /s /q %out%
	mkdir %out%
	pushd %out%
	mkdir int

	call :%%build_hook%% aot x86 3>&1 >nul
	call :%%build_hook%% aot x64 3>&1 >nul

	mkdir bin
	pushd int
	call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

	echo. && echo  x64 release resources
	copy ..\..\aot.c aot.c
	copy ..\..\aot.exe.manifest aot.exe.manifest
	copy ..\..\aot.ico aot.ico
	copy aot.c aot.rc
	rc /DVERCSV=""1,0,0,0"" /DVERDOT=""1.0.0.0"" -d _VERRES /fo ver.res aot.rc
	rc -d _RELRES /fo aot.res aot.rc

	echo. && echo x64 release exe
	cl %cflags% /D "_RELEASE" /Tc %sources% aot.res %libs% ver.res /link /MACHINE:x64 /OUT:AlwaysOnTop.exe /SUBSYSTEM:Windows
	copy AlwaysOnTop.exe ..\bin\AlwaysOnTop.exe

	popd
	popd
)

if "%0" == ":%build_hook%" (
	echo Build %2 Hook @call:
	call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" %2
	
	rmdir /s /q %2 2>nul
	mkdir %2
	pushd %2

	copy ..\..\aot.c aot.c
	copy ..\..\aot.ico aot.ico
	copy aot.c aot.rc
	rc /DVERCSV=""1,0,0,0"" /DVERDOT=""1.0.0.0"" -d _VERRES /fo ver.res aot.rc

	echo. && echo %2 hook dll
	cl %cflags% /D "_WINDLL" /D "_AOTHOOKDLL" /LD %sources% /link /MACHINE:%2 %libs% /IMPLIB:%1-hook.lib /OUT:%1-hook.dll /ENTRY:DllMain

	echo. && echo %2 host dll
	cl %cflags% /D "_WINDLL" /D "_AOTHOSTDLL" /LD %sources% /link /MACHINE:%2 %libs% /IMPLIB:%1-host.lib /OUT:%1-host.dll /ENTRY:DllMain

	echo. && echo %2 hook exe
	cl %cflags% /Tc %sources% %libs% %1-hook.lib %1-host.lib ver.res /link /MACHINE:%2 /OUT:%1-hook.exe /SUBSYSTEM:Windows

	echo. && echo %2 resources
	copy ..\..\aot.exe.manifest aot.exe.manifest
	rc -d _HOSTRES /fo aot.res aot.rc

	echo. && echo %2 host exe
	cl %cflags% /D "_HOST" /Tc %sources% aot.res %libs% %1-host.lib ver.res /link /MACHINE:%2 /OUT:%1%2-host.exe /SUBSYSTEM:Windows
	copy %1%2-host.exe ..\int\%1%2-host.exe

	popd
	
)>&3

exit /b 0