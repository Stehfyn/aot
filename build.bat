::echo off
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
set flags=/nologo /O2 /Oi  /std:c11 /Wall /wd5045 /wd4710 /wd4191 /WX /D _NDEBUG /D UNICODE /D _UNICODE
set libs=user32.lib comctl32.lib shcore.lib ntdll.lib Kernel32.lib vcruntime.lib ucrt.lib Shlwapi.lib Comctl32.lib

rmdir /s /q %out%
mkdir %out%
pushd %out%

set arch=x86
if exist "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" %arch%
    echo %arch% dll
    rmdir /s /q %arch% 2>nul1
    mkdir %arch%
    pushd %arch%
    cl /MD %flags% /D "_USRDLL" /D "_WINDLL" /LD %sources% /link /MACHINE:X86 %libs% /IMPLIB:aot32.lib /OUT:aot32.dll /ENTRY:DllMain
    echo i386 exe
    cl /MD %flags% /Tc %sources% %libs% aot32.lib /link /MACHINE:X86
    popd
)
set arch=x64
if exist "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    call "%VSINSTALLPATH%\VC\Auxiliary\Build\vcvarsall.bat" %arch%
    rmdir /s /q %arch% 2>&1>nul
    mkdir %arch%
    pushd %arch%
    echo %arch% dll
    cl /MD %flags% /D "_USRDLL" /D "_WINDLL" /LD %sources% /link %libs% /IMPLIB:aot64.lib /OUT:aot64.dll /ENTRY:DllMain
    echo %arch% exe
    cl /MD %flags% /Tc %sources% %libs% aot64.lib 
    popd
)

exit /b 0