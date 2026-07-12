#!/usr/bin/env python3
"""
Small-scope verification build: reuses ttf_to_epdfont.py's glyph/write logic
but with a tiny custom interval list (ASCII + the instruction doc's sample
Japanese phrase) instead of the script's huge default interval set, so the
resulting .epdfont file is fast to generate and easy to hex-dump/verify by
hand against the format documented in docs/dev-notes/reading-font-format-survey.md.
"""
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from ttf_to_epdfont import load_glyph, GlyphProps, write_epdfont, norm_floor, norm_ceil
import freetype

FONT_PATH = os.path.join(os.path.dirname(__file__), "assets", "NotoSerifJP-Regular.ttf")
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "assets", "test_small.epdfont")
SIZE = 38

SAMPLE_TEXT = "あいうテスト用電子書籍"
codepoints = sorted(set(ord(c) for c in SAMPLE_TEXT) | set(range(0x20, 0x7F)))

# Build intervals (contiguous runs) from the codepoint set, matching the
# original script's interval-based storage model.
codepoints.sort()
intervals = []
start = prev = codepoints[0]
for cp in codepoints[1:]:
    if cp == prev + 1:
        prev = cp
        continue
    intervals.append((start, prev))
    start = prev = cp
intervals.append((start, prev))

font_stack = [freetype.Face(FONT_PATH)]
for face in font_stack:
    face.set_pixel_sizes(0, SIZE)

all_glyphs = []
total_size = 0
for i_start, i_end in intervals:
    for code_point in range(i_start, i_end + 1):
        face = load_glyph(font_stack, code_point)
        if face is None:
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

face = load_glyph(font_stack, ord('|')) or font_stack[0]
advance_y = norm_ceil(face.size.height)
ascender = norm_ceil(face.size.ascender)
descender = norm_floor(face.size.descender)

print(f"Test glyphs: {len(all_glyphs)}, intervals: {len(intervals)}")
write_epdfont(OUTPUT_PATH, intervals, all_glyphs, advance_y, ascender, descender, is_2bit=False)
