@echo off
REM Compile-check one or more of the port's .cpp files WITHOUT the build tree: the same flags, include
REM paths and definitions build\ uses for game code, /W4, objects to a throwaway folder, nothing linked.
REM It takes no lock and writes nothing into the repository, so parallel agents may run it at once
REM while one integration step owns build\. It never produces an executable (no Smart App Control).
REM Usage (from Git Bash at the repo root):  cmd //c "tools\check.bat game\eth\Scene.cpp game\eth\Entity.cpp"
REM MSVC comes from tools\msvc_env.bat; the Vulkan headers from the SDK (%VULKAN_SDK%) when it is
REM installed, as the build takes them, else the engine's vendored copy, as the engine falls back to.
setlocal enabledelayedexpansion
for %%I in ("%~dp0..") do set "REPO=%%~fI"
call "%~dp0msvc_env.bat" || exit /b 1
set "E=%REPO%\engine"
set "REPO_FWD=%REPO:\=/%"
set "VKINC=%E%\third_party\Vulkan-Headers-1.3.290\include"
if defined VULKAN_SDK if exist "%VULKAN_SDK%\Include\vulkan\vulkan.hpp" set "VKINC=%VULKAN_SDK%\Include"
set "OUT=%TEMP%\penumbra-check-%RANDOM%%RANDOM%"
mkdir "%OUT%" >nul 2>&1
set INC=/I"%REPO%\game" /I"%E%\src" /I"%E%\tests" /I"%E%\third_party\glm" /I"%E%\third_party\stb" /I"%E%\third_party\tinygltf" /I"%E%\third_party\imgui" /I"%E%\third_party\imgui\backends" /I"%E%\third_party\imguizmo" /I"%E%\third_party\entt-3.13.2\src" /I"%E%\third_party\VulkanMemoryAllocator-3.1.0\include" /I"%E%\third_party\glfw-3.4\include" /I"%REPO%\game\third_party\tinyxml" /external:I"%VKINC%" /external:W0
REM The directories as the build bakes them (game/eth, game/CMakeLists.txt and, for the suites'
REM own data, tests/CMakeLists.txt: tests/data), from wherever this clone is; each define quoted whole
REM so a path with spaces stays one argument. E28: PENUMBRA_TESTS_DATA_DIR added, for the two suites
REM that read tests/data (test_pn_render_hud's rooms, test_pn_scenarios).
set DEF=/DGLFW_INCLUDE_NONE /DGLM_ENABLE_EXPERIMENTAL /DGLM_FORCE_DEPTH_ZERO_TO_ONE /DGLM_FORCE_RADIANS /DSUPERSONIC_ENABLE_VALIDATION=1 /DSUPERSONIC_PLATFORM_WINDOWS=1 /DTIXML_USE_STL "/DPENUMBRA_ORIGINAL_DIR=\"%REPO_FWD%/extracted/app\"" "/DPENUMBRA_DATA_DIR=\"%REPO_FWD%/game/data\"" "/DPENUMBRA_TESTS_DATA_DIR=\"%REPO_FWD%/tests/data\""
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
