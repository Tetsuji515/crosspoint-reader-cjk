#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

CASES = [
    {
        "name": "SettingsActivity uses shared external font label helper",
        "path": ROOT / "src" / "activities" / "settings" / "SettingsActivity.cpp",
        "required": [
            '#include "util/ExternalFontLabel.h"',
            'buildExternalFontLabel(info->filename, info->name, info->size,',
        ],
        "forbidden": [
            'valueText = info ? info->name : tr(STR_EXTERNAL_FONT);',
        ],
    },
    {
        "name": "EpubReaderMenuActivity uses shared external font label helper",
        "path": ROOT / "src" / "activities" / "reader" / "EpubReaderMenuActivity.cpp",
        "required": [
            '#include "util/ExternalFontLabel.h"',
            'return info ? buildExternalFontLabel(info->filename, info->name, info->size,',
        ],
        "forbidden": [
            'return info ? std::string(info->name) : std::string(tr(STR_EXTERNAL_FONT));',
        ],
    },
    {
        "name": "FontSelectActivity list items use shared external font label helper",
        "path": ROOT / "src" / "activities" / "settings" / "FontSelectActivity.cpp",
        "required": [
            '#include "util/ExternalFontLabel.h"',
            'return buildExternalFontLabel(info->filename, info->name, info->size, loadable);',
        ],
        "forbidden": [
            'snprintf(label, sizeof(label), "%s (%dpt)%s", info->name, info->size, loadable ? "" : " [!]");',
        ],
    },
    {
        "name": "CrossPointWebServer options use shared external font label helper",
        "path": ROOT / "src" / "network" / "CrossPointWebServer.cpp",
        "required": [
            '#include "util/ExternalFontLabel.h"',
            'std::string label = buildExternalFontLabel(info->filename, info->name, info->size,',
        ],
        "forbidden": [
            'std::string label = std::string(info->name) + " (" + std::to_string(info->size) + "pt)";',
        ],
    },
]


def main() -> int:
    failures: list[str] = []

    for case in CASES:
        content = case["path"].read_text(encoding="utf-8")
        for needle in case["required"]:
            if needle not in content:
                failures.append(f"{case['name']}: missing required snippet: {needle}")
        for needle in case["forbidden"]:
            if needle in content:
                failures.append(f"{case['name']}: forbidden snippet still present: {needle}")

    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        return 1

    print("EXTERNAL_FONT_DISPLAY_PATHS_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
