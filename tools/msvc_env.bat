@echo off
REM Put MSVC's x64 environment (cl, link, rc, lib, the Windows SDK) into the calling script. CALL it:
REM it has no setlocal of its own, so what vcvars64.bat sets stays set for the caller.
REM     call "%~dp0msvc_env.bat" || exit /b 1
REM Which vcvars64.bat: %PENUMBRA_VCVARS% when that is set; otherwise that of the newest Visual Studio
REM or Build Tools with the x64 C++ tools, as vswhere reports it (the Visual Studio Installer keeps
REM vswhere in the one place used below, on every machine). Used by build.bat, check.bat, package.bat.
REM No parenthesised block below expands %ProgramFiles(x86)%: its ")" would end the block.
if defined PENUMBRA_VCVARS goto :load
set "PENUMBRA_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PENUMBRA_VSWHERE%" goto :noVisualStudio
REM Through a file rather than FOR /F, so the quoted path holding "(x86)" never meets the quote
REM handling of the cmd /c that FOR /F runs its command through.
set "PENUMBRA_VSOUT=%TEMP%\penumbra-vswhere-%RANDOM%%RANDOM%.txt"
"%PENUMBRA_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%PENUMBRA_VSOUT%"
set "PENUMBRA_VSDIR="
set /p PENUMBRA_VSDIR=<"%PENUMBRA_VSOUT%"
del "%PENUMBRA_VSOUT%" >nul 2>&1
if not defined PENUMBRA_VSDIR goto :noVisualStudio
set "PENUMBRA_VCVARS=%PENUMBRA_VSDIR%\VC\Auxiliary\Build\vcvars64.bat"
:load
if not exist "%PENUMBRA_VCVARS%" goto :noVcvars
call "%PENUMBRA_VCVARS%" >nul || exit /b 1
exit /b 0

:noVisualStudio
echo No Visual Studio or Build Tools with the x64 C++ tools was found. Install Visual Studio 2022 or
echo later, or its Build Tools, with "Desktop development with C++" - or set PENUMBRA_VCVARS to the
echo full path of a vcvars64.bat.
exit /b 1

:noVcvars
echo No vcvars64.bat at "%PENUMBRA_VCVARS%". Set PENUMBRA_VCVARS to the full path of one.
exit /b 1
