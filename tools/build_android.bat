@echo off
REM Build, package and optionally install Penumbra for Android. The work is tools\build_android.sh;
REM this only runs it under Git Bash: %PENUMBRA_GIT_BASH% when set, else Git for Windows' own at its
REM default place. Options as there: --abi, --config, --install, --serial, --run, --package-only
REM (tools\build_android.bat --help lists them).
setlocal
set "GITBASH=%PENUMBRA_GIT_BASH%"
if not defined GITBASH set "GITBASH=%ProgramFiles%\Git\bin\bash.exe"
if exist "%GITBASH%" goto :run
echo No Git Bash at "%GITBASH%". Install Git for Windows, or set PENUMBRA_GIT_BASH to its bash.exe.
exit /b 1
:run
"%GITBASH%" "%~dp0build_android.sh" %*
