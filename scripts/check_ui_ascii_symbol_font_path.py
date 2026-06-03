#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GFX_CPP = ROOT / "lib" / "GfxRenderer" / "GfxRenderer.cpp"
GFX_H = ROOT / "lib" / "GfxRenderer" / "GfxRenderer.h"


def require(text: str, needle: str, failures: list[str]) -> None:
    if needle not in text:
        failures.append(f"missing {needle!r}")


def main() -> int:
    cpp = GFX_CPP.read_text(encoding="utf-8")
    header = GFX_H.read_text(encoding="utf-8")

    failures: list[str] = []
    require(header, "bool allowBuiltInUiGlyph", failures)
    require(cpp, "const bool allowBuiltInUiGlyph = fontFamily.getGlyph(cp, style) == nullptr;", failures)
    require(cpp, "if (allowBuiltInUiGlyph && CjkUiFont20::hasCjkUiGlyph(cp))", failures)
    require(cpp, "fontFamily.getGlyph(testCp, style) == nullptr", failures)
    require(cpp, "fontFamily.getGlyph(cp, style) == nullptr", failures)

    if failures:
      print("ASCII UI symbols with EPD glyphs must not use CJK UI fallback glyphs.")
      for failure in failures:
          print(f"- {failure}")
      return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
