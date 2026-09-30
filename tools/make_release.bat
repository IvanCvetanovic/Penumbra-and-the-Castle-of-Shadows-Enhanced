@echo off
REM The Windows release a player downloads: out\release\Penumbra-Windows.zip, from an existing build.
REM Usage (from Git Bash at the repo root), after a build:
REM     cmd //c "tools\build.bat --target Penumbra"
REM     cmd //c "tools\make_release.bat"
REM
REM   1. tools\package.bat assembles out\package\Penumbra (the game, its files, the Visual C++ runtime).
REM   2. tools\make_release.py windows stages out\release\Penumbra-Windows\ ("HOW TO PLAY.txt" beside
REM      the Penumbra\ folder), zips it into out\release\Penumbra-Windows.zip with fixed timestamps and
REM      sorted entries, and writes out\release\SHA256SUMS.txt. It refuses a package without the Visual
REM      C++ runtime, and a version that differs between CMakeLists.txt and the two manifests.
REM
REM It builds nothing, runs nothing and signs nothing: the zip is unsigned (docs/code-signing.md).
REM The Android half of a release is  bash tools/build_android.sh --release  (out\release\Penumbra-Android.apk).
setlocal
call "%~dp0package.bat" || exit /b 1
python "%~dp0make_release.py" windows || exit /b 1
exit /b 0
