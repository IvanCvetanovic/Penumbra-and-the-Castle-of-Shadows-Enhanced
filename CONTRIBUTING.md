# Contributing

Thank you for helping. A few things are especially welcome.

## What helps most

- **Play reports.** What has and has not been checked on each platform is in the README's [Platform status](README.md#platform-status) table. If you play on one that no person has played yet, tell us how it went, working or not, by opening an [issue](https://github.com/IvanCvetanovic/Penumbra-and-the-Castle-of-Shadows-Enhanced/issues/new/choose).
- **Translation fixes.** The game has eleven languages. The text of each is in `game/data/strings.json` (English) and `game/data/strings/<id>.json`. Every text has a room on screen (`tests/data/l10n_rooms.json`), and the tests check that it fits, so a corrected text goes in the same file and the tests tell you if it is too long. If you change `ja.json`, rerun `tools/l10n/make_fonts.py`.
- **Bugs and code.** Please open an issue first for anything large.

## Building and testing

- Clone with `git clone --recurse-submodules` (the engine is a submodule), then follow [`docs/building.md`](docs/building.md). The original game's files are already in `extracted/`; nothing else needs to be downloaded.
- Run the tests as described in [`docs/testing.md`](docs/testing.md): `test_pn_all` holds every suite, and `bash tools/build_linux.sh --test` builds and runs it on Linux.
- On Windows 11, Smart App Control can refuse to run a program you have just built. Building and testing on Linux (or WSL) avoids that.

## Rules for code

- The ported scripts in `game/script/` follow the original's AngelScript line by line, with the original's names and order. Change them only to fix a difference from the original, and say which one.
- Never change anything in `extracted/`: it is the original game, and the game reads it in place.
- Style: four spaces, PascalCase types and public methods, camelCase locals, `m_` members, `k` constants; comments explain why, not what. The build has no warnings, and a change should keep it so.
- New enhancements are listed in [`docs/enhancements.md`](docs/enhancements.md) with what the original did.

## Pull requests

- Keep each pull request to one change. Say what was wrong before and what you checked afterwards.
- Run `test_pn_all` and say what it printed.
- Title: `Penumbra: <what changed>`.

## Licences of what you contribute

New code is under the same terms as the code around it: MIT No Attribution for most of the repository, and LGPL-3.0-or-later for `game/script/` and `game/eth/` ([`LICENSE.md`](LICENSE.md) has the table). Please do not add other people's art, music or code that the licences there do not already cover.
