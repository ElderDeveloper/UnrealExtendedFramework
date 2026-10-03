from __future__ import annotations

import json
import importlib.util
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ANALYZER = Path(__file__).resolve().parents[1] / "perf_sentinel_analyze.py"
MODULE_SPEC = importlib.util.spec_from_file_location("perf_sentinel_analyze", ANALYZER)
assert MODULE_SPEC and MODULE_SPEC.loader
MODULE = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(MODULE)


def coverage_entry(count: int = 1) -> dict[str, object]:
    return {"available": True, "count": count}


def native_evidence(frame_scale: float = 1.0) -> dict[str, object]:
    frames = [
        {"index": 1, "start_seconds": 1.0, "end_seconds": 1.016, "duration_ms": 16.0 * frame_scale},
        {"index": 2, "start_seconds": 2.0, "end_seconds": 2.080, "duration_ms": 80.0 * frame_scale},
        {"index": 3, "start_seconds": 3.0, "end_seconds": 3.020, "duration_ms": 20.0 * frame_scale},
    ]
    return {
        "schema_version": 2,
        "duration_seconds": 60.0,
        "game_frames": frames,
        "render_frames": [],
        "threads": [{"id": 1, "name": "GameThread", "group": "Game"}],
        "timing_events": [
            {
                "frame_index": 2,
                "thread_id": 1,
                "thread": "GameThread",
                "timer_id": 10,
                "timer": "UWorld_Tick",
                "start_seconds": 2.0,
                "end_seconds": 2.070,
                "duration_ms": 70.0,
                "depth": 0,
            },
            {
                "frame_index": 2,
                "thread_id": 1,
                "thread": "GameThread",
                "timer_id": 11,
                "timer": "BP_EnemyManager_C::Tick",
                "start_seconds": 2.010,
                "end_seconds": 2.060,
                "duration_ms": 50.0,
                "depth": 1,
            },
        ],
        "counters": [{"id": 1, "name": "Objects", "group": "Memory", "sample_count": 2, "first": 100, "last": 120, "min": 100, "max": 120, "delta": 20}],
        "bookmarks": [{"time_seconds": 1.5, "text": "BossSpawn", "callstack_id": 0}],
        "tasks": [{"id": "42", "name": "StreamingTask", "queue_delay_ms": 12.0, "execution_ms": 4.0, "prerequisite_count": 1, "nested_count": 0}],
        "file_activity": [{"path": "Content/Paks/Test.pak", "operation_count": 2, "failure_count": 0, "actual_bytes": 4096, "total_ms": 18.0, "max_ms": 15.0}],
        "package_loads": [{"id": 5, "package": "/Game/Boss", "request_id": "99", "serialized_bytes": 4194304, "header_bytes": 4096, "export_count": 12, "export_bytes": 4190208, "main_thread_ms": 18.0, "async_loading_thread_ms": 24.0, "total_ms": 42.0}],
        "export_loads": [{"id": 7, "package": "/Game/Boss", "class": "SkeletalMesh", "event": "Serialize", "serialized_bytes": 2097152, "main_thread_ms": 12.0, "async_loading_thread_ms": 10.0, "total_ms": 22.0}],
        "load_requests": [{"id": "99", "name": "BossArena", "start_seconds": 1.8, "duration_ms": 65.0, "package_count": 3}],
        "logs": [{"time_seconds": 2.0, "category": "LogStreaming", "message": "slow load", "verbosity": 3}],
        "context_switches": [{"thread_id": 1, "thread": "GameThread", "switch_count": 4, "running_ms": 45.0, "scheduled_ratio": 0.75}],
        "stack_frames": [{"timer_id": 100, "address": "0x1234", "module": "PerfSentinelTests", "symbol": "TickBoss", "file": "Boss.cpp", "line": 42}],
        "stack_sample_events": [{"frame_index": 2, "system_thread_id": 1, "thread": "GameThread", "timer_id": 100, "symbol": "TickBoss", "module": "PerfSentinelTests", "start_seconds": 2.02, "end_seconds": 2.021, "duration_ms": 1.0, "depth": 0}],
        "memory_tags": [{"tracker": "Default", "tag": "Textures", "tag_id": "7", "sample_count": 3, "first_bytes": 0, "last_bytes": 67108864, "peak_bytes": 67108864, "growth_bytes": 67108864}],
        "allocation_summary": {"first_bytes": 104857600, "last_bytes": 188743680, "peak_bytes": 188743680, "growth_bytes": 83886080, "has_allocation_events": True},
        "object_snapshots": [
            {"id": 0, "start_seconds": 0.0, "end_seconds": 1.0, "object_count": 10000, "object_array_size": 12000, "reference_count": 20000, "traced_object_count": 10000, "has_total_memory_sizes": True},
            {"id": 1, "start_seconds": 50.0, "end_seconds": 60.0, "object_count": 12000, "object_array_size": 14000, "reference_count": 25000, "traced_object_count": 12000, "has_total_memory_sizes": True},
        ],
        "object_classes": [{"class": "Texture2D", "count": 600, "system_bytes": 104857600, "video_bytes": 209715200, "total_bytes": 314572800}],
        "network_connections": [{"instance": "Server", "server": True, "connection": "Client1", "address": "127.0.0.1", "mode": "outgoing", "packet_count": 10, "total_bytes": 4096, "max_packet_bytes": 900, "dropped_packets": 2}],
        "coverage": {
            "frames": coverage_entry(3),
            "timing": coverage_entry(2),
            "counters": coverage_entry(),
            "bookmarks": coverage_entry(),
            "tasks": coverage_entry(),
            "file_activity": coverage_entry(),
            "load_time": coverage_entry(),
            "logs": coverage_entry(),
            "context_switches": coverage_entry(),
            "stack_samples": coverage_entry(),
            "memory_tags": coverage_entry(),
            "allocations": coverage_entry(),
            "objects": coverage_entry(2),
            "network": coverage_entry(),
            "screenshots": coverage_entry(0),
        },
    }


class PerfSentinelAnalyzerTests(unittest.TestCase):
    def run_analyzer(self, root: Path, native: dict[str, object], *extra: str) -> subprocess.CompletedProcess[str]:
        trace = root / "capture.utrace"
        metadata = root / "capture.metadata.json"
        native_path = root / "native_evidence.json"
        out = root / "report"
        trace.write_bytes(b"synthetic trace placeholder")
        metadata.write_text(
            json.dumps(
                {
                    "project": "PerfSentinelTests",
                    "scenario": "HitchFixture",
                    "channels": ["frame", "cpu", "gpu", "counters", "bookmark", "task", "file", "loadtime", "log", "contextswitch", "stacksampling", "memory", "metadata", "assetmetadata", "net"],
                    "required_launch_arguments": ["-trace=default,memory,net"],
                    "budgets": {"frame_budget_ms": 33.33, "hitch_threshold_ms": 50.0, "severe_frame_budget_ms": 66.66},
                }
            ),
            encoding="utf-8",
        )
        native_path.write_text(json.dumps(native), encoding="utf-8")
        command = [
            sys.executable,
            str(ANALYZER),
            "--trace",
            str(trace),
            "--metadata",
            str(metadata),
            "--native-evidence",
            str(native_path),
            "--out",
            str(out),
            "--frame-budget-ms",
            "33.33",
            "--hitch-threshold-ms",
            "50",
            *extra,
        ]
        return subprocess.run(command, text=True, capture_output=True, check=False)

    def test_native_evidence_report_database_and_baseline(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline = root / "baselines"
            result = self.run_analyzer(root, native_evidence(), "--baseline-dir", str(baseline), "--baseline-key", "Hitch Fixture", "--update-baseline")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            report = json.loads((root / "report" / "findings.json").read_text(encoding="utf-8"))
            self.assertEqual(report["schema_version"], 3)
            self.assertEqual(report["summary"]["status"], "native")
            self.assertEqual(report["summary"]["worst_frame_ms"], 80.0)
            self.assertTrue(report["coverage"]["complete_for_requested_profile"])
            categories = {finding["category"] for finding in report["findings"]}
            self.assertTrue({"frame_scope_attribution", "task_queue_delay", "stack_sampling", "file_io", "asset_loading", "load_request_latency", "memory_tag_growth", "object_snapshot_growth", "network_packet_loss"}.issubset(categories))
            self.assertEqual(report["frame_attribution"][0]["dominant_thread"], "GameThread")
            self.assertAlmostEqual(report["frame_attribution"][0]["top_contributors"][0]["self_ms"], 50.0)
            self.assertTrue((baseline / "Hitch_Fixture.json").exists())
            self.assertIn("PerfSentinel analyzer started", (root / "report" / "analysis.log").read_text(encoding="utf-8"))

            database = sqlite3.connect(root / "report" / "evidence.sqlite")
            try:
                self.assertEqual(database.execute("SELECT COUNT(*) FROM game_frames").fetchone()[0], 3)
                self.assertGreaterEqual(database.execute("SELECT COUNT(*) FROM findings").fetchone()[0], 5)
                self.assertEqual(database.execute("SELECT dropped_packets FROM network_connections").fetchone()[0], 2)
                self.assertEqual(database.execute("SELECT package FROM package_loads").fetchone()[0], "/Game/Boss")
                self.assertEqual(database.execute("SELECT class_name FROM object_classes").fetchone()[0], "Texture2D")
            finally:
                database.close()

    def test_ci_returns_nonzero_for_regression_and_budget_failures(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline = root / "baselines"
            seed = self.run_analyzer(root, native_evidence(0.5), "--baseline-dir", str(baseline), "--baseline-key", "ci", "--update-baseline")
            self.assertEqual(seed.returncode, 0, seed.stdout + seed.stderr)

            result = self.run_analyzer(
                root,
                native_evidence(2.0),
                "--baseline-dir",
                str(baseline),
                "--baseline-key",
                "ci",
                "--ci",
                "--max-p99-regression-percent",
                "5",
                "--max-hitches-per-minute",
                "0.5",
                "--max-memory-growth-mb",
                "10",
            )
            self.assertEqual(result.returncode, 10, result.stdout + result.stderr)
            report = json.loads((root / "report" / "findings.json").read_text(encoding="utf-8"))
            self.assertEqual(report["regression"]["status"], "compared")
            self.assertFalse(report["ci"]["passed"])
            failed = {gate["name"] for gate in report["ci"]["gates"] if not gate["passed"]}
            self.assertIn("p99_regression_percent", failed)
            self.assertIn("hitches_per_minute", failed)
            self.assertIn("memory_growth_mb", failed)

    def test_network_costs_preserve_direction_unknown_status_and_runtime_latency(self) -> None:
        native = native_evidence()
        native["network_connections"] = [{"identity": "server/client/outgoing", "mode": "outgoing", "packet_count": 10,
                                           "total_bytes": 4096, "delivered_packets": 6, "dropped_packets": 2, "unknown_packets": 2,
                                           "capture_duration_seconds": 60.0}]
        native["network_time_bins"] = [{"identity": "server/client/outgoing", "mode": "outgoing", "bytes_per_second": 10240}]
        native["network_content_costs"] = [{"name": "SomeSerializedScope", "mode": "outgoing", "event_count": 4,
                                             "inclusive_bits": 800, "exclusive_bits": 200}]
        samples = [{"capture_elapsed_seconds": 1.0, "worlds": [{"world_id": "W", "net_mode": "Client", "map_name": "Lobby",
                    "drivers": [{"driver_id": "D", "connections": [{"connection_id": 1, "state": "open", "counter_status": "valid",
                    "sample_interval_seconds": 1.0, "out_packets_notified_delta": 100, "out_packets_lost_delta": 3,
                    "out_bytes_per_second": 20480, "avg_rtt_ms": 450, "out_loss_percent": 3.0, "reliable_outstanding_bunches": 12}]}]}]}]
        network = MODULE.build_network_report(native, samples, {"network_budgets": {"max_rtt_ms": 300}})
        self.assertEqual(network["metrics"]["peak_outgoing_kib_per_second"], 20.0)
        self.assertEqual(network["metrics"]["peak_rtt_ms"], 450)
        self.assertEqual(network["connections"][0]["trace_reported_drop_percent"], 25.0)
        self.assertEqual(network["unknown_packets"], 2)
        self.assertEqual(network["top_content_costs"][0]["exclusive_bits"], 200)
        self.assertIn("without confirming an ACK", " ".join(network["limitations"]))
        builder = MODULE.FindingBuilder()
        MODULE.add_network_findings(builder, network)
        self.assertTrue(any(row["category"] == "network_budget" for row in builder.findings))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime_rows = MODULE.flatten_runtime_network(samples)
            paths = MODULE.write_network_csv(root, native, runtime_rows)
            self.assertTrue(Path(paths["runtime_network_samples"]).exists())
            MODULE.write_evidence_database(root / "evidence.sqlite", {**native, "runtime_network_samples": runtime_rows}, {"network": network})
            database = sqlite3.connect(root / "evidence.sqlite")
            try:
                self.assertEqual(database.execute("SELECT json_extract(row_json,'$.avg_rtt_ms') FROM runtime_network_samples").fetchone()[0], 450)
                self.assertEqual(database.execute("SELECT json_extract(row_json,'$.exclusive_bits') FROM network_content_costs").fetchone()[0], 200)
            finally:
                database.close()

    def test_missing_metrics_and_empty_requested_providers_are_unavailable(self) -> None:
        coverage = MODULE.build_coverage_manifest({"channels": ["frame", "net"]}, {"coverage": {"frames": coverage_entry(0), "network": coverage_entry(0)}})
        self.assertFalse(coverage["complete_for_requested_profile"])
        self.assertEqual(coverage["providers"]["network"]["status"], "empty")
        metrics = MODULE.build_performance_metrics({}, [], 50)
        self.assertFalse(metrics["frame_metrics_available"])
        self.assertIsNone(metrics["memory_growth_mb"])
        args = MODULE.argparse.Namespace(ci=True, max_p99_regression_percent=15, max_hitches_per_minute=3, max_memory_growth_mb=200)
        ci = MODULE.evaluate_ci_gates(args, metrics, {"status": "compared"}, {"budgets": {"max_rtt_ms": 400}, "metrics": {"peak_rtt_ms": None}})
        self.assertFalse(ci["passed"])
        self.assertTrue(any(gate["name"] == "peak_rtt_ms" and gate["status"] == "unavailable" and not gate["passed"] for gate in ci["gates"]))

    def test_timing_attribution_clips_scope_to_frame(self) -> None:
        native = native_evidence()
        native["timing_events"] = [{"frame_index": 2, "thread": "Worker", "timer": "LongBackgroundScope", "start_seconds": 0,
                                    "end_seconds": 10, "depth": 0, "duration_ms": 10000}]
        attribution = MODULE.build_frame_attribution(native, 50)
        self.assertAlmostEqual(attribution[0]["top_contributors"][0]["inclusive_ms"], 80.0)
        self.assertEqual(attribution[0]["dominant_thread"], "Worker")
        self.assertIn("does not prove", attribution[0]["attribution_limit"])

    def test_spikes_do_not_bias_regular_frame_percentiles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fallback = root / "fallback.ndjson"
            spikes = root / "spikes.ndjson"
            fallback.write_text(json.dumps({"frame_time_ms": 10.0}) + "\n", encoding="utf-8")
            spikes.write_text(json.dumps({"frame_time_ms": 500.0}) + "\n", encoding="utf-8")
            result = self.run_analyzer(root, {}, "--fallback-stats", str(fallback), "--spikes", str(spikes))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((root / "report" / "findings.json").read_text(encoding="utf-8"))
            self.assertEqual(report["metrics"]["sample_count"], 1)
            self.assertEqual(report["metrics"]["frame_ms"]["p99"], 10.0)
            self.assertEqual(report["summary"]["worst_frame_ms"], 500.0)

    def test_baseline_rejects_different_capture_context(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            args = MODULE.argparse.Namespace(baseline_dir=Path(temporary), baseline_key="run")
            (Path(temporary) / "run.json").write_text(json.dumps({"metrics": {"capture_context": {"roles": ["server"]}}}), encoding="utf-8")
            comparison, _ = MODULE.compare_baseline(args, {"capture_context": {"roles": ["client"]}}, MODULE.AnalyzerLog())
            self.assertEqual(comparison["status"], "incompatible_context")


if __name__ == "__main__":
    unittest.main()
