#!/usr/bin/env python3
"""
Generate a Japanese reading-font .epdfont for CrossPoint Reader CJK, using the
practical Unicode range for Japanese book text (not the full x4-epdfont-converter
default set, which additionally covers Hangul, emoji, and rare CJK extension
blocks not needed here and would needlessly bloat the file/flash usage).

Reuses the glyph rasterization and .epdfont serialization from
scripts/font/ttf_to_epdfont.py (vendored from eunchurn/x4-epdfont-converter,
confirmed byte-for-byte compatible with the firmware's ExternalFont loader --
see docs/dev-notes/reading-font-format-survey.md).

Usage:
    python generate_jp_reading_font.py <font.ttf> <size> <output.epdfont>
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from ttf_to_epdfont import load_glyph, GlyphProps, write_epdfont, norm_floor, norm_ceil
import freetype


def _jis_level1_kanji_codepoints() -> set:
    """JIS X 0208 Level 1 kanji (2,965 characters, ku 16-47), enumerated via
    Python's built-in euc_jp codec rather than a hardcoded list. Verified to
    produce exactly 2965 codepoints matching the standard's known count."""
    cps = set()
    for ku in range(16, 48):
        for ten in range(1, 95):
            b1, b2 = 0xA0 + ku, 0xA0 + ten
            try:
                cps.add(ord(bytes([b1, b2]).decode("euc_jp")))
            except UnicodeDecodeError:
                pass
    return cps


def _codepoints_to_intervals(codepoints) -> list:
    pts = sorted(codepoints)
    intervals = []
    start = prev = pts[0]
    for cp in pts[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        intervals.append((start, prev))
        start = prev = cp
    intervals.append((start, prev))
    return intervals


# Practical Japanese reading range, sized to fit comfortably in the ~150-200KB
# heap actually available while reading (see docs/dev-notes -- a first attempt
# using the full CJK Unified Ideographs block, ~13.8k glyphs/2.3MB, left only
# ~46-48KB free heap while reading and crashed with std::bad_alloc soon after).
# Kanji coverage is JIS X 0208 Level 1 (2,965 chars, common-use + then some)
# instead of the ~20,992-char CJK Unified block. Non-kanji ranges (kana,
# punctuation, ASCII) are cheap and kept in full.
# NOTE: must stay sorted by start -- the firmware binary-searches the interval
# table assuming ascending order (see ExternalFont.cpp's findEpdInterval()).
JP_INTERVALS = sorted(
    [
        (0x0000, 0x007F),  # Basic Latin (ASCII)
        (0x0080, 0x00FF),  # Latin-1 Supplement
        (0x2000, 0x206F),  # General Punctuation
        (0x3000, 0x303F),  # CJK Symbols and Punctuation
        (0x3040, 0x309F),  # Hiragana
        (0x30A0, 0x30FF),  # Katakana
        (0x31F0, 0x31FF),  # Katakana Phonetic Extensions
        (0xFF00, 0xFFEF),  # Halfwidth and Fullwidth Forms
    ]
    + _codepoints_to_intervals(_jis_level1_kanji_codepoints())
)


def generate(font_path: str, size: int, output_path: str) -> None:
    font_stack = [freetype.Face(font_path)]
    for face in font_stack:
        face.set_pixel_sizes(0, size)

    all_glyphs = []
    total_size = 0
    validated_intervals = []

    for i_start, i_end in JP_INTERVALS:
        start = i_start
        for code_point in range(i_start, i_end + 1):
            face = load_glyph(font_stack, code_point)
            if face is None:
                if start < code_point:
                    validated_intervals.append((start, code_point - 1))
                start = code_point + 1
                continue

            bitmap = face.glyph.bitmap
            pixels4g = []
            px = 0
            for i, v in enumerate(bitmap.buffer):
                x = i % bitmap.width if bitmap.width > 0 else 0
                if x % 2 == 0:
                    px = v >> 4
                else:
                    px = px | (v & 0xF0)
                    pixels4g.append(px)
                    px = 0
                if bitmap.width > 0 and x == bitmap.width - 1 and bitmap.width % 2 > 0:
                    pixels4g.append(px)
                    px = 0

            pixelsbw = []
            px = 0
            pitch = (bitmap.width // 2) + (bitmap.width % 2)
            for y in range(bitmap.rows):
                for x in range(bitmap.width):
                    px = px << 1
                    if pitch > 0 and len(pixels4g) > 0:
                        idx = y * pitch + (x // 2)
                        bm = pixels4g[idx] if idx < len(pixels4g) else 0
                        px += 1 if ((x & 1) == 0 and bm & 0xE > 0) or ((x & 1) == 1 and bm & 0xE0 > 0) else 0
                    if (y * bitmap.width + x) % 8 == 7:
                        pixelsbw.append(px)
                        px = 0
            if (bitmap.width * bitmap.rows) % 8 != 0:
                px = px << (8 - (bitmap.width * bitmap.rows) % 8)
                pixelsbw.append(px)
            packed = bytes(pixelsbw)

            glyph = GlyphProps(
                width=bitmap.width,
                height=bitmap.rows,
                advance_x=norm_floor(face.glyph.advance.x),
                left=face.glyph.bitmap_left,
                top=face.glyph.bitmap_top,
                data_length=len(packed),
                data_offset=total_size,
                code_point=code_point,
            )
            total_size += len(packed)
            all_glyphs.append((glyph, packed))
        if start <= i_end:
            validated_intervals.append((start, i_end))

    face = load_glyph(font_stack, ord('|')) or font_stack[0]
    advance_y = norm_ceil(face.size.height)
    ascender = norm_ceil(face.size.ascender)
    descender = norm_floor(face.size.descender)

    print(f"Requested intervals: {len(JP_INTERVALS)}, validated (glyph-present) intervals: {len(validated_intervals)}")
    print(f"Total glyphs: {len(all_glyphs)}")
    write_epdfont(output_path, validated_intervals, all_glyphs, advance_y, ascender, descender, is_2bit=False)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", help="Path to TTF/OTF font file")
    parser.add_argument("size", type=int, help="Font size in pixels")
    parser.add_argument("output", help="Output .epdfont path")
    args = parser.parse_args()
    generate(args.font, args.size, args.output)


if __name__ == "__main__":
    main()
