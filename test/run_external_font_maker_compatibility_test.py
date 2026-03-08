#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_FILE = REPO_ROOT / "test" / "ExternalFontMakerCompatibilityTest.cpp"
EXTERNAL_FONT_H = REPO_ROOT / "lib" / "ExternalFont" / "ExternalFont.h"
EXTERNAL_FONT_CPP = REPO_ROOT / "lib" / "ExternalFont" / "ExternalFont.cpp"
HOST_STUBS = REPO_ROOT / "test" / "support" / "host_stubs"


def compile_test(output: Path) -> subprocess.CompletedProcess[str]:
    command = [
        "g++",
        "-std=c++20",
        f"-I{HOST_STUBS}",
        f"-I{EXTERNAL_FONT_H.parent}",
        str(TEST_FILE),
        str(EXTERNAL_FONT_CPP),
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
    with tempfile.TemporaryDirectory(prefix="external-font-maker-compat-") as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        binary = temp_dir / "ExternalFontMakerCompatibilityTest"

        result = compile_test(binary)
        print_result(result)
        if result.returncode != 0:
            return result.returncode

        run_result = run_binary(binary)
        print_result(run_result)
        return run_result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
