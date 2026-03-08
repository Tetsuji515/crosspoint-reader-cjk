#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
TEST_FILE = REPO_ROOT / "test" / "ExternalFontRenderMetricsTest.cpp"
METRICS_CPP = REPO_ROOT / "lib" / "GfxRenderer" / "ExternalFontRenderMetrics.cpp"
EXTERNAL_FONT_CPP = REPO_ROOT / "lib" / "ExternalFont" / "ExternalFont.cpp"
HOST_STUBS = REPO_ROOT / "test" / "support" / "host_stubs"
GFX_INCLUDE = REPO_ROOT / "lib" / "GfxRenderer"
EXTERNAL_FONT_INCLUDE = REPO_ROOT / "lib" / "ExternalFont"
FAIL_STUB = REPO_ROOT / "test" / "support" / "external_font_render_metrics_task3_fail_stub.cpp"


def compile_test(sources: list[Path], output: Path) -> subprocess.CompletedProcess[str]:
    command = [
        "g++",
        "-std=c++20",
        f"-I{HOST_STUBS}",
        f"-I{GFX_INCLUDE}",
        f"-I{EXTERNAL_FONT_INCLUDE}",
        *(str(source) for source in sources),
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
        print("Usage: test/run_external_font_render_metrics_test.py [pass|fail]", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="external-font-render-metrics-test-") as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        binary = temp_dir / "ExternalFontRenderMetricsTest"

        if mode == "fail":
            result = compile_test([TEST_FILE, FAIL_STUB, EXTERNAL_FONT_CPP], binary)
            print_result(result)
            if result.returncode != 0:
                return result.returncode

            run_result = run_binary(binary)
            print_result(run_result)
            if run_result.returncode == 0:
                print("Expected fail mode to fail, but test passed.", file=sys.stderr)
                return 1
            return 0

        result = compile_test([TEST_FILE, METRICS_CPP, EXTERNAL_FONT_CPP], binary)
        print_result(result)
        if result.returncode != 0:
            return result.returncode

        run_result = run_binary(binary)
        print_result(run_result)
        return run_result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
