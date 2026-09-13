@echo off
setlocal
cd /d "%~dp0"

set "version=1.0.0.0"
set "src=%~dp0aot.c"
set "out=%~dp0out"

rem Direct entry points bypass CRT initialization, including the /GS cookie.
set "cflags=/nologo /O2 /Oi /GS- /Zl /std:c11 /Wall /WX /D_NDEBUG /DUNICODE /D_UNICODE /external:anglebrackets /external:W0"
set "rcflags=/nologo /DVERCSV=%version:.=,% /DVERDOT=\"%version%\""
set "libs=ntdll.lib kernel32.lib user32.lib comctl32.lib shlwapi.lib shell32.lib runtimeobject.lib ole32.lib advapi32.lib"

rem Find the newest MSVC install that ships the x86/x64 C++ tools.
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%p in (`"%vswhere%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvarsall.bat`) do set "vcvarsall=%%p"
if not defined vcvarsall (
   echo No Visual Studio C++ toolset found.
   exit /b 1
)

rmdir /s /q "%out%" 2>nul
mkdir "%out%\bin"

call :host x86
call :host x64
call :release
exit /b 0


rem host <arch> -- build out\<arch>\aot<arch>-host.exe with the hook dll, hook
rem exe and host dll embedded, then hand it up to out\ for the release build.
rem setlocal keeps the vcvarsall env and cwd scoped to this call.
:host
setlocal
echo.
echo === %1 ===
call "%vcvarsall%" %1 >nul
mkdir "%out%\%1"
cd /d "%out%\%1"

call :run rc %rcflags% /D_VERRES /fo ver.res "%src%"
call :run cl %cflags% /D_WINDLL /D_AOTHOOKDLL /LD "%src%" %libs% /link /NODEFAULTLIB /MACHINE:%1 /ENTRY:DllMain /IMPLIB:aot-hook.lib /OUT:aot-hook.dll
call :run cl %cflags% /D_WINDLL /D_AOTHOSTDLL /LD "%src%" %libs% /link /NODEFAULTLIB /MACHINE:%1 /ENTRY:DllMain /IMPLIB:aot-host.lib /OUT:aot-host.dll
call :run cl %cflags% "%src%" ver.res %libs% aot-hook.lib aot-host.lib /link /NODEFAULTLIB /MACHINE:%1 /SUBSYSTEM:Windows /OUT:aot-hook.exe

call :run rc /nologo /D_HOSTRES /fo aot.res "%src%"
call :run cl %cflags% /D_HOST "%src%" aot.res ver.res %libs% aot-host.lib /link /NODEFAULTLIB /MACHINE:%1 /SUBSYSTEM:Windows /OUT:aot%1-host.exe
copy /y aot%1-host.exe "%out%" >nul
exit /b 0


rem release -- build out\bin\AlwaysOnTop.exe: a 32-bit launcher with both host
rem executables embedded as resources.
:release
setlocal
echo.
echo === release ===
call "%vcvarsall%" x86 >nul
cd /d "%out%"

call :run rc %rcflags% /D_VERRES /fo ver.res "%src%"
call :run rc /nologo /D_RELRES /fo aot.res "%src%"
call :run cl %cflags% /D_RELEASE "%src%" aot.res ver.res %libs% /link /NODEFAULTLIB /MACHINE:x86 /SUBSYSTEM:Windows /OUT:bin\AlwaysOnTop.exe
exit /b 0


rem run <command> -- echo the command line, then run it.
:run
echo.
echo   ^> %*
%*
exit /b %errorlevel%
