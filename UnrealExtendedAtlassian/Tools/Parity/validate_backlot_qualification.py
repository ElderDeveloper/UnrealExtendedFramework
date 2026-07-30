#!/usr/bin/env python3
"""Validate and expand the Backlot qualification contract without running tests or captures."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


PLUGIN_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MATRIX = PLUGIN_ROOT / "Tests" / "Parity" / "BacklotQualificationMatrix.json"

REQUIRED_ROUTES = {
    "docs",
    "issues",
    "issue-detail",
    "board",
    "pins",
    "inbox",
    "capture",
}
REQUIRED_SCALES = {1.0, 1.25, 1.5, 2.0}
REQUIRED_VIEWPORTS = {
    "compact",
    "threshold-minus-one",
    "threshold",
    "threshold-plus-one",
    "wide-720",
    "wide-900",
    "wide",
    "wide-hires",
}
REQUIRED_PROFILES = {
    "pages": 2000,
    "issues": 200,
    "notifications": 500,
    "pins": 200,
    "document-blocks": 5000,
}
REQUIRED_ANNOTATED_SCREENSHOT = {
    "width": 7680,
    "height": 4320,
    "annotations": 250,
}


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def unique(values: list[Any], label: str) -> None:
    normalized = [json.dumps(value, sort_keys=True) for value in values]
    if len(normalized) != len(set(normalized)):
        raise ValueError(f"{label} contains duplicate entries")


def validate(matrix: dict[str, Any]) -> None:
    if matrix.get("schemaVersion") != 1:
        raise ValueError("schemaVersion must be 1")
    if len(str(matrix.get("referenceRevision", ""))) != 64:
        raise ValueError("referenceRevision must be the frozen 64-character SHA-256")

    policy = matrix.get("executionPolicy", {})
    if policy.get("defaultMode") != "plan-only":
        raise ValueError("qualification tooling must default to plan-only")
    for safety_key in (
        "compileRequiresExplicitFlag",
        "captureRequiresExplicitFlag",
        "liveMutationRequiresExplicitFlag",
        "productionTargetsForbidden",
    ):
        if policy.get(safety_key) is not True:
            raise ValueError(f"executionPolicy.{safety_key} must be true")

    route_scenarios = matrix.get("routeScenarios", {})
    missing_routes = REQUIRED_ROUTES.difference(route_scenarios)
    if missing_routes:
        raise ValueError(f"missing route scenario groups: {sorted(missing_routes)}")
    for route, scenarios in route_scenarios.items():
        if not scenarios:
            raise ValueError(f"routeScenarios.{route} must not be empty")
        unique(scenarios, f"routeScenarios.{route}")
    shared_scenarios = matrix.get("sharedScenarios", [])
    if not shared_scenarios:
        raise ValueError("sharedScenarios must not be empty")
    unique(shared_scenarios, "sharedScenarios")

    scales = matrix.get("applicationScales", [])
    if set(scales) != REQUIRED_SCALES:
        raise ValueError(f"applicationScales must be {sorted(REQUIRED_SCALES)}")
    unique(scales, "applicationScales")

    viewports = matrix.get("viewports", [])
    unique(viewports, "viewports")
    viewport_id_list = [item.get("id") for item in viewports]
    unique(viewport_id_list, "viewport ids")
    viewport_ids = set(viewport_id_list)
    if viewport_ids != REQUIRED_VIEWPORTS:
        raise ValueError(f"viewports must be {sorted(REQUIRED_VIEWPORTS)}")
    for viewport in viewports:
        if int(viewport.get("width", 0)) <= 0 or int(viewport.get("height", 0)) <= 0:
            raise ValueError(f"invalid viewport: {viewport}")

    compact_routes = set(matrix.get("compactRoutes", []))
    if compact_routes != REQUIRED_ROUTES.difference({"capture"}):
        raise ValueError("compactRoutes must cover every shippable route")

    profiles = matrix.get("performanceProfiles", [])
    profile_map = {
        profile.get("id"): int(profile.get("count", 0))
        for profile in profiles
        if "count" in profile
    }
    for profile_id, minimum in REQUIRED_PROFILES.items():
        if profile_map.get(profile_id, 0) < minimum:
            raise ValueError(
                f"performance profile {profile_id!r} must cover at least {minimum:,} items"
            )
    screenshot_profile = next(
        (
            profile
            for profile in profiles
            if profile.get("id") == "annotated-screenshot"
        ),
        None,
    )
    if screenshot_profile is None:
        raise ValueError("performance profile 'annotated-screenshot' is required")
    for field, minimum in REQUIRED_ANNOTATED_SCREENSHOT.items():
        if int(screenshot_profile.get(field, 0)) < minimum:
            raise ValueError(
                "performance profile 'annotated-screenshot' must cover at least "
                f"{minimum:,} {field}"
            )

    if len(matrix.get("interactionSuites", [])) < 18:
        raise ValueError("interactionSuites does not cover the full input contract")
    if len(matrix.get("liveQualification", [])) < 11:
        raise ValueError("liveQualification does not cover the destructive/recovery contract")

    results = matrix.get("qualificationResults", {})
    allowed_results = {"pending", "passed", "failed", "approved-exception"}
    invalid_results = {
        key: value for key, value in results.items() if value not in allowed_results
    }
    if invalid_results:
        raise ValueError(f"invalid qualification result values: {invalid_results}")


def expand_jobs(matrix: dict[str, Any]) -> list[dict[str, Any]]:
    jobs_by_id: dict[str, dict[str, Any]] = {}
    wide = next(item for item in matrix["viewports"] if item["id"] == "wide")
    compact = next(item for item in matrix["viewports"] if item["id"] == "compact")

    for route, scenarios in matrix["routeScenarios"].items():
        for scenario in scenarios:
            for scale in matrix["applicationScales"]:
                job_id = f"{route}.{scenario}.wide.scale-{scale:g}"
                jobs_by_id[job_id] = {
                    "kind": "visual",
                    "id": job_id,
                    "route": route,
                    "scenario": scenario,
                    "viewport": wide,
                    "scale": scale,
                }
    for scenario in matrix["sharedScenarios"]:
        for scale in matrix["applicationScales"]:
            job_id = f"shared.{scenario}.wide.scale-{scale:g}"
            jobs_by_id[job_id] = {
                "kind": "visual",
                "id": job_id,
                "route": "docs",
                "scenario": scenario,
                "viewport": wide,
                "scale": scale,
            }
    for route in matrix["compactRoutes"]:
        for scale in matrix["applicationScales"]:
            job_id = f"{route}.default.compact.scale-{scale:g}"
            jobs_by_id[job_id] = {
                "kind": "visual",
                "id": job_id,
                "route": route,
                "scenario": "default",
                "viewport": compact,
                "scale": scale,
            }
    for viewport in matrix["viewports"]:
        for scale in matrix["applicationScales"]:
            job_id = f"docs.read.{viewport['id']}.scale-{scale:g}"
            jobs_by_id[job_id] = {
                "kind": "layout-scale",
                "id": job_id,
                "route": "docs",
                "scenario": "read",
                "viewport": viewport,
                "scale": scale,
            }
    return list(jobs_by_id.values())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument(
        "--write-jobs",
        type=Path,
        help="write the deterministic expanded job list; no capture is performed",
    )
    parser.add_argument(
        "--require-passed",
        action="store_true",
        help="release-only gate: reject any non-passed qualification result",
    )
    args = parser.parse_args()

    matrix = load_json(args.matrix.resolve())
    validate(matrix)
    jobs = expand_jobs(matrix)
    if args.require_passed:
        pending = {
            key: value
            for key, value in matrix["qualificationResults"].items()
            if value not in {"passed", "approved-exception"}
        }
        if pending:
            raise ValueError(f"qualification is not complete: {pending}")
    if args.write_jobs:
        output = args.write_jobs.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            json.dumps({"schemaVersion": 1, "jobs": jobs}, indent=2) + "\n",
            encoding="utf-8",
        )
    print(
        f"Backlot qualification contract is valid: {len(jobs)} visual/layout jobs, "
        f"{len(matrix['interactionSuites'])} interaction suites, "
        f"{len(matrix['liveQualification'])} live scenarios."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
