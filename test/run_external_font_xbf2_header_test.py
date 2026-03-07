#!/usr/bin/env python3

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_FILE = REPO_ROOT / "test" / "ExternalFontXbf2HeaderTest.cpp"
EXTERNAL_FONT_H = REPO_ROOT / "lib" / "ExternalFont" / "ExternalFont.h"
EXTERNAL_FONT_CPP = REPO_ROOT / "lib" / "ExternalFont" / "ExternalFont.cpp"
HOST_STUBS = REPO_ROOT / "test" / "support" / "host_stubs"

HEADER_REMOVE_SNIPPETS = [
    "struct ExternalGlyphMetrics {\n  uint8_t width = 0;\n  uint8_t height = 0;\n  uint8_t advanceX = 0;\n  int16_t left = 0;\n  int16_t top = 0;\n};\n\n",
    "struct ExternalFontMetrics {\n  int16_t ascender = 0;\n  int16_t descender = 0;\n  uint16_t lineHeight = 0;\n};\n\n",
    "  bool isRichMetricsFormat() const { return _isRichMetricsFormat; }\n",
    "  int16_t getAscender() const { return _fontMetrics.ascender; }\n",
    "  int16_t getDescender() const { return _fontMetrics.descender; }\n",
    "  uint16_t getLineHeight() const { return _fontMetrics.lineHeight; }\n",
    "  bool _isRichMetricsFormat = false;\n",
    "  ExternalFontMetrics _fontMetrics;\n",
]

CPP_REMOVE_SNIPPETS = [
    "namespace {\n\nconstexpr uint8_t XBF2_MAGIC[4] = {'X', 'B', 'F', '2'};\nconstexpr size_t XBF2_HEADER_SIZE = 12;\n\nint16_t readInt16LE(const uint8_t* bytes) {\n  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8));\n}\n\nuint16_t readUint16LE(const uint8_t* bytes) {\n  return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);\n}\n\n}  // namespace\n\n",
    "  _isRichMetricsFormat = false;\n  _fontMetrics = {};\n",
    "  uint8_t header[XBF2_HEADER_SIZE];\n  const size_t headerBytesRead = _fontFile.read(header, sizeof(header));\n  if (headerBytesRead == sizeof(header) && memcmp(header, XBF2_MAGIC, sizeof(XBF2_MAGIC)) == 0) {\n    _isRichMetricsFormat = true;\n    _charWidth = header[4];\n    _charHeight = header[5];\n    _bytesPerRow = (_charWidth + 7) / 8;\n    _bytesPerChar = _bytesPerRow * _charHeight;\n    _fontMetrics.ascender = readInt16LE(header + 6);\n    _fontMetrics.descender = readInt16LE(header + 8);\n    _fontMetrics.lineHeight = readUint16LE(header + 10);\n\n    if (_bytesPerChar > MAX_GLYPH_BYTES) {\n      LOG_ERR(\"EFT\", \"Glyph too large: %d bytes (max %d)\", _bytesPerChar, MAX_GLYPH_BYTES);\n      _fontFile.close();\n      return false;\n    }\n  } else if (!_fontFile.seek(0)) {\n    _fontFile.close();\n    return false;\n  }\n\n",
]


def rewrite_without_task1(source: Path, destination: Path, snippets: list[str]) -> None:
    content = source.read_text()
    for snippet in snippets:
        if snippet not in content:
            raise RuntimeError(f"Expected snippet not found in {source}")
        content = content.replace(snippet, "", 1)
    destination.write_text(content)



def compile_test(header: Path, implementation: Path, output: Path) -> subprocess.CompletedProcess[str]:
    command = [
        "g++",
        "-std=c++20",
        f"-I{HOST_STUBS}",
        f"-I{header.parent}",
        str(TEST_FILE),
        str(implementation),
        "-o",
        str(output),
    ]
    return subprocess.run(command, text=True, capture_output=True, cwd=REPO_ROOT)



def run_binary(binary: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(binary)], text=True, capture_output=True, cwd=REPO_ROOT)



def print_result(result: subprocess.CompletedProcess[str]) -> None:
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)



def main() -> int:
    mode = sys.argv[1] if len(sys.argv) > 1 else "pass"
    if mode not in {"pass", "fail"}:
        print("Usage: test/run_external_font_xbf2_header_test.py [pass|fail]", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="xbf2-header-test-") as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        binary = temp_dir / "ExternalFontXbf2HeaderTest"

        if mode == "fail":
            header = temp_dir / "ExternalFont.h"
            implementation = temp_dir / "ExternalFont.cpp"
            rewrite_without_task1(EXTERNAL_FONT_H, header, HEADER_REMOVE_SNIPPETS)
            rewrite_without_task1(EXTERNAL_FONT_CPP, implementation, CPP_REMOVE_SNIPPETS)
            result = compile_test(header, implementation, binary)
            print_result(result)
            if result.returncode == 0:
                print("Expected fail mode to fail, but compilation succeeded.", file=sys.stderr)
                return 1
            return 0

        result = compile_test(EXTERNAL_FONT_H, EXTERNAL_FONT_CPP, binary)
        print_result(result)
        if result.returncode != 0:
            return result.returncode

        run_result = run_binary(binary)
        print_result(run_result)
        return run_result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
