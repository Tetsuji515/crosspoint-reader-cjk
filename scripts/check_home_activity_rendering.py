#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOME_ACTIVITY_CPP = ROOT / "src" / "activities" / "home" / "HomeActivity.cpp"
HOME_ACTIVITY_H = ROOT / "src" / "activities" / "home" / "HomeActivity.h"


def main() -> int:
    source = HOME_ACTIVITY_CPP.read_text(encoding="utf-8")
    header = HOME_ACTIVITY_H.read_text(encoding="utf-8")

    forbidden = [
        ("HomeActivity.cpp", "setPartialUpdateRect("),
        ("HomeActivity.cpp", "menuOnlyPartialUpdate"),
        ("HomeActivity.h", "menuOnlyPartialUpdate"),
        ("HomeActivity.h", "fullRedrawRequired"),
        ("HomeActivity.h", "setMenuPartialUpdateIfSafe"),
    ]

    failures = []
    for filename, needle in forbidden:
        text = source if filename.endswith(".cpp") else header
        if needle in text:
            failures.append(f"{filename} still contains {needle!r}")

    if failures:
        print("HomeActivity must fully redraw the home screen so stale cover selection state is erased.")
        for failure in failures:
            print(f"- {failure}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
