#!/usr/bin/env python3
"""PerfSentinel offline trace analyzer.

This script is intentionally project-agnostic. It consumes a .utrace, metadata
sidecar, optional spike/fallback NDJSON files, and optional UnrealInsights.exe,
then writes LLM-readable JSON and Markdown findings.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sqlite3
import statistics
import subprocess
import sys
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ANALYZER_NAME = "perf_sentinel_analyze.py"
SCHEMA_VERSION = 2

OBJECT_TIMER_PATTERNS = [
    re.compile(r"\b(?P<target>[A-Za-z][A-Za-z0-9_]*_C_UAID_[A-Fa-f0-9]+)\b"),
    re.compile(r"\bExecuteUbergraph_(?P<target>[A-Za-z][A-Za-z0-9_]*)\b"),
    re.compile(r"\b(?P<target>(?:BP|WBP|UI|Widget)_[A-Za-z][A-Za-z0-9_]*(?:_C)?)\b"),
]


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def ue_cli_path(path: Path) -> str:
    return str(path.resolve()).replace("\\", "/")


class AnalyzerLog:
    def __init__(self, path: Path | None = None) -> None:
        self.path = path

    def write(self, message: str) -> None:
        line = f"[{utc_now_iso()}] {message}"
        if self.path:
            try:
                with self.path.open("a", encoding="utf-8") as stream:
                    stream.write(line + "\n")
            except OSError:
                pass
        try:
            print(line)
        except UnicodeEncodeError:
            encoding = sys.stdout.encoding or "utf-8"
            print(line.encode(encoding, errors="replace").decode(encoding, errors="replace"))
        except OSError:
            pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze a PerfSentinel Unreal trace capture.")
    parser.add_argument("--trace", required=True, type=Path)
    parser.add_argument("--metadata", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--frame-budget-ms", required=True, type=float)
    parser.add_argument("--hitch-threshold-ms", required=True, type=float)
    parser.add_argument("--fallback-stats", type=Path)
    parser.add_argument("--spikes", type=Path)
    parser.add_argument("--runtime-counters", type=Path)
    parser.add_argument("--insights-exe", type=Path)
    parser.add_argument("--native-evidence", type=Path)
    parser.add_argument("--baseline-dir", type=Path)
    parser.add_argument("--baseline-key", default="default")
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--ci", action="store_true")
    parser.add_argument("--max-p99-regression-percent", type=float, default=15.0)
    parser.add_argument("--max-hitches-per-minute", type=float, default=3.0)
    parser.add_argument("--max-memory-growth-mb", type=float, default=200.0)
    parser.add_argument(
        "--trace-time-offset",
        type=float,
        default=0.0,
        help="Seconds added to wall-clock spike offsets to map them into Unreal Insights trace time.",
    )
    return parser.parse_args()


def load_json_file(path: Path, log: AnalyzerLog) -> dict[str, Any]:
    if not path.exists():
        log.write(f"metadata file missing: {path}")
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
        return value if isinstance(value, dict) else {}
    except Exception as exc:
        log.write(f"failed to parse metadata file {path}: {exc}")
        return {}


def percentile(values: list[float], percentile_value: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * max(0.0, min(100.0, percentile_value)) / 100.0
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def safe_baseline_key(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_.-]+", "_", value.strip())
    return sanitized.strip("._") or "default"


def load_ndjson(path: Path | None, log: AnalyzerLog) -> list[dict[str, Any]]:
    if not path:
        return []
    if not path.exists():
        log.write(f"ndjson file missing: {path}")
        return []

    rows: list[dict[str, Any]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        stripped = line.strip()
        if not stripped:
            continue
        try:
            value = json.loads(stripped)
        except Exception as exc:
            log.write(f"skipping invalid ndjson line {path}:{line_number}: {exc}")
            continue
        if isinstance(value, dict):
            rows.append(value)
    return rows


def parse_iso_datetime(value: Any) -> datetime | None:
    text = str(value or "").strip()
    if not text:
        return None
    try:
        return datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return None


def find_spike_output_dir(report_dir: Path, spike: dict[str, Any]) -> Path:
    frame_number = str(spike.get("frame_number") or "").strip()
    if frame_number:
        matches = sorted((report_dir / "spikes").glob(f"*frame_{frame_number}"))
        if matches:
            return matches[0]
        return report_dir / "spikes" / f"spike_frame_{frame_number}"
    return report_dir / "spikes" / "spike_unknown"


def spike_offset_seconds(spike: dict[str, Any], metadata: dict[str, Any], started_at: datetime | None) -> float | None:
    """Seconds from capture start to this spike, preferring monotonic platform time over wall clock."""
    capture_start = float_or_none(metadata.get("capture_start_platform_seconds"))
    spike_platform = float_or_none(spike.get("platform_seconds"))
    if capture_start is not None and spike_platform is not None:
        return max(0.0, spike_platform - capture_start)

    if started_at is not None:
        spike_at = parse_iso_datetime(spike.get("timestamp"))
        if spike_at is not None:
            return max(0.0, (spike_at - started_at).total_seconds())

    return None


def build_spike_timing_event_commands(
    report_dir: Path,
    metadata: dict[str, Any],
    spikes: list[dict[str, Any]],
    trace_time_offset: float = 0.0,
) -> list[str]:
    started_at = parse_iso_datetime(metadata.get("started_at"))
    if not spikes:
        return []

    commands: list[str] = []
    for index, spike in enumerate(spikes, start=1):
        spike_seconds = spike_offset_seconds(spike, metadata, started_at)
        if spike_seconds is None:
            continue
        spike_seconds = max(0.0, spike_seconds + trace_time_offset)
        frame_ms = float_or_none(spike.get("frame_time_ms")) or 0.0
        pre_seconds = max(0.5, (frame_ms / 1000.0) + 0.25)
        post_seconds = 0.25
        start_seconds = max(0.0, spike_seconds - pre_seconds)
        end_seconds = spike_seconds + post_seconds

        spike_dir = find_spike_output_dir(report_dir, spike)
        spike_dir.mkdir(parents=True, exist_ok=True)
        output_csv = spike_dir / "timing_events.csv"
        commands.append(
            f'TimingInsights.ExportTimingEvents "{ue_cli_path(output_csv)}" '
            '-columns=ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth '
            '-threads="GameThread,RenderThread,RHIThread,*GPU*,ActualRenderingThread" '
            f"-startTime={start_seconds:.6f} -endTime={end_seconds:.6f}"
        )

    return commands


def classify_target(target: str) -> str:
    if target.startswith("WBP_") or target.startswith("UI_") or "Widget" in target:
        return "widget"
    if "_C_UAID_" in target or target.startswith("BP_"):
        return "actor_or_blueprint"
    return "object"


def normalize_target_name(target: str) -> str:
    return target.strip().strip("\"'")


def extract_object_targets(timer_name: str) -> list[str]:
    targets: list[str] = []
    for pattern in OBJECT_TIMER_PATTERNS:
        for match in pattern.finditer(timer_name):
            target = normalize_target_name(match.group("target"))
            if target and target not in targets:
                targets.append(target)
    return targets


def timing_event_duration_ms(row: dict[str, Any]) -> float:
    duration = float_or_none(row.get("Duration"))
    if duration is not None:
        return duration * 1000.0

    start = float_or_none(row.get("StartTime"))
    end = float_or_none(row.get("EndTime"))
    if start is not None and end is not None and end >= start:
        return (end - start) * 1000.0

    return 0.0


def add_target_evidence(target: dict[str, Any], spike: dict[str, Any], timing_csv: Path, row: dict[str, Any], duration_ms: float) -> None:
    target["total_time_ms"] += duration_ms
    target["max_event_time_ms"] = max(target["max_event_time_ms"], duration_ms)
    target["event_count"] += 1

    frame_number = spike.get("frame_number")
    spike_key = str(frame_number or "unknown")
    per_spike = target.setdefault("_per_spike", {})
    spike_entry = per_spike.setdefault(
        spike_key,
        {
            "frame_number": frame_number,
            "timestamp": spike.get("timestamp", ""),
            "frame_time_ms": spike.get("frame_time_ms", 0),
            "timing_events_csv": str(timing_csv),
            "total_time_ms": 0.0,
            "max_event_time_ms": 0.0,
            "event_count": 0,
            "sample_events": [],
        },
    )
    spike_entry["total_time_ms"] += duration_ms
    spike_entry["max_event_time_ms"] = max(spike_entry["max_event_time_ms"], duration_ms)
    spike_entry["event_count"] += 1

    sample_events = spike_entry["sample_events"]
    if len(sample_events) < 8:
        sample_events.append(
            {
                "timer_name": row.get("TimerName") or row.get("Name") or "",
                "thread": row.get("ThreadName") or "",
                "duration_ms": round(duration_ms, 6),
                "start_time": row.get("StartTime", ""),
                "end_time": row.get("EndTime", ""),
                "depth": row.get("Depth", ""),
            }
        )


def build_game_stats(report_dir: Path, metadata: dict[str, Any], spikes: list[dict[str, Any]], log: AnalyzerLog) -> Path:
    targets: dict[str, dict[str, Any]] = {}

    for spike in spikes:
        spike_dir = find_spike_output_dir(report_dir, spike)
        timing_csv = spike_dir / "timing_events.csv"
        if not timing_csv.exists():
            continue

        try:
            with timing_csv.open("r", encoding="utf-8-sig", newline="") as handle:
                reader = csv.DictReader(handle)
                for row in reader:
                    timer_name = str(row.get("TimerName") or row.get("Name") or "").strip()
                    if not timer_name:
                        continue

                    duration_ms = timing_event_duration_ms(row)
                    if duration_ms <= 0.0:
                        continue

                    for target_name in extract_object_targets(timer_name):
                        target = targets.setdefault(
                            target_name,
                            {
                                "target": target_name,
                                "kind": classify_target(target_name),
                                "cost_status": "measured_from_timing_events",
                                "total_time_ms": 0.0,
                                "max_event_time_ms": 0.0,
                                "event_count": 0,
                                "spike_count": 0,
                                "suspicion": False,
                                "suspicion_evidence": [],
                                "spikes": [],
                            },
                        )
                        add_target_evidence(target, spike, timing_csv, row, duration_ms)
        except Exception as exc:
            log.write(f"failed to parse spike timing events {timing_csv}: {exc}")

    target_list: list[dict[str, Any]] = []
    for target in targets.values():
        per_spike = target.pop("_per_spike", {})
        spike_entries = list(per_spike.values())
        spike_entries.sort(key=lambda item: (float(item["max_event_time_ms"]), float(item["total_time_ms"])), reverse=True)

        target["spike_count"] = len(spike_entries)
        target["spikes"] = [
            {
                **entry,
                "total_time_ms": round(float(entry["total_time_ms"]), 6),
                "max_event_time_ms": round(float(entry["max_event_time_ms"]), 6),
            }
            for entry in spike_entries
        ]
        target["total_time_ms"] = round(float(target["total_time_ms"]), 6)
        target["max_event_time_ms"] = round(float(target["max_event_time_ms"]), 6)

        suspicion = target["max_event_time_ms"] >= 1.0 or target["total_time_ms"] >= 5.0 or target["spike_count"] >= 3
        target["suspicion"] = suspicion
        if suspicion:
            evidence: list[str] = []
            if target["max_event_time_ms"] >= 1.0:
                evidence.append(f"max_event_time_ms={target['max_event_time_ms']}")
            if target["total_time_ms"] >= 5.0:
                evidence.append(f"total_time_ms={target['total_time_ms']}")
            if target["spike_count"] >= 3:
                evidence.append(f"spike_count={target['spike_count']}")
            target["suspicion_evidence"] = evidence

        target_list.append(target)

    target_list.sort(key=lambda item: (bool(item["suspicion"]), float(item["max_event_time_ms"]), float(item["total_time_ms"])), reverse=True)

    offline_status = "measured_from_timing_events" if target_list else "no_events_in_window_check_timebase"

    offline_block = {
        "schema_version": 1,
        "generated_at": utc_now_iso(),
        "source": "per_spike_timing_events",
        "status": offline_status,
        "trace": str(metadata.get("trace_file") or ""),
        "scenario": metadata.get("scenario", ""),
        "target_count": len(target_list),
        "targets": target_list,
    }

    output_path = report_dir / "GameStats.json"

    # Merge into the C++-written inventory (actors/widgets/runtime_stats) instead of overwriting it.
    merged: dict[str, Any] = {}
    if output_path.exists():
        try:
            existing = json.loads(output_path.read_text(encoding="utf-8-sig"))
            if isinstance(existing, dict):
                merged = existing
        except Exception as exc:
            log.write(f"could not read existing GameStats.json, writing offline-only: {exc}")

    merged["offline_cost"] = offline_block
    merged.setdefault("generated_at", utc_now_iso())
    merged.setdefault("scenario", metadata.get("scenario", ""))
    if "cost_source" not in merged:
        # No C++ inventory present; the offline export is the only cost source in this file.
        merged["cost_source"] = "offline_timing_events" if target_list else "unavailable"

    output_path.write_text(json.dumps(merged, indent=2), encoding="utf-8")
    if not target_list:
        log.write(
            "offline per-spike timing export returned no events for any spike window; "
            "runtime stats (cost_source in GameStats.json) are the reliable cost source. "
            "See trace_time_offset if offline per-instance cost is required."
        )
    log.write(f"wrote {output_path}")
    return output_path


def write_export_response_file(
    rsp_path: Path,
    timer_csv: Path,
    threads_csv: Path,
    timers_csv: Path,
    counters_csv: Path,
    spike_timing_event_commands: list[str],
) -> None:
    rsp_path.write_text(
        "\n".join(
            [
                f'TimingInsights.ExportTimerStatistics "{ue_cli_path(timer_csv)}"',
                f'TimingInsights.ExportThreads "{ue_cli_path(threads_csv)}"',
                f'TimingInsights.ExportTimers "{ue_cli_path(timers_csv)}"',
                f'TimingInsights.ExportCounters "{ue_cli_path(counters_csv)}"',
                *spike_timing_event_commands,
                "",
            ]
        ),
        encoding="utf-8",
    )


def ensure_csv_placeholders(timer_csv: Path, threads_csv: Path) -> None:
    if not timer_csv.exists():
        timer_csv.write_text("Name,Count,Incl,I.Min,I.Max,I.Avg,I.Med,Excl,E.Min,E.Max,E.Avg,E.Med\n", encoding="utf-8")
    if not threads_csv.exists():
        threads_csv.write_text("Id,Name,Group\n", encoding="utf-8")


def export_unreal_insights(
    insights_exe: Path | None,
    trace_path: Path,
    metadata: dict[str, Any],
    spikes: list[dict[str, Any]],
    report_dir: Path,
    csv_dir: Path,
    log: AnalyzerLog,
    trace_time_offset: float = 0.0,
) -> tuple[Path, Path, Path]:
    csv_dir.mkdir(parents=True, exist_ok=True)
    insights_log = csv_dir / "UnrealInsightsExport.log"
    rsp_path = csv_dir / "export_commands.rsp"
    timer_csv = csv_dir / "TimerStatistics.csv"
    threads_csv = csv_dir / "Threads.csv"
    timers_csv = csv_dir / "Timers.csv"
    counters_csv = csv_dir / "Counters.csv"

    if not insights_exe:
        insights_log.write_text("UnrealInsights executable was not provided.\n", encoding="utf-8")
        ensure_csv_placeholders(timer_csv, threads_csv)
        log.write("UnrealInsights executable was not provided; trace data export is not_available.")
        return timer_csv, threads_csv, insights_log

    insights_exe = insights_exe.resolve()
    if not insights_exe.exists():
        insights_log.write_text(f"UnrealInsights executable was not found: {insights_exe}\n", encoding="utf-8")
        ensure_csv_placeholders(timer_csv, threads_csv)
        log.write(f"UnrealInsights executable was not found: {insights_exe}")
        return timer_csv, threads_csv, insights_log

    spike_timing_event_commands = build_spike_timing_event_commands(report_dir, metadata, spikes, trace_time_offset)
    write_export_response_file(rsp_path, timer_csv, threads_csv, timers_csv, counters_csv, spike_timing_event_commands)
    cmd = [
        str(insights_exe),
        f"-OpenTraceFile={ue_cli_path(trace_path)}",
        "-NoUI",
        "-AutoQuit",
        f"-ABSLOG={ue_cli_path(insights_log)}",
        "-log",
        f"-ExecOnAnalysisCompleteCmd=@={ue_cli_path(rsp_path)}",
    ]
    log.write("running UnrealInsights export:")
    log.write(" ".join(cmd))

    try:
        completed = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdin=subprocess.DEVNULL,
            timeout=1800,
        )
    except subprocess.TimeoutExpired as exc:
        log.write(f"UnrealInsights export timed out: {exc}")
        with insights_log.open("a", encoding="utf-8") as handle:
            handle.write("\nPerfSentinel wrapper timeout.\n")
        ensure_csv_placeholders(timer_csv, threads_csv)
        return timer_csv, threads_csv, insights_log
    except Exception as exc:
        log.write(f"UnrealInsights export failed to launch: {exc}")
        with insights_log.open("a", encoding="utf-8") as handle:
            handle.write(f"\nPerfSentinel wrapper launch failure: {exc}\n")
        ensure_csv_placeholders(timer_csv, threads_csv)
        return timer_csv, threads_csv, insights_log
    finally:
        try:
            rsp_path.unlink(missing_ok=True)
        except OSError as exc:
            log.write(f"could not remove temporary UnrealInsights response file {rsp_path}: {exc}")

    log.write(f"UnrealInsights exit code: {completed.returncode}")
    if completed.stdout:
        log.write("UnrealInsights stdout:")
        log.write(completed.stdout.rstrip())
    if completed.stderr:
        log.write("UnrealInsights stderr:")
        log.write(completed.stderr.rstrip())

    if not insights_log.exists():
        insights_log.write_text(
            "UnrealInsights did not create an ABSLOG file. See the Unreal editor log for captured analyzer output.\n",
            encoding="utf-8",
        )

    ensure_csv_placeholders(timer_csv, threads_csv)
    return timer_csv, threads_csv, insights_log


def float_or_none(value: Any) -> float | None:
    if value is None:
        return None
    text = str(value).strip()
    if not text or text == "-":
        return None
    try:
        return float(text.replace(",", ""))
    except ValueError:
        return None


def int_or_zero(value: Any) -> int:
    numeric = float_or_none(value)
    return int(numeric) if numeric is not None else 0


def metric(row: dict[str, Any], specs: list[tuple[str, float]]) -> float:
    for column_name, multiplier in specs:
        if column_name in row:
            value = float_or_none(row.get(column_name))
            if value is not None:
                return value * multiplier
    return 0.0


def parse_timer_statistics(path: Path, log: AnalyzerLog) -> list[dict[str, Any]]:
    if not path.exists():
        log.write(f"TimerStatistics.csv missing: {path}")
        return []

    timers: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            for row in reader:
                name = (row.get("Name") or "").strip()
                if not name:
                    continue
                timers.append(
                    {
                        "name": name,
                        "count": int_or_zero(row.get("Count")),
                        "total_time_ms": metric(row, [("Incl", 1000.0), ("TotalMs", 1.0), ("TotalTime", 1.0), ("Total", 1.0)]),
                        "min_time_ms": metric(row, [("I.Min", 1000.0), ("MinMs", 1.0), ("MinTime", 1.0), ("Min", 1.0)]),
                        "max_time_ms": metric(row, [("I.Max", 1000.0), ("MaxMs", 1.0), ("MaxTime", 1.0), ("Max", 1.0)]),
                        "avg_time_ms": metric(row, [("I.Avg", 1000.0), ("AvgMs", 1.0), ("AvgTime", 1.0), ("Avg", 1.0)]),
                        "exclusive_total_time_ms": metric(row, [("Excl", 1000.0), ("ExclusiveTotalMs", 1.0), ("ExclusiveTotal", 1.0)]),
                    }
                )
    except Exception as exc:
        log.write(f"failed to parse TimerStatistics.csv: {exc}")
        return []

    timers.sort(key=lambda item: (item["max_time_ms"], item["total_time_ms"]), reverse=True)
    log.write(f"parsed {len(timers)} timer statistic row(s)")
    return timers


def parse_threads(path: Path, log: AnalyzerLog) -> list[dict[str, Any]]:
    if not path.exists():
        log.write(f"Threads.csv missing: {path}")
        return []

    threads: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            for row in reader:
                thread_id = (row.get("Id") or "").strip()
                name = (row.get("Name") or "").strip()
                group = (row.get("Group") or "").strip()
                if thread_id or name or group:
                    threads.append({"id": thread_id, "name": name, "group": group})
    except Exception as exc:
        log.write(f"failed to parse Threads.csv: {exc}")
        return []

    log.write(f"parsed {len(threads)} thread row(s)")
    return threads


def get_number(row: dict[str, Any], *keys: str) -> float | None:
    for key in keys:
        if key in row:
            value = float_or_none(row.get(key))
            if value is not None:
                return value
    return None


def normalize_perf_rows(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    for row in rows:
        normalized_row = dict(row)
        aliases = {
            "frame_time_ms": ("frame_time_ms", "frame_ms", "FrameTimeMs"),
            "game_thread_ms": ("game_thread_ms", "game_thread_time_ms", "GameThreadMs"),
            "render_thread_ms": ("render_thread_ms", "render_thread_time_ms", "RenderThreadMs"),
            "rhi_thread_ms": ("rhi_thread_ms", "rhi_thread_time_ms", "RHIThreadMs"),
            "gpu_ms": ("gpu_ms", "gpu_time_ms", "GpuMs"),
        }
        for canonical, possible_keys in aliases.items():
            value = get_number(row, *possible_keys)
            if value is not None:
                normalized_row[canonical] = value
        normalized.append(normalized_row)
    return normalized


def screenshot_list(spikes: list[dict[str, Any]]) -> list[str]:
    screenshots: list[str] = []
    for row in spikes:
        screenshot = str(row.get("screenshot") or row.get("screenshot_path") or "").strip()
        if screenshot and screenshot not in screenshots:
            screenshots.append(screenshot)
    return screenshots


def numeric_series(counters: list[dict[str, Any]], key: str) -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    for row in counters:
        timestamp = float_or_none(row.get("platform_seconds"))
        value = float_or_none(row.get(key))
        if timestamp is not None and value is not None:
            points.append((timestamp, value))
    points.sort(key=lambda item: item[0])
    return points


def linear_slope(points: list[tuple[float, float]]) -> float:
    """Least-squares slope (value per second). 0 when undefined."""
    n = len(points)
    if n < 2:
        return 0.0
    t0 = points[0][0]
    xs = [p[0] - t0 for p in points]
    ys = [p[1] for p in points]
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    denom = sum((x - mean_x) ** 2 for x in xs)
    if denom <= 0.0:
        return 0.0
    num = sum((xs[i] - mean_x) * (ys[i] - mean_y) for i in range(n))
    return num / denom


def series_stats(points: list[tuple[float, float]]) -> dict[str, Any] | None:
    if not points:
        return None
    values = [p[1] for p in points]
    duration = max(1e-6, points[-1][0] - points[0][0])
    return {
        "start": values[0],
        "end": values[-1],
        "min": min(values),
        "max": max(values),
        "delta": values[-1] - values[0],
        "slope_per_second": linear_slope(points),
        "samples": len(points),
        "duration_seconds": round(duration, 3),
    }


def class_breakdown_delta(counters: list[dict[str, Any]], field: str, top: int = 15) -> list[dict[str, Any]]:
    heavy = [row for row in counters if isinstance(row.get(field), list) and row.get(field)]
    if len(heavy) < 1:
        return []
    first_row, last_row = heavy[0], heavy[-1]

    def to_map(row: dict[str, Any]) -> dict[str, int]:
        result: dict[str, int] = {}
        for entry in row.get(field, []):
            class_name = str(entry.get("class") or "").strip()
            if class_name:
                result[class_name] = int_or_zero(entry.get("count"))
        return result

    start_map = to_map(first_row)
    end_map = to_map(last_row)
    deltas = []
    for class_name in set(start_map) | set(end_map):
        start_count = start_map.get(class_name, 0)
        end_count = end_map.get(class_name, 0)
        deltas.append({"class": class_name, "start": start_count, "end": end_count, "delta": end_count - start_count})
    deltas.sort(key=lambda item: item["delta"], reverse=True)
    return deltas[:top]


def summarize_counters(counters: list[dict[str, Any]]) -> dict[str, Any]:
    if not counters:
        return {}

    summary: dict[str, Any] = {"sample_count": len(counters)}
    for key in ("actor_count", "component_count", "uobject_count", "memory_used_physical_bytes"):
        stats = series_stats(numeric_series(counters, key))
        if stats is not None:
            summary[key] = stats

    gc_points = numeric_series(counters, "gc_count")
    if gc_points:
        summary["gc_count_total"] = int(gc_points[-1][1])

    actor_growth = class_breakdown_delta(counters, "actor_classes")
    if actor_growth:
        summary["actor_class_growth"] = actor_growth
    object_growth = class_breakdown_delta(counters, "uobject_classes")
    if object_growth:
        summary["uobject_class_growth"] = object_growth
    return summary


def generate_counter_findings(builder: "FindingBuilder", counters: list[dict[str, Any]]) -> None:
    if not counters:
        return

    rows = [row for row in counters if float_or_none(row.get("platform_seconds")) is not None]
    rows.sort(key=lambda row: float_or_none(row.get("platform_seconds")) or 0.0)
    if len(rows) < 2:
        return

    mb = 1024.0 * 1024.0

    # --- Memory growth / leak heuristic ---
    mem_points = numeric_series(rows, "memory_used_physical_bytes")
    mem_stats = series_stats(mem_points)
    if mem_stats is not None and mem_stats["start"] > 0:
        growth_mb = mem_stats["delta"] / mb
        growth_pct = mem_stats["delta"] / mem_stats["start"]
        not_reclaimed = mem_stats["end"] >= mem_stats["max"] * 0.95
        gc_total = int(numeric_series(rows, "gc_count")[-1][1]) if numeric_series(rows, "gc_count") else 0
        if growth_mb > 100.0 and growth_pct > 0.15 and not_reclaimed:
            severity = "error" if growth_mb > 500.0 else "warning"
            builder.add(
                severity=severity,
                category="memory_leak_suspected",
                title="Physical memory grew steadily and was not reclaimed",
                evidence=[
                    f"Used physical memory rose {growth_mb:.0f} MB ({growth_pct * 100:.0f}%) over {mem_stats['duration_seconds']:.0f}s.",
                    f"Growth rate ~{mem_stats['slope_per_second'] / mb:.2f} MB/s; ended at {mem_stats['end'] / mb:.0f} MB (peak {mem_stats['max'] / mb:.0f} MB).",
                    f"{gc_total} garbage collection(s) occurred during capture but memory did not return to baseline.",
                ],
                suggested_next_step="Capture with the 'memory' insights channel or use 'obj list'/'memreport -full' around the growth window to identify the retaining class.",
            )

    # --- UObject count growth ---
    obj_points = numeric_series(rows, "uobject_count")
    obj_stats = series_stats(obj_points)
    if obj_stats is not None and obj_stats["start"] > 0:
        obj_delta = obj_stats["delta"]
        if obj_delta > 2000 and (obj_delta / obj_stats["start"]) > 0.05:
            growth = class_breakdown_delta(rows, "uobject_classes", top=8)
            top_text = "; ".join(f"{g['class']} +{g['delta']}" for g in growth if g["delta"] > 0) or "per-class breakdown unavailable"
            builder.add(
                severity="warning",
                category="object_growth",
                title="UObject count grew significantly during capture",
                evidence=[
                    f"UObject count rose by {int(obj_delta)} ({obj_delta / obj_stats['start'] * 100:.0f}%), from {int(obj_stats['start'])} to {int(obj_stats['end'])}.",
                    f"Top growing classes: {top_text}.",
                ],
                suggested_next_step="Check whether the top growing classes are expected to persist; if not, they may be leaking or not being garbage collected.",
            )

    # --- Actor churn / streaming unload not reclaimed ---
    actor_points = numeric_series(rows, "actor_count")
    if len(actor_points) >= 2:
        peak_value = max(p[1] for p in actor_points)
        peak_index = next(i for i, p in enumerate(actor_points) if p[1] == peak_value)
        final_value = actor_points[-1][1]
        drop = peak_value - final_value
        if drop >= max(20.0, 0.3 * peak_value) and peak_index < len(actor_points) - 1:
            # A meaningful actor unload happened after the peak. Was memory reclaimed?
            mem_at_peak = None
            mem_at_end = None
            if mem_points:
                peak_time = actor_points[peak_index][0]
                mem_at_peak = min(mem_points, key=lambda p: abs(p[0] - peak_time))[1]
                mem_at_end = mem_points[-1][1]

            if mem_at_peak is not None and mem_at_end is not None and mem_at_peak > 0 and mem_at_end >= mem_at_peak * 0.95:
                builder.add(
                    severity="warning",
                    category="streaming_unload_not_reclaimed",
                    title="Actors were unloaded but memory was not reclaimed",
                    evidence=[
                        f"Actor count dropped {int(drop)} (from peak {int(peak_value)} to {int(final_value)}).",
                        f"Used physical memory stayed at {mem_at_end / mb:.0f} MB (was {mem_at_peak / mb:.0f} MB at the actor peak).",
                    ],
                    suggested_next_step="Inspect references kept alive after the unload (delegates, timers, soft refs, editor-only references) preventing GC of the unloaded actors.",
                )
            else:
                builder.add(
                    severity="info",
                    category="actor_churn",
                    title="Significant actor load/unload churn observed",
                    evidence=[
                        f"Actor count dropped {int(drop)} from a peak of {int(peak_value)} to {int(final_value)} during capture.",
                    ],
                    suggested_next_step="Confirm the churn aligns with an intended level streaming or spawn/despawn event.",
                )


def compute_event_self_times(native: dict[str, Any]) -> list[dict[str, Any]]:
    """Compute best-effort self time from nested events in each frame/thread hierarchy."""
    events = [dict(row) for row in native.get("timing_events", []) if isinstance(row, dict)]
    groups: dict[tuple[int, str], list[dict[str, Any]]] = {}
    for event in events:
        key = (int_or_zero(event.get("frame_index")), str(event.get("thread") or ""))
        groups.setdefault(key, []).append(event)

    for group in groups.values():
        group.sort(key=lambda row: (float_or_none(row.get("start_seconds")) or 0.0, int_or_zero(row.get("depth"))))
        for index, event in enumerate(group):
            start = float_or_none(event.get("start_seconds")) or 0.0
            end = float_or_none(event.get("end_seconds")) or start
            depth = int_or_zero(event.get("depth"))
            child_intervals: list[tuple[float, float]] = []
            for child in group[index + 1 :]:
                child_start = float_or_none(child.get("start_seconds")) or 0.0
                if child_start >= end:
                    break
                child_depth = int_or_zero(child.get("depth"))
                if child_depth == depth + 1:
                    child_end = min(end, float_or_none(child.get("end_seconds")) or child_start)
                    if child_end > child_start:
                        child_intervals.append((max(start, child_start), child_end))
            merged = 0.0
            cursor_start: float | None = None
            cursor_end = 0.0
            for child_start, child_end in sorted(child_intervals):
                if cursor_start is None:
                    cursor_start, cursor_end = child_start, child_end
                elif child_start <= cursor_end:
                    cursor_end = max(cursor_end, child_end)
                else:
                    merged += cursor_end - cursor_start
                    cursor_start, cursor_end = child_start, child_end
            if cursor_start is not None:
                merged += cursor_end - cursor_start
            duration_ms = max(0.0, (end - start) * 1000.0)
            event["self_time_ms"] = round(max(0.0, duration_ms - merged * 1000.0), 6)
    return events


def build_frame_attribution(native: dict[str, Any], hitch_threshold_ms: float) -> list[dict[str, Any]]:
    events = compute_event_self_times(native)
    by_frame: dict[int, list[dict[str, Any]]] = {}
    for event in events:
        by_frame.setdefault(int_or_zero(event.get("frame_index")), []).append(event)

    frames = [row for row in native.get("game_frames", []) if isinstance(row, dict)]
    results: list[dict[str, Any]] = []
    for frame in frames:
        duration_ms = float_or_none(frame.get("duration_ms")) or 0.0
        if duration_ms < hitch_threshold_ms:
            continue
        frame_index = int_or_zero(frame.get("index"))
        frame_events = by_frame.get(frame_index, [])
        contributors = sorted(
            frame_events,
            key=lambda row: (float_or_none(row.get("self_time_ms")) or 0.0, float_or_none(row.get("duration_ms")) or 0.0),
            reverse=True,
        )[:20]
        thread_root: dict[str, float] = {}
        for event in frame_events:
            if int_or_zero(event.get("depth")) == 0:
                thread = str(event.get("thread") or "Unknown")
                thread_root[thread] = thread_root.get(thread, 0.0) + (float_or_none(event.get("duration_ms")) or 0.0)
        critical_thread = max(thread_root, key=thread_root.get) if thread_root else "unknown"
        results.append(
            {
                "frame_index": frame_index,
                "start_seconds": float_or_none(frame.get("start_seconds")) or 0.0,
                "end_seconds": float_or_none(frame.get("end_seconds")) or 0.0,
                "duration_ms": duration_ms,
                "critical_thread": critical_thread,
                "thread_root_ms": dict(sorted(thread_root.items(), key=lambda item: item[1], reverse=True)),
                "top_contributors": [
                    {
                        "timer": row.get("timer") or "<unnamed>",
                        "thread": row.get("thread") or "",
                        "inclusive_ms": round(float_or_none(row.get("duration_ms")) or 0.0, 4),
                        "self_ms": round(float_or_none(row.get("self_time_ms")) or 0.0, 4),
                        "depth": int_or_zero(row.get("depth")),
                    }
                    for row in contributors
                ],
            }
        )
    results.sort(key=lambda row: row["duration_ms"], reverse=True)
    return results


def build_coverage_manifest(metadata: dict[str, Any], native: dict[str, Any]) -> dict[str, Any]:
    requested = {str(value).lower() for value in metadata.get("channels", [])}
    provider_map = {
        "frames": {"frame"},
        "timing": {"cpu", "gpu", "slate", "animation"},
        "counters": {"counters", "stats"},
        "bookmarks": {"bookmark", "region"},
        "tasks": {"task"},
        "file_activity": {"file"},
        "load_time": {"loadtime"},
        "logs": {"log"},
        "context_switches": {"contextswitch"},
        "stack_samples": {"stacksampling"},
        "memory_tags": {"memory", "memtag"},
        "allocations": {"memory", "memalloc"},
        "objects": {"metadata", "assetmetadata"},
        "network": {"net"},
        "screenshots": {"screenshot"},
    }
    native_coverage = native.get("coverage") if isinstance(native.get("coverage"), dict) else {}
    providers: dict[str, Any] = {}
    missing_requested: list[str] = []
    for provider, channels in provider_map.items():
        requested_here = bool(requested & channels)
        raw = native_coverage.get(provider) if isinstance(native_coverage.get(provider), dict) else {}
        available = bool(raw.get("available"))
        status = "available" if available else ("missing" if requested_here else "not_requested")
        providers[provider] = {"status": status, "requested_channels": sorted(requested & channels), "count": raw.get("count")}
        if status == "missing":
            missing_requested.append(provider)
    launch_requirements_satisfied = bool(metadata.get("launch_requirements_satisfied", True))
    return {
        "native_extractor_available": bool(native),
        "requested_channels": sorted(requested),
        "required_launch_arguments": metadata.get("required_launch_arguments", []),
        "launch_requirements_satisfied": launch_requirements_satisfied,
        "providers": providers,
        "missing_requested_providers": missing_requested,
        "complete_for_requested_profile": bool(native) and not missing_requested and launch_requirements_satisfied,
    }


def build_performance_metrics(native: dict[str, Any], fallback_rows: list[dict[str, Any]], hitch_threshold_ms: float) -> dict[str, Any]:
    native_values = [float_or_none(row.get("duration_ms")) for row in native.get("game_frames", []) if isinstance(row, dict)]
    frame_values = [value for value in native_values if value is not None]
    if not frame_values:
        frame_values = [
            float(row["frame_time_ms"])
            for row in normalize_perf_rows(fallback_rows)
            if get_number(row, "frame_time_ms") is not None
        ]
    duration_seconds = float_or_none(native.get("duration_seconds")) or 0.0
    if duration_seconds <= 0.0 and frame_values:
        duration_seconds = sum(frame_values) / 1000.0
    hitch_count = sum(1 for value in frame_values if value >= hitch_threshold_ms)
    allocation = native.get("allocation_summary") if isinstance(native.get("allocation_summary"), dict) else {}
    memory_growth_mb = (float_or_none(allocation.get("growth_bytes")) or 0.0) / (1024.0 * 1024.0)
    return {
        "sample_count": len(frame_values),
        "duration_seconds": round(duration_seconds, 3),
        "frame_ms": {
            "mean": round(statistics.fmean(frame_values), 4) if frame_values else 0.0,
            "median": round(statistics.median(frame_values), 4) if frame_values else 0.0,
            "p90": round(percentile(frame_values, 90), 4),
            "p95": round(percentile(frame_values, 95), 4),
            "p99": round(percentile(frame_values, 99), 4),
            "p99_9": round(percentile(frame_values, 99.9), 4),
            "max": round(max(frame_values), 4) if frame_values else 0.0,
        },
        "hitch_count": hitch_count,
        "hitches_per_minute": round(hitch_count / max(duration_seconds / 60.0, 1e-9), 4) if duration_seconds else 0.0,
        "memory_growth_mb": round(memory_growth_mb, 4),
    }


def compare_baseline(args: argparse.Namespace, metrics: dict[str, Any], log: AnalyzerLog) -> tuple[dict[str, Any], Path | None]:
    if not args.baseline_dir:
        return {"status": "disabled"}, None
    args.baseline_dir.mkdir(parents=True, exist_ok=True)
    baseline_path = args.baseline_dir / f"{safe_baseline_key(args.baseline_key)}.json"
    comparison: dict[str, Any] = {"status": "missing", "baseline_path": str(baseline_path), "deltas": {}}
    if baseline_path.exists():
        baseline = load_json_file(baseline_path, log)
        previous_metrics = baseline.get("metrics") if isinstance(baseline.get("metrics"), dict) else {}
        previous_frame = previous_metrics.get("frame_ms") if isinstance(previous_metrics.get("frame_ms"), dict) else {}
        current_frame = metrics.get("frame_ms") if isinstance(metrics.get("frame_ms"), dict) else {}
        deltas: dict[str, Any] = {}
        for key in ("median", "p95", "p99", "max"):
            previous = float_or_none(previous_frame.get(key)) or 0.0
            current = float_or_none(current_frame.get(key)) or 0.0
            deltas[key] = {
                "baseline": previous,
                "current": current,
                "absolute_ms": round(current - previous, 4),
                "percent": round((current - previous) / previous * 100.0, 3) if previous > 0 else 0.0,
            }
        comparison = {"status": "compared", "baseline_path": str(baseline_path), "deltas": deltas}
    return comparison, baseline_path


def evaluate_ci_gates(args: argparse.Namespace, metrics: dict[str, Any], regression: dict[str, Any]) -> dict[str, Any]:
    gates: list[dict[str, Any]] = []
    if args.ci:
        baseline_available = regression.get("status") == "compared"
        gates.append({"name": "baseline_available", "value": 1 if baseline_available else 0, "limit": 1, "passed": baseline_available})
    p99_delta = float_or_none(((regression.get("deltas") or {}).get("p99") or {}).get("percent")) or 0.0
    gates.append({"name": "p99_regression_percent", "value": p99_delta, "limit": args.max_p99_regression_percent, "passed": p99_delta <= args.max_p99_regression_percent})
    hitch_rate = float_or_none(metrics.get("hitches_per_minute")) or 0.0
    gates.append({"name": "hitches_per_minute", "value": hitch_rate, "limit": args.max_hitches_per_minute, "passed": hitch_rate <= args.max_hitches_per_minute})
    memory_growth = float_or_none(metrics.get("memory_growth_mb")) or 0.0
    gates.append({"name": "memory_growth_mb", "value": memory_growth, "limit": args.max_memory_growth_mb, "passed": memory_growth <= args.max_memory_growth_mb})
    return {"enabled": bool(args.ci), "passed": all(gate["passed"] for gate in gates), "gates": gates}


def write_evidence_database(path: Path, native: dict[str, Any], report: dict[str, Any]) -> None:
    if path.exists():
        path.unlink()
    connection = sqlite3.connect(path)
    try:
        connection.execute("PRAGMA journal_mode=WAL")
        connection.execute("CREATE TABLE session (key TEXT PRIMARY KEY, value_json TEXT NOT NULL)")
        for key in ("schema_version", "extractor", "trace", "duration_seconds", "hitch_threshold_ms", "coverage"):
            connection.execute("INSERT INTO session VALUES (?, ?)", (key, json.dumps(native.get(key))))

        table_specs: dict[str, tuple[str, tuple[str, ...]]] = {
            "game_frames": ("CREATE TABLE game_frames (frame_index INTEGER, start_seconds REAL, end_seconds REAL, duration_ms REAL)", ("index", "start_seconds", "end_seconds", "duration_ms")),
            "render_frames": ("CREATE TABLE render_frames (frame_index INTEGER, start_seconds REAL, end_seconds REAL, duration_ms REAL)", ("index", "start_seconds", "end_seconds", "duration_ms")),
            "timing_events": ("CREATE TABLE timing_events (frame_index INTEGER, thread_id INTEGER, thread TEXT, timer_id INTEGER, timer TEXT, start_seconds REAL, end_seconds REAL, duration_ms REAL, depth INTEGER)", ("frame_index", "thread_id", "thread", "timer_id", "timer", "start_seconds", "end_seconds", "duration_ms", "depth")),
            "counters": ("CREATE TABLE counters (counter_id INTEGER, name TEXT, group_name TEXT, sample_count INTEGER, first_value REAL, last_value REAL, min_value REAL, max_value REAL, delta REAL)", ("id", "name", "group", "sample_count", "first", "last", "min", "max", "delta")),
            "bookmarks": ("CREATE TABLE bookmarks (time_seconds REAL, text TEXT, callstack_id INTEGER)", ("time_seconds", "text", "callstack_id")),
            "tasks": ("CREATE TABLE tasks (task_id TEXT, name TEXT, created_seconds REAL, scheduled_seconds REAL, started_seconds REAL, finished_seconds REAL, queue_delay_ms REAL, execution_ms REAL, prerequisite_count INTEGER, nested_count INTEGER)", ("id", "name", "created_seconds", "scheduled_seconds", "started_seconds", "finished_seconds", "queue_delay_ms", "execution_ms", "prerequisite_count", "nested_count")),
            "file_activity": ("CREATE TABLE file_activity (path TEXT, operation_count INTEGER, failure_count INTEGER, actual_bytes INTEGER, total_ms REAL, max_ms REAL)", ("path", "operation_count", "failure_count", "actual_bytes", "total_ms", "max_ms")),
            "package_loads": ("CREATE TABLE package_loads (package_id INTEGER, package TEXT, request_id TEXT, serialized_bytes INTEGER, header_bytes INTEGER, export_count INTEGER, export_bytes INTEGER, main_thread_ms REAL, async_loading_thread_ms REAL, total_ms REAL)", ("id", "package", "request_id", "serialized_bytes", "header_bytes", "export_count", "export_bytes", "main_thread_ms", "async_loading_thread_ms", "total_ms")),
            "export_loads": ("CREATE TABLE export_loads (export_id INTEGER, package TEXT, class_name TEXT, event TEXT, serialized_bytes INTEGER, main_thread_ms REAL, async_loading_thread_ms REAL, total_ms REAL)", ("id", "package", "class", "event", "serialized_bytes", "main_thread_ms", "async_loading_thread_ms", "total_ms")),
            "load_requests": ("CREATE TABLE load_requests (request_id TEXT, name TEXT, start_seconds REAL, duration_ms REAL, package_count INTEGER)", ("id", "name", "start_seconds", "duration_ms", "package_count")),
            "logs": ("CREATE TABLE logs (time_seconds REAL, category TEXT, message TEXT, verbosity INTEGER)", ("time_seconds", "category", "message", "verbosity")),
            "context_switches": ("CREATE TABLE context_switches (thread_id INTEGER, thread TEXT, switch_count INTEGER, running_ms REAL, scheduled_ratio REAL)", ("thread_id", "thread", "switch_count", "running_ms", "scheduled_ratio")),
            "stack_frames": ("CREATE TABLE stack_frames (timer_id INTEGER, address TEXT, module TEXT, symbol TEXT, file TEXT, line INTEGER)", ("timer_id", "address", "module", "symbol", "file", "line")),
            "stack_sample_events": ("CREATE TABLE stack_sample_events (frame_index INTEGER, system_thread_id INTEGER, thread TEXT, timer_id INTEGER, symbol TEXT, module TEXT, start_seconds REAL, end_seconds REAL, duration_ms REAL, depth INTEGER)", ("frame_index", "system_thread_id", "thread", "timer_id", "symbol", "module", "start_seconds", "end_seconds", "duration_ms", "depth")),
            "memory_tags": ("CREATE TABLE memory_tags (tracker TEXT, tag TEXT, tag_id TEXT, sample_count INTEGER, first_bytes INTEGER, last_bytes INTEGER, peak_bytes INTEGER, growth_bytes INTEGER)", ("tracker", "tag", "tag_id", "sample_count", "first_bytes", "last_bytes", "peak_bytes", "growth_bytes")),
            "object_snapshots": ("CREATE TABLE object_snapshots (snapshot_id INTEGER, start_seconds REAL, end_seconds REAL, object_count INTEGER, object_array_size INTEGER, reference_count INTEGER, traced_object_count INTEGER, has_total_memory_sizes INTEGER)", ("id", "start_seconds", "end_seconds", "object_count", "object_array_size", "reference_count", "traced_object_count", "has_total_memory_sizes")),
            "object_classes": ("CREATE TABLE object_classes (class_name TEXT, instance_count INTEGER, system_bytes INTEGER, video_bytes INTEGER, total_bytes INTEGER)", ("class", "count", "system_bytes", "video_bytes", "total_bytes")),
            "network_connections": ("CREATE TABLE network_connections (instance TEXT, server INTEGER, connection_name TEXT, address TEXT, mode TEXT, packet_count INTEGER, total_bytes INTEGER, max_packet_bytes INTEGER, dropped_packets INTEGER)", ("instance", "server", "connection", "address", "mode", "packet_count", "total_bytes", "max_packet_bytes", "dropped_packets")),
        }
        for table, (create_sql, columns) in table_specs.items():
            connection.execute(create_sql)
            placeholders = ",".join("?" for _ in columns)
            rows = native.get(table, [])
            if isinstance(rows, list):
                connection.executemany(
                    f"INSERT INTO {table} VALUES ({placeholders})",
                    [tuple(row.get(column) for column in columns) for row in rows if isinstance(row, dict)],
                )
        connection.execute("CREATE INDEX idx_frames_duration ON game_frames(duration_ms DESC)")
        connection.execute("CREATE INDEX idx_timing_frame_thread ON timing_events(frame_index, thread, depth)")
        connection.execute("CREATE INDEX idx_timing_timer ON timing_events(timer)")
        connection.execute("CREATE INDEX idx_stack_samples_frame ON stack_sample_events(frame_index, thread, symbol)")
        connection.execute("CREATE INDEX idx_package_load_time ON package_loads(total_ms DESC)")
        connection.execute("CREATE INDEX idx_export_load_time ON export_loads(total_ms DESC)")
        connection.execute("CREATE INDEX idx_object_class_bytes ON object_classes(total_bytes DESC)")
        connection.execute("CREATE TABLE findings (id TEXT PRIMARY KEY, severity TEXT, category TEXT, confidence REAL, title TEXT, finding_json TEXT)")
        for finding in report.get("findings", []):
            connection.execute(
                "INSERT INTO findings VALUES (?, ?, ?, ?, ?, ?)",
                (finding.get("id"), finding.get("severity"), finding.get("category"), finding.get("confidence"), finding.get("title"), json.dumps(finding)),
            )
        connection.commit()
    finally:
        connection.close()


class FindingBuilder:
    def __init__(self) -> None:
        self.findings: list[dict[str, Any]] = []

    def add(
        self,
        severity: str,
        category: str,
        title: str,
        evidence: list[str],
        suggested_next_step: str,
        screenshots: list[str] | None = None,
        time_range: dict[str, Any] | None = None,
        confidence: float = 0.75,
        attribution: str = "",
        evidence_ids: list[str] | None = None,
        alternative_explanations: list[str] | None = None,
    ) -> None:
        finding: dict[str, Any] = {
            "id": f"PS-{len(self.findings) + 1:04d}",
            "severity": severity,
            "category": category,
            "title": title,
            "evidence": evidence,
            "suggested_next_step": suggested_next_step,
            "confidence": round(max(0.0, min(1.0, confidence)), 3),
            "attribution": attribution,
            "evidence_ids": evidence_ids or [],
            "alternative_explanations": alternative_explanations or [],
        }
        if screenshots:
            finding["screenshots"] = screenshots
        if time_range:
            finding["time_range"] = time_range
        self.findings.append(finding)


def add_native_findings(builder: FindingBuilder, native: dict[str, Any], attribution: list[dict[str, Any]], coverage: dict[str, Any]) -> None:
    if attribution:
        worst = attribution[0]
        contributors = worst.get("top_contributors", [])[:5]
        contributor_text = "; ".join(
            f"{row.get('timer')} [{row.get('thread')}] self={float(row.get('self_ms', 0)):.2f}ms incl={float(row.get('inclusive_ms', 0)):.2f}ms"
            for row in contributors
        )
        builder.add(
            severity="error" if float(worst.get("duration_ms", 0)) >= 66.66 else "warning",
            category="frame_critical_path",
            title="Native trace analysis identified the critical hitch path",
            evidence=[
                f"Frame {worst.get('frame_index')} took {float(worst.get('duration_ms', 0)):.2f} ms.",
                f"Critical thread/queue: {worst.get('critical_thread', 'unknown')}.",
                f"Top self-time contributors: {contributor_text or 'no named timing scopes were available'}.",
            ],
            suggested_next_step="Open the trace at the recorded start/end interval and inspect the named scopes and their callers/callees.",
            time_range={"start_seconds": worst.get("start_seconds"), "end_seconds": worst.get("end_seconds"), "frame_index": worst.get("frame_index")},
            confidence=0.95 if contributors else 0.65,
            attribution=f"Frame-local nested CPU/GPU events point to {worst.get('critical_thread', 'unknown')} as the longest traced execution path.",
            evidence_ids=[f"game_frames:{worst.get('frame_index')}", f"timing_events:frame={worst.get('frame_index')}"],
            alternative_explanations=["Untraced work, driver stalls, or OS scheduling may contribute when the corresponding provider is unavailable."],
        )

    tasks = [row for row in native.get("tasks", []) if isinstance(row, dict)]
    queue_delays = [row for row in tasks if (float_or_none(row.get("queue_delay_ms")) or 0.0) >= 5.0]
    if queue_delays:
        queue_delays.sort(key=lambda row: float_or_none(row.get("queue_delay_ms")) or 0.0, reverse=True)
        worst = queue_delays[0]
        builder.add(
            severity="warning",
            category="task_queue_delay",
            title="Task scheduling delay contributed measurable latency",
            evidence=[
                f"{len(queue_delays)} task(s) waited at least 5 ms between scheduling and execution.",
                f"Worst task was {worst.get('name') or worst.get('id')} at {float(worst.get('queue_delay_ms', 0)):.2f} ms queue delay.",
            ],
            suggested_next_step="Inspect prerequisites, worker saturation, and waits around the slowest task in Task Graph Insights.",
            confidence=0.9,
            attribution="Task provider timestamps directly measure scheduled-to-start latency.",
            evidence_ids=[f"tasks:{worst.get('id')}"],
        )

    stack_events = [row for row in native.get("stack_sample_events", []) if isinstance(row, dict)]
    if stack_events:
        symbol_counts = Counter(
            (str(row.get("module") or ""), str(row.get("symbol") or "<unresolved>"), str(row.get("thread") or ""))
            for row in stack_events
        )
        (module, symbol, thread), sample_count = symbol_counts.most_common(1)[0]
        builder.add(
            severity="info",
            category="stack_sampling",
            title="Stack samples corroborate code active during hitch windows",
            evidence=[
                f"Captured {len(stack_events)} stack-sample event(s) across hitch windows.",
                f"Most frequent sampled frame: {module}!{symbol} on {thread} ({sample_count} sample(s)).",
            ],
            suggested_next_step="Use the sampled symbol as a caller-level lead, then confirm its inclusive and self time in Timing Insights before optimizing it.",
            confidence=0.8,
            attribution="The stack sampler observed this symbol most frequently during frame intervals already classified as hitches.",
            evidence_ids=[f"stack_sample_events:{module}!{symbol}"],
            alternative_explanations=["Sampling frequency is statistical and can overrepresent long waits or commonly active callers."],
        )

    files = [row for row in native.get("file_activity", []) if isinstance(row, dict)]
    slow_files = [row for row in files if (float_or_none(row.get("max_ms")) or 0.0) >= 5.0]
    if slow_files:
        slow_files.sort(key=lambda row: float_or_none(row.get("max_ms")) or 0.0, reverse=True)
        worst = slow_files[0]
        builder.add(
            severity="warning",
            category="file_io",
            title="Slow file activity was present in the trace",
            evidence=[
                f"{len(slow_files)} file(s) had an operation lasting at least 5 ms.",
                f"Worst path: {worst.get('path')} max={float(worst.get('max_ms', 0)):.2f} ms total={float(worst.get('total_ms', 0)):.2f} ms.",
            ],
            suggested_next_step="Correlate the file interval with hitch frames and replace unexpected synchronous loads with preloading or asynchronous streaming.",
            confidence=0.85,
            attribution="FileActivity provider measured the I/O operation duration.",
            evidence_ids=[f"file_activity:{worst.get('path')}"],
        )

    package_loads = [row for row in native.get("package_loads", []) if isinstance(row, dict)]
    slow_packages = [row for row in package_loads if (float_or_none(row.get("total_ms")) or 0.0) >= 10.0]
    if slow_packages:
        slow_packages.sort(key=lambda row: float_or_none(row.get("total_ms")) or 0.0, reverse=True)
        worst = slow_packages[0]
        builder.add(
            severity="warning",
            category="asset_loading",
            title="Package loading consumed significant traced thread time",
            evidence=[
                f"{len(slow_packages)} package(s) consumed at least 10 ms across the main and async loading threads.",
                f"Worst package: {worst.get('package')} total={float(worst.get('total_ms', 0)):.2f} ms, main={float(worst.get('main_thread_ms', 0)):.2f} ms, async={float(worst.get('async_loading_thread_ms', 0)):.2f} ms.",
                f"Serialized {(float(worst.get('serialized_bytes', 0)) / (1024 * 1024)):.2f} MB across {int_or_zero(worst.get('export_count'))} export(s).",
            ],
            suggested_next_step="Inspect the matching load request and export rows; move avoidable main-thread serialization/post-load work out of the hitch window and preload recurring packages.",
            confidence=0.9,
            attribution="LoadTimeProfiler directly aggregated package work on the main and async loading threads.",
            evidence_ids=[f"package_loads:{worst.get('id')}"],
        )

    load_requests = [row for row in native.get("load_requests", []) if isinstance(row, dict)]
    slow_requests = [row for row in load_requests if (float_or_none(row.get("duration_ms")) or 0.0) >= 20.0]
    if slow_requests:
        slow_requests.sort(key=lambda row: float_or_none(row.get("duration_ms")) or 0.0, reverse=True)
        worst = slow_requests[0]
        builder.add(
            severity="warning",
            category="load_request_latency",
            title="A load request remained active long enough to affect responsiveness",
            evidence=[
                f"{len(slow_requests)} load request(s) lasted at least 20 ms.",
                f"Worst request: {worst.get('name') or worst.get('id')} duration={float(worst.get('duration_ms', 0)):.2f} ms packages={int_or_zero(worst.get('package_count'))}.",
            ],
            suggested_next_step="Correlate this request with package and file rows to separate storage latency from serialization, dependency expansion, and post-load work.",
            confidence=0.9,
            attribution="LoadTimeProfiler request start and duration fields directly describe request lifetime.",
            evidence_ids=[f"load_requests:{worst.get('id')}"],
        )

    memory_tags = [row for row in native.get("memory_tags", []) if isinstance(row, dict)]
    growing_tags = [row for row in memory_tags if (float_or_none(row.get("growth_bytes")) or 0.0) >= 50 * 1024 * 1024]
    if growing_tags:
        growing_tags.sort(key=lambda row: float_or_none(row.get("growth_bytes")) or 0.0, reverse=True)
        worst = growing_tags[0]
        builder.add(
            severity="warning",
            category="memory_tag_growth",
            title="Memory trace recorded substantial unreclaimed tag growth",
            evidence=[
                f"{worst.get('tracker')} / {worst.get('tag')} grew by {(float(worst.get('growth_bytes', 0)) / (1024 * 1024)):.1f} MB.",
                f"Peak was {(float(worst.get('peak_bytes', 0)) / (1024 * 1024)):.1f} MB across {int_or_zero(worst.get('sample_count'))} samples.",
            ],
            suggested_next_step="Query allocations in the growth interval by tag, asset metadata, and callstack to identify surviving owners.",
            confidence=0.9,
            attribution="LLM tag samples show end-minus-start growth; retention still requires allocation/callstack confirmation.",
            evidence_ids=[f"memory_tags:{worst.get('tracker')}:{worst.get('tag_id')}"],
            alternative_explanations=["The scenario may intentionally warm caches or retain streamed content."],
        )

    object_snapshots = [row for row in native.get("object_snapshots", []) if isinstance(row, dict)]
    if len(object_snapshots) >= 2:
        first_count = int_or_zero(object_snapshots[0].get("object_count"))
        last_count = int_or_zero(object_snapshots[-1].get("object_count"))
        growth = last_count - first_count
        if growth >= max(1000, int(first_count * 0.05)):
            top_classes = [row for row in native.get("object_classes", []) if isinstance(row, dict)][:5]
            top_text = "; ".join(
                f"{row.get('class')} count={int_or_zero(row.get('count'))} memory={(float(row.get('total_bytes', 0)) / (1024 * 1024)):.1f}MB"
                for row in top_classes
            )
            builder.add(
                severity="warning",
                category="object_snapshot_growth",
                title="Object trace snapshots grew materially during the scenario",
                evidence=[
                    f"Traced object count grew by {growth}, from {first_count} to {last_count}.",
                    f"Largest final-snapshot classes: {top_text or 'class memory metadata unavailable'}.",
                ],
                suggested_next_step="Compare snapshots in Object/Memory Insights and inspect references retaining the growing classes after the scenario cleanup point.",
                confidence=0.85,
                attribution="ObjectProvider snapshot counts measure live traced objects; final class totals identify investigation targets but not ownership by themselves.",
                evidence_ids=[f"object_snapshots:{object_snapshots[0].get('id')}->{object_snapshots[-1].get('id')}"],
                alternative_explanations=["Cache warmup, intentionally persistent subsystems, and streamed content can produce legitimate growth."],
            )

    network = [row for row in native.get("network_connections", []) if isinstance(row, dict)]
    dropped = [row for row in network if int_or_zero(row.get("dropped_packets")) > 0]
    if dropped:
        total_dropped = sum(int_or_zero(row.get("dropped_packets")) for row in dropped)
        builder.add(
            severity="warning",
            category="network_packet_loss",
            title="Network trace recorded dropped packets",
            evidence=[f"{total_dropped} dropped packet(s) were recorded across {len(dropped)} connection/direction stream(s)."],
            suggested_next_step="Inspect packet content, RPC/property contributors, reliable backlog, and connection conditions in Networking Insights.",
            confidence=0.95,
            attribution="Networking Insights delivery status directly marked these packets as dropped.",
            evidence_ids=["network_connections:dropped_packets>0"],
        )

    missing = coverage.get("missing_requested_providers", [])
    if missing:
        builder.add(
            severity="warning",
            category="trace_coverage",
            title="Requested trace providers were missing from the capture",
            evidence=[f"Missing requested providers: {', '.join(str(value) for value in missing)}."],
            suggested_next_step="Relaunch with the profile's required command-line arguments and verify the channel manifest before trusting absence-of-evidence conclusions.",
            confidence=1.0,
            attribution="Coverage validation compared requested channels with providers actually materialized by TraceServices.",
            evidence_ids=["coverage:missing_requested_providers"],
        )

    if not coverage.get("launch_requirements_satisfied", True):
        required = coverage.get("required_launch_arguments", [])
        builder.add(
            severity="warning",
            category="trace_launch_requirements",
            title="Capture started without the selected profile's startup-only providers",
            evidence=[f"Required process arguments were not present: {' '.join(str(value) for value in required)}."],
            suggested_next_step="Use Window > PerfSentinel > Launch Profile Session or add the copied profile arguments to the packaged-build launcher, then recapture.",
            confidence=1.0,
            attribution="The capture metadata compared the active process command line with the profile's declared launch requirements.",
            evidence_ids=["metadata:launch_requirements_satisfied=false"],
        )


def generate_findings(
    spikes: list[dict[str, Any]],
    fallback_rows: list[dict[str, Any]],
    timers: list[dict[str, Any]],
    frame_budget_ms: float,
    hitch_threshold_ms: float,
    severe_frame_budget_ms: float,
    counters: list[dict[str, Any]] | None = None,
    native: dict[str, Any] | None = None,
    frame_attribution: list[dict[str, Any]] | None = None,
    coverage: dict[str, Any] | None = None,
) -> list[dict[str, Any]]:
    builder = FindingBuilder()
    all_frame_rows = normalize_perf_rows(spikes + fallback_rows)

    spike_rows = [row for row in normalize_perf_rows(spikes) if get_number(row, "frame_time_ms") is not None]
    hitch_spikes = [row for row in spike_rows if float(row["frame_time_ms"]) >= hitch_threshold_ms]
    if hitch_spikes:
        worst = max(hitch_spikes, key=lambda row: float(row["frame_time_ms"]))
        severity = "error" if float(worst["frame_time_ms"]) >= severe_frame_budget_ms else "warning"
        builder.add(
            severity=severity,
            category="frame_hitch",
            title="Frame-time spike captured during trace",
            evidence=[
                f"{len(hitch_spikes)} spike event(s) met or exceeded the {hitch_threshold_ms:.2f} ms hitch threshold.",
                f"Worst captured frame was {float(worst['frame_time_ms']):.2f} ms on frame {worst.get('frame_number', 'unknown')}.",
            ],
            screenshots=screenshot_list(hitch_spikes),
            time_range={"timestamp": worst.get("timestamp")} if worst.get("timestamp") else None,
            suggested_next_step="Open the trace near the spike timestamp/frame and inspect Game Thread, Render Thread, RHI Thread, and GPU timing tracks.",
        )

    fallback_frame_rows = [row for row in normalize_perf_rows(fallback_rows) if get_number(row, "frame_time_ms") is not None]
    over_budget = [row for row in fallback_frame_rows if float(row["frame_time_ms"]) > frame_budget_ms]
    if fallback_frame_rows and len(over_budget) >= max(5, int(len(fallback_frame_rows) * 0.2)):
        worst = max(over_budget, key=lambda row: float(row["frame_time_ms"]))
        builder.add(
            severity="warning",
            category="frame_budget",
            title="Sustained frame time exceeded the target budget",
            evidence=[
                f"{len(over_budget)} of {len(fallback_frame_rows)} fallback frame sample(s) exceeded {frame_budget_ms:.2f} ms.",
                f"Worst fallback frame was {float(worst['frame_time_ms']):.2f} ms.",
            ],
            time_range={"timestamp": worst.get("timestamp")} if worst.get("timestamp") else None,
            suggested_next_step="Use the timer statistics and timing tracks to identify which recurring scopes dominate over-budget frames.",
        )

    thread_rules = [
        ("game_thread_ms", "game_thread", "Game thread spike detected", "Inspect high Game Thread timer scopes and gameplay/update work around the spike."),
        ("render_thread_ms", "render_thread", "Render thread spike detected", "Inspect Render Thread scopes, scene updates, visibility, and draw submission around the spike."),
        ("rhi_thread_ms", "rhi_thread", "RHI thread spike detected", "Inspect RHI submission, resource creation, and driver-facing work around the spike."),
        ("gpu_ms", "gpu", "GPU frame spike detected", "Inspect GPU timing, passes, materials, shadows, translucency, and resolution-dependent work around the spike."),
    ]
    for key, category, title, next_step in thread_rules:
        rows_with_value = [row for row in all_frame_rows if get_number(row, key) is not None]
        exceeding = [row for row in rows_with_value if float(row[key]) >= hitch_threshold_ms]
        if not exceeding:
            continue
        worst = max(exceeding, key=lambda row: float(row[key]))
        builder.add(
            severity="warning",
            category=category,
            title=title,
            evidence=[
                f"{len(exceeding)} sample(s) reported {key} at or above {hitch_threshold_ms:.2f} ms.",
                f"Worst {key} value was {float(worst[key]):.2f} ms.",
            ],
            screenshots=screenshot_list(exceeding),
            time_range={"timestamp": worst.get("timestamp")} if worst.get("timestamp") else None,
            suggested_next_step=next_step,
        )

    expensive_timers = [timer for timer in timers if float(timer.get("max_time_ms", 0.0)) > hitch_threshold_ms]
    if expensive_timers:
        top = expensive_timers[:5]
        builder.add(
            severity="warning",
            category="timer_scope",
            title="Timer scope exceeded hitch threshold",
            evidence=[
                f"{len(expensive_timers)} timer scope(s) reported max inclusive time above {hitch_threshold_ms:.2f} ms.",
                "Top scopes: "
                + "; ".join(
                    f"{timer['name']} max={float(timer['max_time_ms']):.2f} ms avg={float(timer['avg_time_ms']):.2f} ms calls={timer['count']}"
                    for timer in top
                ),
            ],
            suggested_next_step="Open the most expensive timer scopes in Unreal Insights and confirm whether the high max values align with captured frame spikes.",
        )

    generate_counter_findings(builder, counters or [])

    if native:
        add_native_findings(builder, native, frame_attribution or [], coverage or {})

    if not builder.findings:
        builder.add(
            severity="info",
            category="summary",
            title="No generic performance findings crossed configured thresholds",
            evidence=[
                f"No spike, fallback, or timer-statistics rule crossed the {hitch_threshold_ms:.2f} ms hitch threshold.",
            ],
            suggested_next_step="If the session still felt slow, capture a longer trace or add more precise scenario markers before drawing conclusions.",
        )

    return builder.findings


def severity_counts(findings: list[dict[str, Any]]) -> dict[str, int]:
    return dict(Counter(str(finding.get("severity", "unknown")) for finding in findings))


def build_report(
    args: argparse.Namespace,
    metadata: dict[str, Any],
    status: str,
    spikes: list[dict[str, Any]],
    fallback_rows: list[dict[str, Any]],
    timers: list[dict[str, Any]],
    threads: list[dict[str, Any]],
    findings: list[dict[str, Any]],
    artifacts: dict[str, str],
    counters: list[dict[str, Any]] | None = None,
    coverage: dict[str, Any] | None = None,
    metrics: dict[str, Any] | None = None,
    frame_attribution: list[dict[str, Any]] | None = None,
    regression: dict[str, Any] | None = None,
    ci_gates: dict[str, Any] | None = None,
) -> dict[str, Any]:
    normalized_spikes = normalize_perf_rows(spikes)
    normalized_fallback = normalize_perf_rows(fallback_rows)
    frame_values = [
        float(row["frame_time_ms"])
        for row in normalized_spikes + normalized_fallback
        if get_number(row, "frame_time_ms") is not None
    ]
    worst_frame_ms = max(frame_values) if frame_values else 0.0
    native_worst_frame_ms = float(((metrics or {}).get("frame_ms") or {}).get("max", 0.0))
    worst_frame_ms = max(worst_frame_ms, native_worst_frame_ms)
    spike_screenshots = screenshot_list(normalized_spikes)

    budgets = dict(metadata.get("budgets") or {})
    budgets.setdefault("frame_budget_ms", args.frame_budget_ms)
    budgets.setdefault("hitch_threshold_ms", args.hitch_threshold_ms)

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_at": utc_now_iso(),
        "analyzer": ANALYZER_NAME,
        "project": metadata.get("project", ""),
        "scenario": metadata.get("scenario", args.trace.stem),
        "trace": {
            "path": str(args.trace),
            "started_at": metadata.get("started_at", ""),
            "stopped_at": metadata.get("stopped_at", ""),
            "duration_seconds": metadata.get("duration_seconds", 0),
            "engine_version": metadata.get("engine_version", ""),
            "channels": metadata.get("channels", []),
        },
        "budgets": budgets,
        "summary": {
            "status": status,
            "finding_count": len(findings),
            "severity_counts": severity_counts(findings),
            "spike_count": len(spikes),
            "worst_frame_ms": round(worst_frame_ms, 3),
            "p99_frame_ms": float(((metrics or {}).get("frame_ms") or {}).get("p99", 0.0)),
            "hitches_per_minute": float((metrics or {}).get("hitches_per_minute", 0.0)),
        },
        "artifacts": {
            "metadata": artifacts.get("metadata", ""),
            "analysis_log": artifacts.get("analysis_log", ""),
            "unreal_insights_log": artifacts.get("unreal_insights_log", ""),
            "timer_statistics_csv": artifacts.get("timer_statistics_csv", ""),
            "threads_csv": artifacts.get("threads_csv", ""),
            "timers_csv": artifacts.get("timers_csv", ""),
            "counters_csv": artifacts.get("counters_csv", ""),
            "game_stats_json": artifacts.get("game_stats_json", ""),
            "native_evidence_json": artifacts.get("native_evidence_json", ""),
            "evidence_database": artifacts.get("evidence_database", ""),
            "baseline": artifacts.get("baseline", ""),
            "spike_screenshots": spike_screenshots,
        },
        "spike_analysis": {
            "spike_count": len(spikes),
            "fallback_sample_count": len(fallback_rows),
            "worst_frame_ms": round(worst_frame_ms, 3),
        },
        "counters_summary": summarize_counters(counters or []),
        "coverage": coverage or {},
        "metrics": metrics or {},
        "frame_attribution": (frame_attribution or [])[:50],
        "regression": regression or {"status": "disabled"},
        "ci": ci_gates or {"enabled": False, "passed": True, "gates": []},
        "top_timers": timers[:25],
        "thread_summary": threads,
        "findings": findings,
    }


def md_escape(value: Any) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def render_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    lines: list[str] = []
    lines.append("# PerfSentinel Findings")
    lines.append("")
    lines.append(f"- Project: {report.get('project') or '-'}")
    lines.append(f"- Scenario: {report.get('scenario') or '-'}")
    lines.append(f"- Trace: `{report['trace'].get('path') or '-'}`")
    lines.append(f"- Trace Data: {summary.get('status', 'not_available')}")
    lines.append(f"- Findings: {summary.get('finding_count', 0)}")
    lines.append(f"- Spike Count: {summary.get('spike_count', 0)}")
    lines.append(f"- Worst Frame: {float(summary.get('worst_frame_ms', 0.0)):.2f} ms")
    lines.append(f"- P99 Frame: {float(summary.get('p99_frame_ms', 0.0)):.2f} ms")
    lines.append(f"- Hitches/minute: {float(summary.get('hitches_per_minute', 0.0)):.2f}")
    lines.append("")

    coverage = report.get("coverage", {})
    lines.append("## Trace Coverage")
    lines.append("")
    lines.append(f"- Native extractor: {'available' if coverage.get('native_extractor_available') else 'not available'}")
    lines.append(f"- Complete for requested profile: {'yes' if coverage.get('complete_for_requested_profile') else 'no'}")
    missing = coverage.get("missing_requested_providers", [])
    lines.append(f"- Missing requested providers: {', '.join(missing) if missing else 'none'}")
    lines.append("")

    lines.append("## Findings")
    lines.append("")
    for finding in report.get("findings", []):
        lines.append(f"### {finding['id']} {finding['title']}")
        lines.append("")
        lines.append(f"- Severity: {finding.get('severity', '-')}")
        lines.append(f"- Category: {finding.get('category', '-')}")
        lines.append(f"- Confidence: {float(finding.get('confidence', 0.0)):.0%}")
        if finding.get("attribution"):
            lines.append(f"- Attribution: {finding.get('attribution')}")
        for evidence in finding.get("evidence", []):
            lines.append(f"- Evidence: {evidence}")
        if finding.get("screenshots"):
            for screenshot in finding["screenshots"]:
                lines.append(f"- Screenshot: `{screenshot}`")
        lines.append(f"- Suggested next step: {finding.get('suggested_next_step', '-')}")
        lines.append("")

    attribution = report.get("frame_attribution", [])
    if attribution:
        lines.append("## Worst Frame Attribution")
        lines.append("")
        lines.append("| Frame | Duration ms | Critical thread | Top contributor | Self ms | Inclusive ms |")
        lines.append("| ---: | ---: | --- | --- | ---: | ---: |")
        for frame in attribution[:20]:
            top = (frame.get("top_contributors") or [{}])[0]
            lines.append(
                f"| {int(frame.get('frame_index', 0))} | {float(frame.get('duration_ms', 0)):.3f} | "
                f"{md_escape(frame.get('critical_thread', '-'))} | {md_escape(top.get('timer', '-'))} | "
                f"{float(top.get('self_ms', 0)):.3f} | {float(top.get('inclusive_ms', 0)):.3f} |"
            )
        lines.append("")

    regression = report.get("regression", {})
    lines.append("## Regression")
    lines.append("")
    lines.append(f"- Status: {regression.get('status', 'disabled')}")
    if regression.get("baseline_path"):
        lines.append(f"- Baseline: `{regression.get('baseline_path')}`")
    for key, delta in (regression.get("deltas") or {}).items():
        lines.append(f"- {key}: {float(delta.get('current', 0)):.3f} ms ({float(delta.get('percent', 0)):+.2f}% vs baseline)")
    lines.append("")

    ci = report.get("ci", {})
    if ci.get("enabled"):
        lines.append("## CI Gates")
        lines.append("")
        lines.append(f"- Overall: {'PASS' if ci.get('passed') else 'FAIL'}")
        for gate in ci.get("gates", []):
            lines.append(
                f"- {'PASS' if gate.get('passed') else 'FAIL'} {gate.get('name')}: "
                f"{float(gate.get('value', 0)):.3f} (limit {float(gate.get('limit', 0)):.3f})"
            )
        lines.append("")

    counters_summary = report.get("counters_summary", {})
    if counters_summary:
        lines.append("## Runtime Counters & Leak Summary")
        lines.append("")
        lines.append(f"- Samples: {counters_summary.get('sample_count', 0)}")
        if "gc_count_total" in counters_summary:
            lines.append(f"- Garbage collections: {counters_summary.get('gc_count_total')}")
        lines.append("")
        lines.append("| Counter | Start | End | Min | Max | Δ |")
        lines.append("| --- | ---: | ---: | ---: | ---: | ---: |")

        def fmt_metric(key: str, scale: float = 1.0, label: str | None = None) -> None:
            stats = counters_summary.get(key)
            if not isinstance(stats, dict):
                return
            name = label or key
            lines.append(
                f"| {md_escape(name)} | {stats['start'] / scale:.1f} | {stats['end'] / scale:.1f} | "
                f"{stats['min'] / scale:.1f} | {stats['max'] / scale:.1f} | {stats['delta'] / scale:+.1f} |"
            )

        fmt_metric("actor_count", label="actor_count")
        fmt_metric("component_count", label="component_count")
        fmt_metric("uobject_count", label="uobject_count")
        fmt_metric("memory_used_physical_bytes", scale=1024.0 * 1024.0, label="memory_used (MB)")
        lines.append("")

        object_growth = [g for g in counters_summary.get("uobject_class_growth", []) if g.get("delta", 0) > 0]
        if object_growth:
            lines.append("Top growing UObject classes:")
            lines.append("")
            lines.append("| Class | Start | End | Δ |")
            lines.append("| --- | ---: | ---: | ---: |")
            for entry in object_growth[:10]:
                lines.append(
                    f"| {md_escape(entry.get('class', '-'))} | {int(entry.get('start', 0))} | "
                    f"{int(entry.get('end', 0))} | {int(entry.get('delta', 0)):+d} |"
                )
            lines.append("")

    lines.append("## Top Timers")
    lines.append("")
    lines.append("| Timer | Total ms | Avg ms | Max ms | Calls |")
    lines.append("| --- | ---: | ---: | ---: | ---: |")
    top_timers = report.get("top_timers", [])[:20]
    if top_timers:
        for timer in top_timers:
            lines.append(
                f"| {md_escape(timer.get('name', '-'))} | "
                f"{float(timer.get('total_time_ms', 0.0)):.3f} | "
                f"{float(timer.get('avg_time_ms', 0.0)):.3f} | "
                f"{float(timer.get('max_time_ms', 0.0)):.3f} | "
                f"{int(timer.get('count', 0))} |"
            )
    else:
        lines.append("| - | - | - | - | - |")
    lines.append("")

    lines.append("## Thread Summary")
    lines.append("")
    lines.append("| Id | Thread | Group |")
    lines.append("| ---: | --- | --- |")
    threads = report.get("thread_summary", [])
    if threads:
        for thread in threads:
            lines.append(f"| {md_escape(thread.get('id', '-'))} | {md_escape(thread.get('name', '-'))} | {md_escape(thread.get('group', '-'))} |")
    else:
        lines.append("| - | - | - |")
    lines.append("")

    lines.append("## Artifacts")
    lines.append("")
    artifacts = report.get("artifacts", {})
    for key in ["metadata", "analysis_log", "native_evidence_json", "evidence_database", "baseline", "unreal_insights_log", "timer_statistics_csv", "threads_csv", "timers_csv", "counters_csv", "game_stats_json"]:
        value = artifacts.get(key)
        if value:
            lines.append(f"- {key}: `{value}`")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    args.trace = args.trace.resolve()
    args.metadata = args.metadata.resolve()
    args.out = args.out.resolve()
    if args.fallback_stats:
        args.fallback_stats = args.fallback_stats.resolve()
    if args.spikes:
        args.spikes = args.spikes.resolve()
    if args.runtime_counters:
        args.runtime_counters = args.runtime_counters.resolve()
    if args.insights_exe:
        args.insights_exe = args.insights_exe.resolve()
    if args.native_evidence:
        args.native_evidence = args.native_evidence.resolve()
    if args.baseline_dir:
        args.baseline_dir = args.baseline_dir.resolve()

    args.out.mkdir(parents=True, exist_ok=True)
    log = AnalyzerLog(args.out / "analysis.log")
    log.write(f"PerfSentinel analyzer started: {ANALYZER_NAME}")

    metadata = load_json_file(args.metadata, log)
    native = load_json_file(args.native_evidence, log) if args.native_evidence else {}
    spikes_path = args.spikes
    if not spikes_path and metadata.get("spike_events_file"):
        spikes_path = Path(str(metadata["spike_events_file"])).resolve()
    fallback_path = args.fallback_stats
    if not fallback_path and metadata.get("fallback_stats_file"):
        fallback_path = Path(str(metadata["fallback_stats_file"])).resolve()
    counters_path = args.runtime_counters
    if not counters_path and metadata.get("runtime_counters_file"):
        counters_path = Path(str(metadata["runtime_counters_file"])).resolve()

    trace_time_offset = float(getattr(args, "trace_time_offset", 0.0) or 0.0)
    if not trace_time_offset and metadata.get("trace_time_offset_seconds") is not None:
        trace_time_offset = float_or_none(metadata.get("trace_time_offset_seconds")) or 0.0

    spikes = load_ndjson(spikes_path, log)
    counters = load_ndjson(counters_path, log)
    csv_dir = args.out / "csv_export"
    timer_csv, threads_csv, insights_log = export_unreal_insights(
        args.insights_exe, args.trace, metadata, spikes, args.out, csv_dir, log, trace_time_offset
    )
    game_stats_path = build_game_stats(args.out, metadata, spikes, log)

    timers = parse_timer_statistics(timer_csv, log)
    threads = parse_threads(threads_csv, log)
    status = "native+exported" if native and timers else ("native" if native else ("exported" if timers else "not_available"))

    fallback_rows = load_ndjson(fallback_path, log)
    severe_frame_budget_ms = float((metadata.get("budgets") or {}).get("severe_frame_budget_ms", max(args.hitch_threshold_ms, args.frame_budget_ms * 2.0)))
    frame_attribution = build_frame_attribution(native, args.hitch_threshold_ms) if native else []
    coverage = build_coverage_manifest(metadata, native)
    metrics = build_performance_metrics(native, fallback_rows + spikes, args.hitch_threshold_ms)
    regression, baseline_path = compare_baseline(args, metrics, log)
    ci_gates = evaluate_ci_gates(args, metrics, regression)
    findings = generate_findings(
        spikes,
        fallback_rows,
        timers,
        args.frame_budget_ms,
        args.hitch_threshold_ms,
        severe_frame_budget_ms,
        counters,
        native,
        frame_attribution,
        coverage,
    )

    evidence_database = args.out / "evidence.sqlite"

    report = build_report(
        args=args,
        metadata=metadata,
        status=status,
        spikes=spikes,
        fallback_rows=fallback_rows,
        timers=timers,
        threads=threads,
        findings=findings,
        counters=counters,
        artifacts={
            "metadata": str(args.metadata),
            "analysis_log": str(args.out / "analysis.log"),
            "unreal_insights_log": str(insights_log),
            "timer_statistics_csv": str(timer_csv),
            "threads_csv": str(threads_csv),
            "timers_csv": str(csv_dir / "Timers.csv"),
            "counters_csv": str(csv_dir / "Counters.csv"),
            "game_stats_json": str(game_stats_path),
            "native_evidence_json": str(args.native_evidence) if args.native_evidence else "",
            "evidence_database": str(evidence_database),
            "baseline": str(baseline_path) if baseline_path else "",
        },
        coverage=coverage,
        metrics=metrics,
        frame_attribution=frame_attribution,
        regression=regression,
        ci_gates=ci_gates,
    )

    findings_json = args.out / "findings.json"
    findings_md = args.out / "findings.md"
    findings_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    findings_md.write_text(render_markdown(report), encoding="utf-8")
    write_evidence_database(evidence_database, native, report)

    if args.update_baseline and baseline_path and (not args.ci or ci_gates.get("passed", True)):
        baseline_payload = {
            "schema_version": SCHEMA_VERSION,
            "updated_at": utc_now_iso(),
            "scenario": metadata.get("scenario", args.baseline_key),
            "trace": str(args.trace),
            "metrics": metrics,
        }
        baseline_path.write_text(json.dumps(baseline_payload, indent=2), encoding="utf-8")
        regression["baseline_updated"] = True
        log.write(f"updated baseline {baseline_path}")
    elif args.update_baseline and args.ci and not ci_gates.get("passed", True):
        regression["baseline_updated"] = False
        log.write("baseline update skipped because CI gates failed")

    # Re-render after database/baseline generation so every referenced artifact is guaranteed to exist.
    findings_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    findings_md.write_text(render_markdown(report), encoding="utf-8")
    log.write(f"wrote {findings_json}")
    log.write(f"wrote {findings_md}")
    if args.ci and not ci_gates.get("passed", True):
        log.write("one or more CI performance gates failed")
        return 10
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
