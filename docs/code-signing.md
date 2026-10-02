# Code signing policy

This is the code signing policy of **Penumbra and the Castle of Shadows - Enhanced**
(<https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced>). It says which
files are signed, how they are built, who may approve a signature, and what the program does with
information.

## Status today

| Download | Signed? |
|---|---|
| `Penumbra-Windows.zip` (Windows) | **Not signed yet.** Windows shows "Windows protected your PC" the first time; click *More info*, then *Run anyway*. On Windows 11 PCs where Smart App Control is on, an unsigned program may be refused, with no *Run anyway* button. |
| `Penumbra-Android.apk` (Android) | Signed with the project's own release key (below). |
| `Penumbra-macOS.zip` (Mac) | **Signed ad hoc only, not notarised by Apple.** No certificate is used. The first open is stopped by macOS; the player allows it once (*Open Anyway* in System Settings, Privacy & Security, or *Open* from the app's Control-click menu on macOS 13 and 14). |
| `Penumbra-Linux.tar.gz` (Linux) | **Not signed.** Its SHA-256 checksum is published with the release, in `SHA256SUMS.txt`. |
| `Penumbra-iOS.ipa` (iPhone, iPad; experimental) | **Not signed.** It carries no certificate, only the ad-hoc seal the build gives every app bundle. The player's sideloading tool (Sideloadly, AltStore or SideStore) signs it with the player's own Apple ID when it installs it. |

The project has prepared an application to the [SignPath Foundation](https://signpath.org/), which
signs open-source software for free, for the Windows build. If it is accepted, signed Windows
releases will carry this notice:

> Free code signing provided by [SignPath.io](https://signpath.io/), certificate by
> [SignPath Foundation](https://signpath.org/).

Until then, no Windows file of this project is signed by anyone, and no other certificate is used.

## What is signed

**Windows.** One file: `Penumbra/Penumbra.exe` inside `Penumbra-Windows.zip`, the only Windows
executable this project builds for players. Nothing else in the zip is signed by this project:

- `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` (and the other Visual C++ runtime files)
  are Microsoft's redistributable runtime, already signed by Microsoft, copied unchanged;
- `original/` holds the data of the 2010 game (art, music, sounds, levels). It is data read by the
  game, never executed, and it belongs to the original game's authors (see [`LICENSE.md`](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/blob/main/LICENSE.md));
- `data/`, `assets/shaders/`, the licences and the documents are data and text.

The signed program must have its product name set to *Penumbra and the Castle of Shadows - Enhanced*
and its product version to the release's version, from the build
itself (`CMakeLists.txt`, `game/windows/Penumbra.rc.in`).

**Android.** `Penumbra-Android.apk` is signed with the project's release key, which is kept offline
by the maintainer and never stored in the repository. Every release is signed with the same key, so
Android accepts each one as an update of the last. The certificate's SHA-256 fingerprint is:

```
AC:1D:43:BD:18:C5:85:27:E2:D3:10:72:17:3A:39:B5:1A:2A:95:19:6C:56:0E:8D:FE:EA:6C:DF:13:BD:66:DA
```

Anyone can check a downloaded APK with the Android SDK: `apksigner verify --print-certs
Penumbra-Android.apk` must print this fingerprint as the signer's SHA-256 digest.

**Mac.** `Penumbra.app` inside `Penumbra-macOS.zip` is signed ad hoc (`codesign --sign -`, in
[`tools/apple/make_app.sh`](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/blob/main/tools/apple/make_app.sh)): a signature with no certificate and no
identity, which seals the bundle, so that macOS can tell whether anything in it changed, and which
Apple silicon Macs require before they run a program at all. It is not notarised by Apple. The
release workflow unpacks the zip again and checks that seal strictly (`codesign --verify --deep
--strict`). `HOW TO PLAY.txt` sits beside the app, outside the seal.

**iPhone and iPad.** `Penumbra-iOS.ipa` is not signed for any device: the app inside carries only
the same ad-hoc seal, with no certificate. The player's sideloading tool signs it with the player's
own Apple ID when it installs it, so each installed copy is signed by that player, not by this
project.

**Linux.** Nothing in `Penumbra-Linux.tar.gz` is signed. The release's `SHA256SUMS.txt` holds its
checksum, as it does for every other download.

## How a signed release is made

1. The release is built from the public source in this repository, at a commit on `main`, by
   GitHub Actions: the Windows job of [`.github/workflows/ci.yml`](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/blob/main/.github/workflows/ci.yml)
   builds with `tools\build.bat`, runs the test suites, and makes the release folder with
   `tools\make_release.bat` (the same scripts a developer runs; see [`building.md`](building.md)).
2. The job uploads that folder, unsigned, as the build artifact `Penumbra-Windows-unsigned`.
3. That artifact, and only that, is submitted for signing. A person with the approver role checks
   that it comes from the expected commit and workflow run, and approves the request by hand. There
   is no automatic approval.
4. The signed zip is attached to the GitHub release, with its SHA-256 checksum.

Files built on a personal computer are never submitted for signing.

This is how Windows releases will be made once signing starts. The Windows zip is made on
the development machine (`tools\make_release.bat`) and is not signed; the Android APK is also made
there (`tools/build_android.sh --release`) and signed with the offline release key.

The Linux, Mac and iPhone/iPad files are built by GitHub Actions, by the
[release workflow](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/blob/main/.github/workflows/release.yml), from the public source at a commit of this
repository. It is run by hand and only builds and checks, unless it is asked to publish. It runs
every test suite on Linux, and runs the Linux and Mac downloads as a player gets them before it
keeps them: unpacked into a folder outside the source tree, the Linux one read-only and run as an
ordinary user. When it publishes, it checks the files
already in the release against their published checksums, adds the three new files, and rewrites
`SHA256SUMS.txt` for all five, with the Windows and Android lines unchanged. It never replaces the
Windows zip or the APK. The three files are built from the commit the run starts on, which may be
later than the release's tag (the publish job's summary names that commit), so they can include
changes made since the tag.

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
log of the last run (on Windows in `%APPDATA%\Penumbra`, on Linux in `~/.local/share/Penumbra`,
on a Mac in `~/Library/Application Support/Penumbra`, and on an iPhone or iPad inside the app's
own container).

## Installing and removing

The Windows game is not installed: it runs from the folder it was extracted to, and it writes only
inside that folder and `%APPDATA%\Penumbra`. To remove it, delete both. While it runs in
fullscreen, it may switch the monitor to another resolution or refresh rate (chosen in the game's
options). On Linux, the game also runs from the folder it was extracted to; to remove it, delete
that folder and `~/.local/share/Penumbra`. On a Mac, move Penumbra from Applications to the Trash,
and delete `~/Library/Application Support/Penumbra` and the graphics cache in
`~/Library/Caches/com.ivancvetanovic.penumbra`. On Android, iPhone and iPad, uninstall it like any
other app.
