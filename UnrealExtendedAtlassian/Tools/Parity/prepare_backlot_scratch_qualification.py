#!/usr/bin/env python3
"""Prepare a secret-free scratch qualification descriptor.

This tool never contacts Atlassian and never stores credentials. It refuses
targets that do not carry an explicit BACKLOT SCRATCH marker.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = PLUGIN_ROOT / "Tests" / "Parity" / "BacklotLiveQualification.json"
DEFAULT_OUTPUT = (
    PLUGIN_ROOT
    / "Saved"
    / "Automation"
    / "ExtendedAtlassian"
    / "BacklotScratchQualification.json"
)
ID_PATTERN = re.compile(r"^[A-Za-z0-9._:-]+$")


def safe_id(value: str, label: str) -> str:
    value = value.strip()
    if not value or not ID_PATTERN.fullmatch(value):
        raise ValueError(f"{label} must be a non-empty stable ID, not a display label or URL")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-key", required=True)
    parser.add_argument("--project-name", required=True)
    parser.add_argument("--board-id", required=True)
    parser.add_argument("--sprint-id", required=True)
    parser.add_argument("--space-id", required=True)
    parser.add_argument("--space-key", required=True)
    parser.add_argument("--space-name", required=True)
    parser.add_argument(
        "--confirm-scratch",
        action="store_true",
        help="confirm that every supplied resource is disposable qualification data",
    )
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    if not args.confirm_scratch:
        raise ValueError("--confirm-scratch is required")
    if "BACKLOT SCRATCH" not in args.project_name.upper():
        raise ValueError("project name must contain the marker BACKLOT SCRATCH")
    if "BACKLOT SCRATCH" not in args.space_name.upper():
        raise ValueError("space name must contain the marker BACKLOT SCRATCH")

    project_key = safe_id(args.project_key, "project key").upper()
    if not project_key.startswith(("BLS", "TEST", "QA")):
        raise ValueError("project key must start with BLS, TEST, or QA")

    descriptor = json.loads(TEMPLATE.read_text(encoding="utf-8"))
    descriptor["resources"] = {
        "jiraProjectKey": project_key,
        "jiraProjectName": args.project_name.strip(),
        "boardId": safe_id(args.board_id, "board ID"),
        "sprintId": safe_id(args.sprint_id, "sprint ID"),
        "confluenceSpaceId": safe_id(args.space_id, "space ID"),
        "confluenceSpaceKey": safe_id(args.space_key, "space key"),
        "confluenceSpaceName": args.space_name.strip(),
    }
    serialized = json.dumps(descriptor, indent=2) + "\n"
    if args.write:
        output = args.output.resolve()
        if PLUGIN_ROOT not in output.parents:
            raise ValueError("output must stay inside the plugin workspace")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(serialized, encoding="utf-8")
        print(f"Wrote secret-free scratch descriptor: {output}")
    else:
        print(serialized, end="")
        print("Plan only. Pass --write to save the descriptor; no Atlassian call was made.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
