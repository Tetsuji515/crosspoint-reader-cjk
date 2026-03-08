#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory() as tmp:
    binary = Path(tmp) / "ExternalFontLabelFormatTest"
    cmd = [
        "g++",
        "-std=c++17",
        "-I", str(ROOT / "lib" / "ExternalFont"),
        "-I", str(ROOT / "src"),
        "-I", str(ROOT / "src" / "util"),
        str(ROOT / "test" / "ExternalFontLabelFormatTest.cpp"),
        str(ROOT / "src" / "util" / "ExternalFontLabel.cpp"),
        "-o", str(binary),
    ]
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True)
