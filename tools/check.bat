@echo off
REM Compile-check one or more of the port's .cpp files WITHOUT the build tree: the same flags, include
REM paths and definitions build\ uses for game code, /W4, objects to a throwaway folder, nothing linked.
REM It takes no lock and writes nothing into the repository, so parallel agents may run it at once
REM while one integration step owns build\. It never produces an executable (no Smart App Control).
REM Usage (from Git Bash at the repo root):  cmd //c "tools\check.bat game\eth\Scene.cpp game\eth\Entity.cpp"
setlocal enabledelayedexpansion
for %%I in ("%~dp0..") do set "REPO=%%~fI"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "E=%REPO%\engine"
set "OUT=%TEMP%\penumbra-check-%RANDOM%%RANDOM%"
mkdir "%OUT%" >nul 2>&1
set INC=/I"%REPO%\game" /I"%E%\src" /I"%E%\tests" /I"%E%\third_party\glm" /I"%E%\third_party\stb" /I"%E%\third_party\tinygltf" /I"%E%\third_party\imgui" /I"%E%\third_party\imgui\backends" /I"%E%\third_party\imguizmo" /I"%E%\third_party\entt-3.13.2\src" /I"%E%\third_party\VulkanMemoryAllocator-3.1.0\include" /I"%E%\third_party\glfw-3.4\include" /I"%REPO%\game\third_party\tinyxml" /external:I"C:\VulkanSDK\1.4.357.0\Include" /external:W0
set DEF=/DGLFW_INCLUDE_NONE /DGLM_ENABLE_EXPERIMENTAL /DGLM_FORCE_DEPTH_ZERO_TO_ONE /DGLM_FORCE_RADIANS /DSUPERSONIC_ENABLE_VALIDATION=1 /DSUPERSONIC_PLATFORM_WINDOWS=1 /DTIXML_USE_STL /DPENUMBRA_ORIGINAL_DIR=\"C:/Users/icvet/Desktop/Penumbra-and-the-Castle-of-Shadows-Enhanced/extracted/app\" /DPENUMBRA_DATA_DIR=\"C:/Users/icvet/Desktop/Penumbra-and-the-Castle-of-Shadows-Enhanced/game/data\"
set FAIL=0
for %%F in (%*) do (
    cl /nologo /c /DWIN32 /D_WINDOWS /EHsc /Od /MD -std:c++20 /W4 /fp:precise %DEF% %INC% /Fo"%OUT%\\" "%%~fF" || set FAIL=1
)
rmdir /s /q "%OUT%" >nul 2>&1
if !FAIL!==1 (
    echo CHECK FAILED
    exit /b 1
)
echo CHECK OK
exit /b 0
