# Security

## Which versions get fixes

The latest release, the one marked **Latest** on the [Releases page](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/releases).

## Reporting a security problem

If you find a security problem in the game, in one of its downloads or in this repository, please report it privately: open the **Security** tab of this repository and choose **Report a vulnerability**. Please do not describe it in a public issue.

This is a small project kept by one person, so please be patient.

## What is in scope

The game never connects to the internet: it has no network code, no accounts and no server. What can matter is what you download and what the game reads from your disk. For example:

- a download that does not match its checksum in `SHA256SUMS.txt`;
- an APK that is not signed with the key named in [`docs/code-signing.md`](docs/code-signing.md);
- a way to make the game run something it should not through a crafted save or settings file;
- a secret or a mistake in the build and release scripts of this repository.

## Good to know

- The Android app does not ask for the internet permission ([`docs/code-signing.md`](docs/code-signing.md#privacy)).
- Every release lists the SHA-256 checksum of each download in `SHA256SUMS.txt`. The Android APK is signed with the project's release key; its fingerprint is in [`docs/code-signing.md`](docs/code-signing.md).
- The Windows download is not code-signed yet, so Windows warns before it runs.
