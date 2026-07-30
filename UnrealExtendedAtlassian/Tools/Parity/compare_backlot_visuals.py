#!/usr/bin/env python3
"""Create deterministic Backlot candidate/diff/heatmap artifacts and metrics.

The browser baselines are immutable inputs. Native Slate captures are copied to
the output directory as candidates; this tool never updates a reference image.
"""

from __future__ import annotations

import argparse
import json
import shutil
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
from PIL import Image


PAIRS = (
    ("docs_wide_1920x1080", "reference_docs_1920x1080.png"),
    ("docs_1280x720", "reference_docs_1280x720.png"),
    ("docs_1440x900", "reference_docs_1440x900.png"),
    ("docs_2560x1440", "reference_docs_2560x1440.png"),
    ("docs_879x600", "reference_docs_879x600.png"),
    ("docs_880x600", "reference_docs_880x600.png"),
    ("docs_881x600", "reference_docs_881x600.png"),
    ("issues_wide_1920x1080", "reference_issues_wide_1920x1080.png"),
    (
        "issue_detail_wide_1920x1080",
        "reference_issue_detail_wide_1920x1080.png",
    ),
    ("board_wide_1920x1080", "reference_board_wide_1920x1080.png"),
    ("pins_wide_1920x1080", "reference_pins_wide_1920x1080.png"),
    ("inbox_wide_1920x1080", "reference_inbox_wide_1920x1080.png"),
    ("docs_compact_560x900", "reference_docs_compact_560x900.png"),
    ("issues_compact_560x900", "reference_issues_compact_560x900.png"),
    (
        "issue_detail_compact_560x900",
        "reference_issue_detail_compact_560x900.png",
    ),
    ("board_compact_560x900", "reference_board_compact_560x900.png"),
    ("pins_compact_560x900", "reference_pins_compact_560x900.png"),
    ("inbox_compact_560x900", "reference_inbox_compact_560x900.png"),
    ("capture_wide_1920x1080", "reference_capture_wide_1920x1080.png"),
    ("capture_compact_560x900", "reference_capture_compact_560x900.png"),
)


@dataclass(frozen=True)
class Comparison:
    name: str
    width: int
    height: int
    differing_pixels: int
    differing_fraction: float
    maximum_channel_error: int
    mean_absolute_error: float
    root_mean_square_error: float
    global_ssim: float
    passed: bool


def _global_ssim(reference: np.ndarray, actual: np.ndarray) -> float:
    """Return deterministic whole-image RGB SSIM in [0, 1]."""
    values: list[float] = []
    c1 = (0.01 * 255.0) ** 2
    c2 = (0.03 * 255.0) ** 2
    for channel in range(3):
        x = reference[:, :, channel].astype(np.float64)
        y = actual[:, :, channel].astype(np.float64)
        mean_x = float(x.mean())
        mean_y = float(y.mean())
        centered_x = x - mean_x
        centered_y = y - mean_y
        variance_x = float(np.mean(centered_x * centered_x))
        variance_y = float(np.mean(centered_y * centered_y))
        covariance = float(np.mean(centered_x * centered_y))
        numerator = (2.0 * mean_x * mean_y + c1) * (2.0 * covariance + c2)
        denominator = (
            (mean_x * mean_x + mean_y * mean_y + c1)
            * (variance_x + variance_y + c2)
        )
        values.append(numerator / denominator if denominator else 1.0)
    return max(0.0, min(1.0, float(np.mean(values))))


def _write_artifacts(
    output: Path,
    name: str,
    actual_path: Path,
    absolute_delta: np.ndarray,
) -> None:
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(actual_path, output / f"candidate_{name}.png")

    enhanced = np.minimum(
        absolute_delta.astype(np.uint16) * 4,
        255,
    ).astype(np.uint8)
    Image.fromarray(enhanced, "RGB").save(output / f"diff_{name}.png")

    magnitude = absolute_delta.max(axis=2).astype(np.uint16)
    heat = np.zeros((*magnitude.shape, 3), dtype=np.uint8)
    heat[:, :, 0] = np.minimum(magnitude * 6, 255).astype(np.uint8)
    heat[:, :, 1] = np.minimum(
        np.maximum(magnitude.astype(np.int32) - 24, 0) * 3,
        255,
    ).astype(np.uint8)
    heat[:, :, 2] = np.minimum(
        np.maximum(magnitude.astype(np.int32) - 96, 0) * 2,
        255,
    ).astype(np.uint8)
    Image.fromarray(heat, "RGB").save(output / f"heatmap_{name}.png")


def compare_pair(
    reference_path: Path,
    actual_path: Path,
    output: Path,
    name: str,
    maximum_differing_fraction: float,
    minimum_ssim: float,
) -> Comparison:
    reference_image = Image.open(reference_path).convert("RGB")
    actual_image = Image.open(actual_path).convert("RGB")
    if reference_image.size != actual_image.size:
        raise ValueError(
            f"{name}: dimensions differ: reference={reference_image.size}, "
            f"actual={actual_image.size}"
        )

    reference = np.asarray(reference_image, dtype=np.uint8)
    actual = np.asarray(actual_image, dtype=np.uint8)
    signed_delta = actual.astype(np.int16) - reference.astype(np.int16)
    absolute_delta = np.abs(signed_delta).astype(np.uint8)
    pixel_mask = np.any(absolute_delta != 0, axis=2)
    differing_pixels = int(pixel_mask.sum())
    pixel_count = int(pixel_mask.size)
    fraction = differing_pixels / pixel_count if pixel_count else 0.0
    ssim = _global_ssim(reference, actual)
    _write_artifacts(output, name, actual_path, absolute_delta)
    return Comparison(
        name=name,
        width=reference_image.width,
        height=reference_image.height,
        differing_pixels=differing_pixels,
        differing_fraction=fraction,
        maximum_channel_error=int(absolute_delta.max(initial=0)),
        mean_absolute_error=float(absolute_delta.mean()),
        root_mean_square_error=float(
            np.sqrt(np.mean(signed_delta.astype(np.float64) ** 2))
        ),
        global_ssim=ssim,
        passed=(
            fraction <= maximum_differing_fraction
            and ssim >= minimum_ssim
        ),
    )


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    plugin_dir = script_dir.parent.parent
    project_dir = plugin_dir.parent.parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--reference-dir",
        type=Path,
        default=plugin_dir / "Tests" / "Parity" / "ReferenceBaselines",
    )
    parser.add_argument(
        "--actual-dir",
        type=Path,
        default=(
            project_dir
            / "Saved"
            / "Automation"
            / "ExtendedAtlassian"
            / "Visual"
            / "Actual"
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=(
            project_dir
            / "Saved"
            / "Automation"
            / "ExtendedAtlassian"
            / "Visual"
            / "Comparison"
        ),
    )
    parser.add_argument("--maximum-differing-fraction", type=float, default=0.0)
    parser.add_argument("--minimum-ssim", type=float, default=1.0)
    parser.add_argument(
        "--require-pass",
        action="store_true",
        help="Return non-zero when any configured threshold fails.",
    )
    args = parser.parse_args()

    comparisons: list[Comparison] = []
    for name, reference_filename in PAIRS:
        comparisons.append(
            compare_pair(
                args.reference_dir / reference_filename,
                args.actual_dir / f"actual_{name}.png",
                args.output_dir,
                name,
                args.maximum_differing_fraction,
                args.minimum_ssim,
            )
        )

    report = {
        "schemaVersion": 1,
        "referenceDirectory": str(args.reference_dir.resolve()),
        "actualDirectory": str(args.actual_dir.resolve()),
        "thresholds": {
            "maximumDifferingFraction": args.maximum_differing_fraction,
            "minimumGlobalSsim": args.minimum_ssim,
        },
        "allPassed": all(item.passed for item in comparisons),
        "comparisons": [asdict(item) for item in comparisons],
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "report.json").write_text(
        json.dumps(report, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(report, indent=2))
    return 1 if args.require_pass and not report["allPassed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
