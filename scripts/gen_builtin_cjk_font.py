#!/usr/bin/env python3
"""Generate the built-in CJK UI font header before PlatformIO builds."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys


FONT_SIZE = 20
FONT_PATH = Path("fonts/思源黑体-Bold.otf")
OUTPUT_PATH = Path("lib/GfxRenderer/cjk_ui_font_20.h")
GENERATOR_PATH = Path("scripts/generate_cjk_ui_font.py")
TRANSLATIONS_PATH = Path("lib/I18n/translations")
DEFAULT_LANGUAGE_FILTER = ["ENGLISH", "CHINESE_SIMPLIFIED", "CHINESE_TRADITIONAL", "JAPANESE"]


def split_language_filter(raw: str) -> list[str]:
    return [lang.strip() for lang in raw.split(",") if lang.strip()]


def generate_builtin_cjk_font(language_filter: list[str] | None = None) -> None:
    project_root = Path(__file__).resolve().parent.parent
    font_path = project_root / FONT_PATH
    output_path = project_root / OUTPUT_PATH
    generator_path = project_root / GENERATOR_PATH
    translations_path = project_root / TRANSLATIONS_PATH

    if not font_path.is_file():
        print(f"Error: built-in CJK font source not found: {font_path}")
        sys.exit(1)

    cmd = [
        sys.executable,
        str(generator_path),
        "--size",
        str(FONT_SIZE),
        "--font",
        str(font_path),
        "--output",
        str(output_path),
        "--translations-dir",
        str(translations_path),
    ]
    if language_filter:
        cmd.extend(["--languages", ",".join(language_filter)])

    print("Generating built-in CJK UI font...", flush=True)
    if language_filter:
        print(f"  Languages: {', '.join(language_filter)}", flush=True)
    else:
        print("  Languages: all translations", flush=True)

    result = subprocess.run(cmd, check=False)
    if result.returncode != 0:
        sys.exit(result.returncode)

    print(f"Built-in CJK UI font generated: {output_path}")


def main() -> None:
    generate_builtin_cjk_font(DEFAULT_LANGUAGE_FILTER)


if __name__ == "__main__":
    main()
else:
    try:
        Import("env")
        _language_filter = None
        try:
            _languages = env.GetProjectOption("custom_i18n_languages", "")
            if _languages:
                _language_filter = split_language_filter(_languages)
        except Exception:
            pass
        generate_builtin_cjk_font(_language_filter)
    except NameError:
        pass
