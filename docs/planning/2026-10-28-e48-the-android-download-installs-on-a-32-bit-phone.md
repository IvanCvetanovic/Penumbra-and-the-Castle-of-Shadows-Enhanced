# E48: the Android download installs on a 32-bit phone

Built 2026-10-28, in the engine (`engine/`, additive: the new `src/renderer/VulkanHandle.hpp`; `src/core/RenderSystem.cpp`,
`src/renderer/{UIImageStore,VulkanOffscreen,VulkanBuffer,VulkanDevice,VulkanImage}.cpp`, `src/editor/ThumbnailCache.cpp`;
the tilemap's count in `src/core/Components.hpp` and its callers `src/core/{TilemapSystem,ComponentCodec,StateHash}.cpp`,
`src/editor/InspectorPanel.cpp`, and `src/core/NavGrid.cpp`; `tests/test_shadowcache.cpp`, `tests/test_nav.cpp`) and in the game (`tools/build_android.sh`, the new `.github/workflows/android.yml`,
`game/android/AndroidManifest.xml`, the install guides and the README).

## What was wrong

A player's report: an Infinix Smart 9, Android 14, the APK installed twice, and Android said *the app is not compatible*.
The published 1.0.10 `Penumbra-Android.apk` (downloaded; sha256 `cbda9fb2...`, the release's own) holds
`lib/x86_64/libPenumbra.so` and `lib/arm64-v8a/libPenumbra.so` and nothing else; `tools/build_android.sh` took no other ABI.
The Smart 9 is a MediaTek Helio G81 (eight 64-bit cores, a Mali-G52 MC2: the Galaxy A05's GPU, E47) running Android 14
Go edition, and its owners report, consistently, a 32-bit system image: the processor can run 64-bit code and the Android on
it runs apps as `armeabi-v7a` only, which several 64-bit-only apps also refuse to install on. On such a phone the installer
finds no library it can load and refuses the APK with that message, before the game or its Vulkan check ever runs. Nothing
in the README or the install guides said the download was 64-bit only; the `minSdk` (26) and the Vulkan feature line are not
what refuses it (a sideloaded APK is not held to `uses-feature`, and Android 14 is far above 26).

Not confirmed: the reporter's ABI list and exact model (asked for, with the way to read them). Owners' reviews on GSMArena
and a post on itch.io about another game's "isn't compatible" on this model are what the diagnosis rests on, and they agree
with the one message the installer shows for a missing ABI.

## What changed

- **`tools/build_android.sh` builds `armeabi-v7a`**, as a third ABI (`--abi armeabi-v7a`), and `all`, which `--release`
  builds, is now the three: `arm64-v8a` (most phones), `armeabi-v7a` (the phones whose Android is 32-bit), `x86_64` (the
  emulator). The same NDK toolchain file with `ANDROID_ABI=armeabi-v7a` and `android-26`, the NDK's defaults for that ABI
  (Thumb-2, NEON, which every ARMv7 phone that runs Android 8 has), the library stripped and placed at
  `lib/armeabi-v7a/libPenumbra.so`, which is the name the installer looks for.
- **The script runs on Linux as well as Git Bash.** The host is detected once (`uname -s`): on Windows paths go to the SDK's
  tools through `cygpath -m`, the tools carry `.exe`, `d8` and `apksigner` are `.bat` wrappers run through `cmd`, `python`
  is Windows' Python; on Linux a path is a path, the tools have no suffix, the wrappers are shell scripts, `python3`, the
  SDK's default at `~/Android/Sdk`, and the `cmake`/`ninja` on PATH stand in when the SDK's 3.22.1 is not installed. Every
  tool is checked before anything is built. Nothing changes for the Windows release build but the ABI list.
- **`.github/workflows/android.yml`**, new: on every push to main, every pull request, and by hand, the APK with the three
  ABIs is built on GitHub's `ubuntu-24.04` runner with the NDK and cmake the script pins (sdkmanager installs them beside
  the image's own), debug-signed, its `native-code` line checked for all three ABIs, and uploaded as the artifact
  `Penumbra-Android-debug`. Before this, no workflow compiled the Android build at all: the APK was built only on the
  development machine, and a mistake that shows only under the NDK (or only at 32 bits) had nothing to stop it. The release
  APK is still made on the development machine with the offline release key.
- **The engine compiles at 32 bits**, which it had not since Vulkan handles entered it. Six errors, all one defect: a
  non-dispatchable Vulkan handle (`VkDescriptorSet`, `VkBuffer`, `VkImage`, `VkSurfaceKHR`) is a pointer to an opaque struct
  on a 64-bit build and a `uint64_t` on a 32-bit one (`VK_DEFINE_NON_DISPATCHABLE_HANDLE`), so `reinterpret_cast` through
  `uintptr_t` is ill-formed there (`UIImageStore.cpp`, twice), vulkan.hpp's conversion from the raw handle is `explicit`
  there (`vk::Buffer`, `vk::Image` and `vk::SurfaceKHR` assigned from the raw handle VMA or the window returned:
  `VulkanBuffer.cpp`, `VulkanImage.cpp`, `VulkanDevice.cpp`), and `RenderSystem.cpp`'s pass key asserted that every handle
  it is given is 64 bits wide, which the mesh pointer it is also given is not, there. One more site compiled and was
  wrong: `VulkanOffscreen.cpp` made its ImGui texture id with `(ImTextureID)(uintptr_t)ds`, which on a 32-bit build keeps
  the low half of the 64-bit handle, so ImGui would draw with a descriptor that is not the one it was given (the editor's
  viewport; not on the game's path, fixed all the same). The new `renderer/VulkanHandle.hpp` has the two conversions,
  `HandleToU64` and `HandleFromU64<Handle>`, which look at the type (a pointer goes through `uintptr_t`, an integer is
  widened as it is) and give the same bits back on every build; every site above and `ThumbnailCache.cpp` (two casts that
  compiled only because `ImU64` and the 32-bit `uint64_t` are the same typedef) use them, `HandleKey` is a one-line call,
  and the three assignments construct the `vk::` handle explicitly, which is valid on both builds. On a 64-bit build nothing
  changes: the same bits in, the same bits out. `test_shadowcache` gains six checks on the round trip (a 64-bit handle, the
  wrapped `vk::DescriptorSet`, two handles two numbers, a pointer of the build's width, its zero-extended address, null).
- **The engine's suites pass at 32 bits**, which took one more fix. With the handles compiled, the engine's own 59 ctest
  entries ran at `-m32` and one segfaulted: `test_tilemap`, in its own check that "the count does not wrap".
  `TilemapComponent::cellCount` computed width x height as a `size_t` on purpose, against the `uint32_t` product that
  wraps 65,536 x 65,536 to zero; a 32-bit build's `size_t` wraps it the same way, so `withinCap` passed, `Set` resized
  nothing and wrote past an empty vector. The count, `Resize`'s check, `At`'s index and every caller (the bake and the
  content hash in `TilemapSystem`, `ComponentCodec`'s load, `StateHash`, the inspector's `%zu`) compute in `uint64_t`
  and narrow only once the cap has bounded the value; `NavGrid::Resize` had the same `size_t` product in its cap check
  and takes the same 64-bit one, with a check added to its suite. Penumbra uses neither system; the engine's own
  correctness at 32 bits is what this is for.
- **The guides**: the README's requirements line, `docs/install.md` and `docs/install.pt.md` say the download runs on
  32-bit phones from 1.0.11, and the *It didn't work?* list's first bullet names the 1.0.10 message, why it appeared and
  what to do (update). The "64-bit phones only" wording added earlier the same day is withdrawn by this.

## Measured and tested

To be read with what follows it: nothing here ran on a 32-bit phone.

- A 32-bit build on the development host, as a stand-in (GCC 13, `-m32`, Ubuntu's i386 multiarch libraries, the game's
  own CMake with `PENUMBRA_BUILD_TESTS` on): before the fix the build stopped at the six errors above and nowhere else
  (the game's code, its third-party code and the seventeen suites compiled); after it, every target built with no new
  warning, and `test_pn_all` ran every suite as a 32-bit process (the summary line is in DEVLOG.md's entry).
- The engine on its own at `-m32` (editor, plugin and the suites on): built; `ctest` ran its 59 entries with one
  failure, `test_tilemap`'s segfault above; with the count fixed, the result is in DEVLOG.md.
- The engine at 64 bits, in full: built, `ctest` run (DEVLOG.md), `test_shadowcache` green with its six new checks.
- `tools/build_android.sh` on Linux: `bash -n` clean; run without an SDK it stops at the first missing tool, as it should.
- `android.yml` on GitHub (the run is named in DEVLOG.md): the three libraries in the APK, the `native-code` line
  `'arm64-v8a' 'armeabi-v7a' 'x86_64'`, the APK's size.

## Not verified, and open

- **No 32-bit phone has run it.** The `armeabi-v7a` library is compiled and linked, never started; the reporter's phone is
  the first that will. The Smart 9's GPU is the A05's (E47), so the soft picture of that report is to be expected there,
  and whether a 32-bit process is slower than the 64-bit one on the same phone is not known.
- Whether the reporter's phone is in fact 32-bit: asked for the ABI list; a phone that lists `arm64-v8a` and still refuses
  the APK is another report.
- `x86` (the 32-bit emulator ABI) is not built: no current emulator image needs it.
- The release APK grows by one library (the size is in DEVLOG.md); a player downloads all three ABIs, as every APK outside
  the Play Store does.
