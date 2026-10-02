@echo off
REM Assemble a copy of the game that plays outside this repository, in out\package\Penumbra\, from an
REM existing build. It builds nothing and runs nothing: build first (tools\build.bat --target Penumbra).
REM Usage (from Git Bash at the repo root):  cmd //c "tools\package.bat"
REM
REM What goes in, and why each piece is found where it is:
REM   Penumbra.exe          build\game\Penumbra.exe
REM   assets\shaders\*.spv  the engine's compiled shaders. The engine anchors the working directory to the
REM                         executable's folder when it holds assets\shaders (ChooseAssetRoot), so these
REM                         are what make the folder a packaged game, launched from anywhere.
REM   data\                 game\data: strings.json, the English images, the touch controls' manifest and
REM                         art (E16) (eth/Paths.hpp: EXEDIR\data).
REM   original\             extracted\app, the original's DATA only (eth/Paths.hpp: EXEDIR\original holding
REM                         data.enml): no machine.exe, no DLLs, no .as scripts (ported to C++), no Cg
REM                         shaders, no editor project or readmes - nothing the port reads.
REM   *.dll                 the Visual C++ runtime (Penumbra.exe is built /MD), app-local, so a machine
REM                         without the Visual C++ Redistributable runs it too.
REM   README.md, licenses\  what it is, how to play it, and the engine's licences.
REM There is no game.manifest: main.cpp declares the manifest in code, which the engine takes over any
REM file (SupersonicApp: "A DECLARED manifest wins outright and skips the lookup").
REM
REM At run time the engine writes cache\pipeline_cache.bin and creates assets\scenes\ in the folder;
REM saves and settings go to %APPDATA%\Penumbra. out\ is gitignored: a package is never committed.
setlocal
for %%I in ("%~dp0..") do set "REPO=%%~fI"
set "EXE=%REPO%\build\game\Penumbra.exe"
set "PKG=%REPO%\out\package\Penumbra"
set "QUIET=/NFL /NDL /NJH /NJS /NP /R:1 /W:1"

if not exist "%EXE%" (
    echo No %EXE%. Build it first:  cmd //c "tools\build.bat --target Penumbra"
    exit /b 1
)
if not exist "%REPO%\engine\assets\shaders\frag.spv" (
    echo engine\assets\shaders has no SPIR-V. Run  git submodule update --init  first.
    exit /b 1
)
if not exist "%REPO%\extracted\app\data.enml" (
    echo No extracted\app\data.enml: the original game is not where the repository keeps it.
    exit /b 1
)
if not exist "%REPO%\game\data\strings.json" (
    echo No game\data\strings.json.
    exit /b 1
)

REM A fresh folder every time, so nothing from an older package lingers. PKG is fixed, under out\.
if exist "%PKG%\" rmdir /s /q "%PKG%"
if exist "%PKG%\" (
    echo Could not clear %PKG% - is the packaged game still running?
    exit /b 1
)
mkdir "%PKG%" || exit /b 1

copy /y "%EXE%" "%PKG%\Penumbra.exe" >nul || exit /b 1

robocopy "%REPO%\engine\assets\shaders" "%PKG%\assets\shaders" *.spv %QUIET%
if errorlevel 8 goto :copyFailed

robocopy "%REPO%\game\data" "%PKG%\data" /E %QUIET%
if errorlevel 8 goto :copyFailed

robocopy "%REPO%\extracted\app" "%PKG%\original" /E /XF *.exe *.dll *.as *.cg *.ethproj readme.txt %QUIET%
if errorlevel 8 goto :copyFailed

REM The Visual C++ runtime, from the Build Tools' redistributable folder (VCToolsRedistDir, set by vcvars,
REM which tools\msvc_env.bat finds as build.bat does). Without it, the warning below.
call "%~dp0msvc_env.bat" >nul 2>&1
set "CRT="
if defined VCToolsRedistDir (
    for /d %%D in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do set "CRT=%%~fD"
)
if defined CRT (
    robocopy "%CRT%" "%PKG%" *.dll %QUIET%
    if errorlevel 8 goto :copyFailed
) else (
    echo WARNING: no Visual C++ runtime found to copy; a player needs the Visual C++ Redistributable ^(x64^).
)

copy /y "%REPO%\README.md" "%PKG%\README.md" >nul || exit /b 1
copy /y "%REPO%\LICENSE" "%PKG%\LICENSE.txt" >nul || exit /b 1
REM The README links docs/*.md and shows docs/images: they travel with it.
robocopy "%REPO%\docs" "%PKG%\docs" *.md %QUIET%
if errorlevel 8 goto :copyFailed
robocopy "%REPO%\docs\images" "%PKG%\docs\images" /E %QUIET%
if errorlevel 8 goto :copyFailed
mkdir "%PKG%\licenses" || exit /b 1
copy /y "%REPO%\engine\LICENSE" "%PKG%\licenses\Supersonic-Engine-LICENSE.txt" >nul || exit /b 1
copy /y "%REPO%\engine\THIRD_PARTY_LICENSES.md" "%PKG%\licenses\Supersonic-Engine-THIRD_PARTY_LICENSES.md" >nul || exit /b 1
copy /y "%REPO%\LICENSE.md" "%PKG%\licenses\LICENSE.md" >nul || exit /b 1
copy /y "%REPO%\licenses\LGPL-3.0.txt" "%PKG%\licenses\LGPL-3.0.txt" >nul || exit /b 1
copy /y "%REPO%\licenses\GPL-3.0.txt" "%PKG%\licenses\GPL-3.0.txt" >nul || exit /b 1

REM What the game needs is there, and nothing of the original that it does not.
set "MISSING="
for %%F in (Penumbra.exe assets\shaders\frag.spv assets\shaders\screen_overlay_frag.spv data\strings.json data\images\en\entities\menu_buttons.png data\touch_controls.json data\images\touch\jump.png data\images\splash\supersonic-logo.png data\fonts\LiberationSans-Bold.ttf original\data.enml original\hs.enml original\data\shadow.dds original\scenes\menu.esc original\scenes\level1.esc original\soundfx\chefao.mp3 original\entities\menu_buttons.png original\penumbra.ico) do (
    if not exist "%PKG%\%%F" (
        echo MISSING %%F
        set "MISSING=1"
    )
)
if defined MISSING exit /b 1
for %%P in (exe dll as cg) do (
    dir /s /b "%PKG%\original\*.%%P" >nul 2>&1 && (
        echo The original's .%%P files were copied; they must not be.
        exit /b 1
    )
)

echo Packaged to %PKG%
echo Run it from anywhere:  "%PKG%\Penumbra.exe"
exit /b 0

:copyFailed
echo Copying into %PKG% failed (robocopy exit code %ERRORLEVEL%).
exit /b 1
