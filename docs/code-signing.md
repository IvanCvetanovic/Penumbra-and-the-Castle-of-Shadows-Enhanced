# Code signing policy

This is the code signing policy of **Penumbra and the Castle of Shadows - Enhanced**
(<https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced>). It says which
files are signed, how they are built, who may approve a signature, and what the program does with
information.

## Status today

| Download | Signed? |
|---|---|
| `Penumbra-Windows.zip` (Windows) | **Not signed yet.** Windows shows "Windows protected your PC" the first time; click *More info*, then *Run anyway*. On PCs where Smart App Control is on, an unsigned program cannot run at all. |
| `Penumbra-Android.apk` (Android) | Signed with the project's own release key (below). |

The project has prepared an application to the [SignPath Foundation](https://signpath.org/), which
signs open-source software for free, for the Windows build. If it is accepted, signed Windows
releases will carry this notice:

> Free code signing provided by [SignPath.io](https://signpath.io/), certificate by
> [SignPath Foundation](https://signpath.org/).

Until then, no Windows file of this project is signed by anyone, and no other certificate is used.

## What is signed

**Windows.** One file: `Penumbra/Penumbra.exe` inside `Penumbra-Windows.zip`, the only executable
this project builds for players. Nothing else in the zip is signed by this project:

- `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` (and the other Visual C++ runtime files)
  are Microsoft's redistributable runtime, already signed by Microsoft, copied unchanged;
- `original/` holds the data of the 2010 game (art, music, sounds, levels). It is data read by the
  game, never executed, and it belongs to the original game's authors (see [`LICENSE.md`](../LICENSE.md));
- `data/`, `assets/shaders/`, the licences and the documents are data and text.

The signed program must have its product name set to *Penumbra and the Castle of Shadows - Enhanced*
and its product version to the release's version (`1.0.0` for the first release), from the build
itself (`CMakeLists.txt`, `game/windows/Penumbra.rc.in`).

**Android.** `Penumbra-Android.apk` is signed with the project's release key, which is kept offline
by the maintainer and never stored in the repository. Every release is signed with the same key, so
Android accepts each one as an update of the last. The certificate's SHA-256 fingerprint is:

```
AC:1D:43:BD:18:C5:85:27:E2:D3:10:72:17:3A:39:B5:1A:2A:95:19:6C:56:0E:8D:FE:EA:6C:DF:13:BD:66:DA
```

Anyone can check a downloaded APK with the Android SDK: `apksigner verify --print-certs
Penumbra-Android.apk` must print this fingerprint as the signer's SHA-256 digest.

## How a signed release is made

1. The release is built from the public source in this repository, at a commit on `main`, by
   GitHub Actions: the Windows job of [`.github/workflows/ci.yml`](../.github/workflows/ci.yml)
   builds with `tools\build.bat`, runs the test suites, and makes the release folder with
   `tools\make_release.bat` (the same scripts a developer runs; see [`building.md`](building.md)).
2. The job uploads that folder, unsigned, as the build artifact `Penumbra-Windows-unsigned`.
3. That artifact, and only that, is submitted for signing. A person with the approver role checks
   that it comes from the expected commit and workflow run, and approves the request by hand. There
   is no automatic approval.
4. The signed zip is attached to the GitHub release, with its SHA-256 checksum.

Files built on a personal computer are never submitted for signing.

## Team roles

| Role | Members | What they do |
|---|---|---|
| Author (committer) | Ivan Cvetanović ([@IvanCvetanovic](https://github.com/IvanCvetanovic)) | May change the source code. Changes are written by him or with Claude Code (an AI coding tool) under his direction; he reviews every change before it is committed. |
| Reviewer | Ivan Cvetanović | Reviews every contribution from anyone else (a pull request) before it is merged. |
| Approver | Ivan Cvetanović | Approves each signing request, one release at a time. |

Multi-factor authentication (MFA) will be required for the maintainer before signing starts.

## Privacy

This program will not transfer any information to other networked systems unless specifically
requested by the user or the person installing or operating it.

In detail: the game has no network code. It opens no connections, sends nothing and downloads
nothing; it has no ads, purchases, accounts, analytics or crash reporting. The Windows executable
imports no networking library (no Winsock, WinHTTP or WinINet), and the Android app does not ask
for the internet permission, so Android would not let it connect even if it tried. What it keeps,
it keeps on the player's own computer or phone: settings, high scores, the last checkpoint and a
log of the last run (on Windows in `%APPDATA%\Penumbra`).

## Installing and removing

The Windows game is not installed: it runs from the folder it was extracted to, and it writes only
inside that folder and `%APPDATA%\Penumbra`. To remove it, delete both. While it runs in
fullscreen, it may switch the monitor to another resolution or refresh rate (chosen in the game's
options). On Android, uninstall it like any other app.
