@echo off
REM Build the Penumbra port in this repository's build\, from any shell, with MSVC's environment.
REM Usage: tools\build.bat [cmake --build args...]   e.g.  --target Penumbra
REM The first run - or the first after build\ is deleted - also configures it, as the engine's own build
REM is configured: Ninja, Release, Vulkan validation on in Release, MSVC's cl named outright (CMake takes
REM Strawberry Perl's GCC when that comes first on PATH). A configured build\ is left as it is.
REM What it takes, from where (each overridable):
REM   MSVC          tools\msvc_env.bat: %PENUMBRA_VCVARS%, else the newest Visual Studio or Build Tools
REM   CMake, Ninja  on PATH (Visual Studio's C++ CMake tools bring both)
REM   Vulkan        the SDK when installed (%VULKAN_SDK%), else the engine's vendored headers and the
REM                 driver's vulkan-1.dll (engine/CMakeLists.txt)
REM   glslc         %PENUMBRA_GLSLC%, else the Vulkan SDK 1.4.357.0's at the SDK's default place: the
REM                 compiler the engine's committed SPIR-V reproduces under, byte for byte. With neither,
REM                 GLSL_COMPILER=OFF - no shader is compiled and the committed SPIR-V is used, which is
REM                 all a build needs; OFF also keeps another glslc on PATH from rewriting those blobs.
setlocal
for %%I in ("%~dp0..") do set "REPO=%%~fI"
call "%~dp0msvc_env.bat" || exit /b 1
if not exist "%REPO%\engine\CMakeLists.txt" (
    echo engine\ is empty: the Supersonic Engine is a submodule. Run  git submodule update --init  first.
    exit /b 1
)
set "GLSLC=%PENUMBRA_GLSLC%"
if not defined GLSLC if exist "C:\VulkanSDK\1.4.357.0\Bin\glslc.exe" set "GLSLC=C:/VulkanSDK/1.4.357.0/Bin/glslc.exe"
if not defined GLSLC set "GLSLC=OFF"
if not exist "%REPO%\build\CMakeCache.txt" (
    cmake -S "%REPO%" -B "%REPO%\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl "-DGLSL_COMPILER=%GLSLC%" || exit /b 1
)
cmake --build "%REPO%\build" %*
