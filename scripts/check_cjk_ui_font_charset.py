#!/usr/bin/env python3
"""Verify the built-in CJK UI font charset is derived from translations."""

import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from generate_cjk_ui_font import build_ui_chars, get_unique_chars  # noqa: E402


def write_translation(path: Path, language_name: str, language_code: str, value: str) -> None:
    path.write_text(
        "\n".join(
            [
                f'_language_name: "{language_name}"',
                f'_language_code: "{language_code}"',
                'STR_CROSSPOINT: "CrossPoint"',
                f'STR_TEST_ONLY: "{value}"',
                "",
            ]
        ),
        encoding="utf-8",
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as tmpdir:
        translations_dir = Path(tmpdir)
        write_translation(translations_dir / "english.yaml", "English", "EN", "English")
        write_translation(translations_dir / "chinese_simplified.yaml", "简体中文", "CHINESE_SIMPLIFIED", "鱻龘")
        write_translation(translations_dir / "japanese.yaml", "日本語", "JAPANESE", "麒麟")

        chars = set(get_unique_chars(build_ui_chars(translations_dir, ["CHINESE_SIMPLIFIED"])))
        expected = {"鱻", "龘"}
        unexpected = {"麒", "麟"}

        if not expected.issubset(chars):
            print("Generated UI charset missed selected translation characters.")
            print(f"Missing: {sorted(expected - chars)}")
            return 1

        if chars.intersection(unexpected):
            print("Generated UI charset included filtered-out language characters.")
            print(f"Unexpected: {sorted(chars.intersection(unexpected))}")
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
