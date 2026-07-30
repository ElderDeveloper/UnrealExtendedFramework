#!/usr/bin/env python3
"""Reject new ad-hoc visual literals in focused Backlot surface widgets."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[2]
EDITOR_SOURCE = PLUGIN_ROOT / "Source" / "UnrealExtendedAtlassianEditor"

HEX_LITERAL = re.compile(r'TEXT\("#[0-9a-fA-F]{6,8}"\)')
GEOMETRY_LITERAL = re.compile(
    r"\.(?:WidthOverride|HeightOverride|MinDesiredWidth|MinDesiredHeight|"
    r"MaxDesiredWidth|MaxDesiredHeight|Padding)\s*\(\s*(?:FMargin\s*\(\s*)?"
    r"-?\d+(?:\.\d+)?f?"
)
FONT_LITERAL = re.compile(r"(?:FSlateFontInfo|Size)\s*\([^)]*,\s*\d+(?:\.\d+)?f?\s*\)")


def surface_files() -> list[Path]:
    files = set(EDITOR_SOURCE.glob("SBacklot*Surface*.cpp"))
    files.update((EDITOR_SOURCE / "Surfaces").rglob("*.cpp") if (EDITOR_SOURCE / "Surfaces").exists() else [])
    return sorted(files)


def violations(path: Path) -> list[str]:
    found: list[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if "backlot-literal-approved:" in line:
            continue
        if HEX_LITERAL.search(line):
            found.append(f"{path.relative_to(PLUGIN_ROOT)}:{line_number}: color literal")
        if GEOMETRY_LITERAL.search(line):
            found.append(f"{path.relative_to(PLUGIN_ROOT)}:{line_number}: geometry literal")
        if FONT_LITERAL.search(line):
            found.append(f"{path.relative_to(PLUGIN_ROOT)}:{line_number}: font literal")
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-surfaces",
        action="store_true",
        help="fail if no focused surface translation units are present",
    )
    args = parser.parse_args()

    files = surface_files()
    if args.require_surfaces and not files:
        raise SystemExit("no focused Backlot surface files found")
    found = [item for path in files for item in violations(path)]
    if found:
        print("Ad-hoc Backlot visual literals found. Use Backlot style/metric tokens:")
        for item in found:
            print(f"  {item}")
        return 1
    print(f"Backlot surface literal policy passed for {len(files)} files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
