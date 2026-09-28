@echo off
REM Build, package and optionally install Penumbra for Android. The work is tools\build_android.sh;
REM this only runs it under Git Bash. Options as there: --abi, --config, --install, --serial, --run,
REM --package-only (tools\build_android.bat --help lists them).
"C:\Program Files\Git\bin\bash.exe" "%~dp0build_android.sh" %*
