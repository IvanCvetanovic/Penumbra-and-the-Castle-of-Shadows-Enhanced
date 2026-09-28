/*
 * dr_mp3's implementation, compiled once, in C, with its own warnings silenced
 * (game/eth/CMakeLists.txt), as TinyXML is.
 *
 * dr_mp3.h is an unmodified copy of
 *   https://github.com/mackron/dr_libs/blob/51e61d308dde6b437fce0c5fabb32cd86b40f4d7/dr_mp3.h
 *   (v0.7.4 in development, 2026-08-31: 0.7.3 plus the fixes for an overflow
 *   and a heap-buffer over-read in the Xing/Info tag parser)
 *   sha256 997b7ee18de6e6b81e2a83f1ea9fc62aef25c62b28d48db95635f49e65de0a2f
 * by David Reid, based on minimp3 by Lion (lieff). Licence: the choice of
 * public domain (Unlicense) or MIT No Attribution, stated at the end of
 * dr_mp3.h; minimp3's part is CC0. Neither asks for attribution.
 *
 * Why the port carries an MP3 decoder at all: eth/SoundDecode.hpp.
 */

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
