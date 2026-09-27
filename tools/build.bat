@echo off
REM Build the Penumbra port in this repository's build\, from any shell, with MSVC's environment.
REM Machine-specific on purpose: the Build Tools path and the Vulkan SDK's glslc live here, in one place.
REM Usage: tools\build.bat [cmake --build args...]   e.g.  --target Penumbra
REM The first run - or the first after build\ is deleted - also configures it, as the engine's own build
REM is configured: Ninja, Release, Vulkan validation on in Release, MSVC's cl named outright (CMake takes
REM Strawberry Perl's GCC when that comes first on PATH), and the SDK's glslc. A configured build\ is left as it is.
setlocal
for %%I in ("%~dp0..") do set "REPO=%%~fI"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%REPO%\engine\CMakeLists.txt" (
    echo engine\ is empty: the Supersonic Engine is a submodule. Run  git submodule update --init  first.
    exit /b 1
)
if not exist "%REPO%\build\CMakeCache.txt" (
    cmake -S "%REPO%" -B "%REPO%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DGLSL_COMPILER=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe || exit /b 1
)
cmake --build "%REPO%\build" %*
