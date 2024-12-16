@echo off
setlocal enabledelayedexpansion

set platformx86=x86
set platformx64=x64
set target_dir=out
set flags=/nologo /O2 /Oi  /std:c11 /Wall /wd5045 /wd4710 /wd4191 /WX /D _NDEBUG /D UNICODE /D _UNICODE
set sources=..\..\aot.c
set libs=user32.lib comctl32.lib shcore.lib ntdll.lib Kernel32.lib vcruntime.lib ucrt.lib Shlwapi.lib
set root="%~dp0"
pushd %root%

if not defined DevEnvDir (
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if errorlevel 1 (
echo vcvars64.bat not found
exit /b errorlevel
)
)

rmdir /s /q %target_dir%
mkdir %target_dir%
pushd %target_dir%
mkdir bin
echo dll

set PreferredToolArchitecture=x86
mkdir %platformx86%
pushd %platformx86%
cl /MD %flags% /D "_USRDLL" /D "_WINDLL" /LD %sources% /link %libs% /OUT:aot32.dll /ENTRY:DllMain
copy aot32.dll ..\bin\aot32.dll
popd

set PreferredToolArchitecture=x64
mkdir %platformx64%
pushd %platformx64%
cl /MD %flags% /D "_USRDLL" /D "_WINDLL" /LD %sources% /link %libs% /OUT:aot64.dll /ENTRY:DllMain
copy aot64.dll ..\bin\aot64.dll


echo exe
cl /MD %flags% /Tc %sources% %libs% aot.lib
copy aot.exe ..\bin\aot.exe

:fail
popd
popd

endlocal