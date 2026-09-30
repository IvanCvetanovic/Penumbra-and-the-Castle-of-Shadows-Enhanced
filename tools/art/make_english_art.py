#!/usr/bin/env python3
"""English variants of the original's images that have Portuguese baked into their pixels (E5).

The English is one language of tools/art/make_localized_art.py (E24), whose docstring describes the
images, the fitted models and the faces; this is that tool with --lang en, kept for the commands
already written down:

    python tools/art/make_english_art.py            # write game/data/images/en
    python tools/art/make_english_art.py --verify   # measure the models against the originals

Any other option of make_localized_art.py passes through (--check, --out-dir DIR).
"""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from make_localized_art import main  # noqa: E402

if __name__ == "__main__":
    main(["--lang", "en"] + sys.argv[1:])
