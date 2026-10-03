"""Versioned, stdlib-only Perf Sentinel evidence and agent report contract.

This module consumes already collected observations. It never launches Unreal,
reads assets, guesses source ownership from scope names, or runs diagnostic tests.
"""
from __future__ import annotations

import hashlib
import json
import math
import os
import sqlite3
import uuid
from collections import defaultdict
from pathlib import Path
from typing import Any

CONTRACT_VERSION = "4.0.0"
RULESET_VERSION = "1.0.0"
STATUSES = {"measured", "unavailable", "unsupported", "disabled", "truncated"}

# Evidence levels describe what the observations establish, not probabilities.
RULES = {
    "frame_hitch": ("frame", "measured", "spike >= configured hitch threshold"),
    "frame_budget": ("frame", "measured", "regular over-budget samples >= max(5,floor(20% of population))"),
    "frame_scope_attribution": ("cpu", "attributed", "retained named scopes overlap an observed hitch"),
    "game_thread": ("cpu", "measured", "observed game-thread sample >= configured hitch threshold"),
    "render_thread": ("cpu", "measured", "observed render-thread sample >= configured hitch threshold"),
    "rhi_thread": ("cpu", "measured", "observed RHI-thread sample >= configured hitch threshold"),
    "gpu": ("gpu", "measured", "observed GPU sample >= configured hitch threshold"),
    "timer_scope": ("timing", "attributed", "named timer maximum exceeds configured hitch threshold; exported timer kind is unspecified"),
    "task_queue_delay": ("tasks", "measured", "scheduled-to-start duration >= 5ms"),
    "stack_sampling": ("cpu", "correlated", "symbol sampled in retained hitch windows"),
    "file_io": ("loading", "measured", "file operation duration >= 5ms"),
    "asset_loading": ("loading", "attributed", "named package aggregated thread work >= 10ms"),
    "load_request_latency": ("loading", "measured", "load-request duration >= 20ms"),
    "memory_tag_growth": ("memory", "attributed", "observed tag growth >= 50MiB"),
    "memory_leak_suspected": ("memory", "hypothesis", "physical memory growth >100MiB and15%; final >=95% of peak"),
    "object_growth": ("objects", "measured", "object count growth >2000 and5%"),
    "object_snapshot_growth": ("objects", "measured", "snapshot growth >= max(1000,5%)"),
    "streaming_unload_not_reclaimed": ("memory", "correlated", "actor decline >=max(20,30%) with physical memory >=95% of prior sample"),
    "actor_churn": ("objects", "measured", "actor decline >=max(20,30%)"),
    "network_packet_loss": ("network", "measured", "trace contains Dropped statuses; not ACK-delivery proof"),
    "network_budget": ("network", "measured", "observed metric exceeds a positive configured budget"),
    "network_coverage": ("network", "measured", "requested observation or positive budget has no matching measurement"),
    "trace_coverage": ("coverage", "measured", "requested providers have no observed data"),
    "trace_launch_requirements": ("coverage", "measured", "capture launch arguments differ from declared requirements"),
    "causal_failure": ("gameplay", "measured", "instrumented operation explicitly recorded a failed stage"),
    "causal_incomplete": ("gameplay", "hypothesis", "request observed with no terminal stage before capture ended"),
    "transport_failure": ("transport", "measured", "instrumented transport explicitly reported failure"),
    "summary": ("coverage", "measured", "no implemented rule crossed its declared threshold; coverage still applies"),
}

TABLE_DOMAINS = {
    "game_frames": ("frame", "native.frames"), "render_frames": ("frame", "native.frames"),
    "timing_events": ("cpu", "native.timing"), "timing_scope_totals": ("cpu", "native.timing_aggregate"),
    "counters": ("counters", "native.counters"), "bookmarks": ("gameplay", "native.bookmarks"),
    "tasks": ("tasks", "native.tasks"), "stack_frames": ("cpu", "native.stack_samples"),
    "stack_sample_events": ("cpu", "native.stack_samples"), "context_switches": ("cpu", "native.context_switches"),
    "file_activity": ("loading", "native.file_activity"), "package_loads": ("loading", "native.load_time"),
    "export_loads": ("loading", "native.load_time"), "load_requests": ("loading", "native.load_time"),
    "memory_tags": ("memory", "native.memory_tags"), "allocation_summary": ("memory", "native.allocations"),
    "object_snapshots": ("objects", "native.objects"), "object_classes": ("objects", "native.objects"),
    "logs": ("logs", "native.logs"), "network_connections": ("network", "native.network"),
    "network_time_bins": ("network", "native.network"), "network_content_costs": ("network", "native.network_content"),
    "network_packet_samples": ("network", "native.network"), "network_hitch_activity": ("network", "native.network"),
    "network_summary": ("network", "native.network"), "coverage": ("coverage", "native.coverage"),
    "runtime_frame_samples": ("frame", "runtime.frames"), "runtime_counters": ("objects", "runtime.counters"),
    "regular_fallback_samples": ("frame", "runtime.fallback"),
    "runtime_network_samples": ("network", "runtime.network"), "runtime_network_drivers": ("network", "runtime.network"),
    "spikes": ("frame", "runtime.spikes"), "telemetry_events": ("gameplay", "runtime.telemetry"),
    "metadata": ("coverage", "capture.metadata"), "runtime_context": ("coverage", "capture.context"),
    "report_coverage": ("coverage", "analysis.coverage"), "report_metrics": ("frame", "analysis.metrics"),
    "network_metrics": ("network", "analysis.network"), "evidence_table_summaries": ("coverage", "analysis.evidence"),
    "runtime_collector_catalog": ("coverage", "runtime.collector_catalog"),
    "network_runtime_connection_summaries": ("network", "runtime.network"),
    "runtime_metric_measurements": ("runtime", "runtime.instrumented_metrics"),
    "timer_statistics": ("timing", "insights.timer_statistics"), "thread_summary": ("coverage", "insights.threads"),
}


def number(value: Any) -> float | None:
    if isinstance(value, bool) or value is None:
        return None
    try:
        parsed = float(value)
        return parsed if math.isfinite(parsed) else None
    except (ValueError, TypeError, OverflowError):
        return None


def safe_json(value: Any) -> Any:
    if isinstance(value, dict):
        return {str(key): safe_json(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [safe_json(item) for item in value]
    if isinstance(value, float) and not math.isfinite(value):
        return None
    return value


def atomic_write_json(path: Path, value: Any) -> None:
    """Replace one complete JSON document; readers never see a partial write."""
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as stream:
            json.dump(safe_json(value), stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def digest(value: Any) -> str:
    return hashlib.sha256(json.dumps(safe_json(value), sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")).hexdigest()[:32]


def status(value: Any, default: str = "unavailable") -> str:
    aliases = {"available": "measured", "observed": "measured", "observed_budget_overrun": "measured", "empty": "unavailable", "missing": "unavailable",
               "not_requested": "disabled", "disabled_by_profile": "disabled", "disabled_by_settings": "disabled", "not_applicable": "unsupported",
               "deferred_by_budget": "truncated", "suspended_after_budget_overruns": "truncated"}
    result = aliases.get(str(value), str(value))
    return result if result in STATUSES else default


def comparison_readiness(capture_context: dict[str, Any]) -> dict[str, Any]:
    required = ("map_name", "cpu_brand", "platform", "capture_profile", "process_role", "scenario",
                "build_configuration", "build_target", "engine_build_version")
    unknown = [key for key in required if capture_context.get(key) is None or
               (isinstance(capture_context.get(key), str) and capture_context[key].strip().lower() in {"", "unknown", "unavailable", "none"})]
    if capture_context.get("peak_connection_workload") is None and capture_context.get("trace_connection_workload") is None:
        unknown.append("observed_connection_workload")
    gpu_required = ("gpu_brand", "rhi", "render_resolution")
    gpu_unknown = [key for key in gpu_required if not capture_context.get(key) or str(capture_context.get(key)).strip().lower() in {"unknown", "unavailable", "none"}]
    return {"ready": not unknown, "required_fields": [*required, "observed_connection_workload"], "unknown_required_fields": unknown,
            "domain_requirements": {"gpu": {"ready": not unknown and not gpu_unknown, "required_fields": list(gpu_required), "unknown_required_fields": gpu_unknown}},
            "policy": "Matching unknown values do not establish capture parity. Build revision may differ; configuration, workload, hardware, role and coverage must match."}


def context_for(metadata: dict[str, Any], report: dict[str, Any]) -> dict[str, Any]:
    session = str(metadata.get("session_id") or ("legacy-" + digest({key: metadata.get(key) for key in ("trace_file", "started_at", "stopped_at", "project", "scenario")} | {"trace": report.get("trace", {}).get("path")})))
    offset = number(metadata.get("clock_offset_ms"))
    uncertainty = number(metadata.get("clock_uncertainty_ms"))
    synchronized = offset is not None and uncertainty is not None and uncertainty >= 0
    return {
        "session_id": session, "run_id": str(metadata.get("run_id") or session),
        "process_role": str(metadata.get("process_role") or "unknown"), "process_id": metadata.get("process_id"),
        "build_id": str(metadata.get("build_id") or "unknown"), "session_id_source": "capture_guid" if metadata.get("session_id") else "legacy_metadata_hash",
        "build_configuration": metadata.get("build_configuration"), "build_target": metadata.get("build_target"),
        "engine_build_version": metadata.get("engine_build_version", metadata.get("engine_version")), "platform": metadata.get("platform"),
        "clock": {"timebase": "process_platform_seconds", "offset_seconds": offset / 1000.0 if offset is not None else None,
                  "uncertainty_seconds": uncertainty / 1000.0 if uncertainty is not None else None, "synchronized": synchronized,
                  "local_clock_utc": metadata.get("local_clock_utc"), "local_clock_monotonic_seconds": number(metadata.get("local_clock_monotonic_seconds")),
                  "cross_process_duration_policy": "require explicit synchronized anchors with uncertainty; run/occurrence IDs establish only a logical chain"},
    }


def row_window(table: str, row: dict[str, Any]) -> tuple[float | None, float | None, str]:
    if table.startswith("runtime_") or table in {"spikes", "telemetry_events"}:
        if number(row.get("capture_elapsed_seconds")) is not None:
            start = number(row.get("capture_elapsed_seconds"))
            return start, start, "capture_elapsed_seconds"
        start = number(row.get("platform_seconds"))
        return start, start, "process_platform_seconds" if start is not None else "unknown"
    start = number(row.get("start_seconds", row.get("time_seconds", row.get("first_packet_seconds"))))
    end = number(row.get("end_seconds", row.get("last_packet_seconds", start)))
    return start, end, "trace_seconds" if start is not None else "unknown"


class EvidenceStore:
    def __init__(self, context: dict[str, Any]) -> None:
        self.context = context
        self.rows: list[dict[str, Any]] = []
        self.by_table: dict[str, list[dict[str, Any]]] = defaultdict(list)
        self.summaries: dict[str, str] = {}
        self.duplicates: dict[str, int] = defaultdict(int)

    def add(self, table: str, row: dict[str, Any], row_index: int = 0) -> str:
        row = safe_json(row)
        base = f"ev:{self.context['session_id']}:{table}:{digest(row)}"
        duplicate = self.duplicates[base]
        self.duplicates[base] += 1
        evidence_id = base if duplicate == 0 else f"{base}:duplicate{duplicate}"
        domain, collector = TABLE_DOMAINS.get(table, ("unknown", "unknown"))
        if table == "timing_scope_totals" and row.get("kind") == "gpu":
            domain = "gpu"
        if table == "telemetry_events":
            domain = "transport" if row.get("event_type") == "transport_operation" else ("coverage" if row.get("event_type") in {"collector_snapshot", "coverage", "clock_alignment"} else "gameplay")
            collector = str(row.get("source") or "runtime.telemetry")
        start, end, timebase = row_window(table, row)
        source_file = row.get("source_file", row.get("file"))
        # An explicit package field is asset evidence. Timer/class names are not.
        asset = row.get("asset_path", row.get("package"))
        instance = row.get("instance_id", row.get("object_instance_id", row.get("net_object_id", row.get("object_id", row.get("object_path")))))
        if table == "runtime_metric_measurements":
            domain, collector = str(row.get("domain") or "runtime"), str(row.get("source") or "runtime.instrumented_metrics")
        item = {
            "evidence_id": evidence_id, "session_id": self.context["session_id"], "domain": domain,
            "collector_id": collector, "table_name": table, "row_index": row_index,
            "start_seconds": start, "end_seconds": end, "timebase": timebase,
            "source_file": str(source_file) if source_file else None, "source_line": row.get("line"),
            "asset_path": str(asset) if asset else None, "instance_id": str(instance) if instance is not None else None,
            "occurrence_id": row.get("occurrence_id"), "run_id": row.get("run_id") if table in {"telemetry_events", "runtime_metric_measurements"} else str(row.get("run_id") or self.context["run_id"]),
            "stage": row.get("stage"), "process_role": str(row.get("process_role") or ("unknown" if table in {"telemetry_events", "runtime_metric_measurements"} else self.context["process_role"])),
            "process_id": row.get("process_id", self.context["process_id"]), "row": row,
        }
        self.rows.append(item)
        self.by_table[table].append(item)
        return evidence_id

    def populate(self, native: dict[str, Any], extras: dict[str, Any]) -> None:
        for table, value in {**{key: native.get(key) for key in TABLE_DOMAINS if key in native}, **extras}.items():
            if isinstance(value, dict) and value:
                self.add(table, value)
            elif isinstance(value, list):
                for index, row in enumerate(value):
                    if isinstance(row, dict):
                        self.add(table, row, index)
        for event in extras.get("telemetry_events", []):
            if not isinstance(event, dict) or not isinstance(event.get("metrics"), list):
                continue
            for spec in event["metrics"]:
                if not isinstance(spec, dict):
                    continue
                # These are explicit instrumentation rows, not inferred scope ownership.
                row = {**{key: event.get(key) for key in ("event_id", "occurrence_id", "run_id", "process_id", "process_role", "timestamp", "platform_seconds", "capture_elapsed_seconds", "timebase")}, **spec}
                self.add("runtime_metric_measurements", row, len(self.by_table["runtime_metric_measurements"]))
        for table, rows in list(self.by_table.items()):
            start_values = [item["start_seconds"] for item in rows if item["start_seconds"] is not None]
            end_values = [item["end_seconds"] for item in rows if item["end_seconds"] is not None]
            summary = {"table_name": table, "row_count": len(rows), "domain": rows[0]["domain"], "collector_id": rows[0]["collector_id"],
                       "start_seconds": min(start_values) if start_values else None, "end_seconds": max(end_values) if end_values else None,
                       "timebases": sorted({item["timebase"] for item in rows}),
                       "population_resolver": {"table": "evidence_items", "filters": {"session_id": self.context["session_id"], "table_name": table}}}
            self.summaries[table] = self.add("evidence_table_summaries", summary)

    def refs(self, *tables: str) -> list[str]:
        return [self.summaries[table] for table in tables if table in self.summaries]

    def window(self, *tables: str) -> dict[str, Any]:
        rows = [item for table in tables for item in self.by_table.get(table, []) if item["start_seconds"] is not None]
        timebases = {item["timebase"] for item in rows}
        if len(timebases) != 1:
            return {"start_seconds": None, "end_seconds": None, "timebase": "unknown", "population_tables": list(tables)}
        return {"start_seconds": min(item["start_seconds"] for item in rows),
                "end_seconds": max(item["end_seconds"] if item["end_seconds"] is not None else item["start_seconds"] for item in rows),
                "timebase": next(iter(timebases)), "population_tables": list(tables)}

    def resolve_legacy(self, selector: str) -> list[str]:
        table, _, key = selector.partition(":")
        if table == "coverage":
            return self.refs("report_coverage", "coverage")
        if table == "metadata":
            return self.refs("metadata")
        if table == "network.metrics":
            return self.refs("network_metrics")
        rows = self.by_table.get(table, [])
        selected: list[str] = []
        for item in rows:
            row = item["row"]
            matches = not key or (key.startswith("frame=") and str(row.get("frame_index", row.get("index", row.get("frame_number")))) == key[6:])
            matches |= key == str(row.get("id", "")) or key == str(row.get("index", "")) or key == str(row.get("path", ""))
            matches |= key == f"{row.get('module')}!{row.get('symbol')}" or key == f"{row.get('tracker')}:{row.get('tag_id')}"
            matches |= key == "dropped_packets>0" and (number(row.get("dropped_packets")) or 0) > 0
            matches |= "->" in key and str(row.get("id")) in key.split("->")
            if matches:
                selected.append(item["evidence_id"])
        return sorted(selected)[:20] or self.refs(table)


def collector_catalog(report: dict[str, Any], native: dict[str, Any], telemetry: list[dict[str, Any]], store: EvidenceStore,
                      declared_catalog: dict[str, Any] | None = None) -> list[dict[str, Any]]:
    catalog: dict[str, dict[str, Any]] = {}
    native_coverage = native.get("coverage") if isinstance(native.get("coverage"), dict) else {}
    providers = report.get("coverage", {}).get("providers", {})
    for provider in sorted(set(native_coverage) | set(providers)):
        raw = native_coverage.get(provider, {})
        observed = providers.get(provider, {})
        raw = raw if isinstance(raw, dict) else {}
        state = status(observed.get("status"), "measured" if raw.get("available") and raw.get("count") != 0 else "unavailable")
        count = raw.get("count", observed.get("count"))
        identifier = f"native.{provider}"
        if provider.startswith("network") and report.get("coverage", {}).get("network_trace_compiled") is False:
            state = "unsupported"
        catalog[identifier] = {"collector_id": identifier, "domain": TABLE_DOMAINS.get(provider, (provider, ""))[0], "status": state, "observed_count": count,
                               "requested_channels": observed.get("requested_channels", []), "source": "TraceServices provider observations",
                               "reason": "registered provider presence alone does not establish captured events", "evidence_ids": store.refs("coverage", "report_coverage")}
    for table in ("runtime_frame_samples", "regular_fallback_samples", "runtime_counters", "runtime_network_samples", "telemetry_events", "runtime_metric_measurements"):
        identifier = TABLE_DOMAINS[table][1]
        rows = store.by_table.get(table, [])
        catalog[identifier] = {"collector_id": identifier, "domain": TABLE_DOMAINS[table][0], "status": "measured" if rows else "unavailable",
                               "observed_count": len(rows), "source": "runtime sidecar", "evidence_ids": store.refs(table)}
    declared = (declared_catalog or {}).get("collectors", [])
    for entry in declared if isinstance(declared, list) else []:
        if isinstance(entry, dict) and entry.get("collector_id"):
            identifier = str(entry["collector_id"])
            catalog[identifier] = {**entry, "collector_id": identifier, "status": status(entry.get("status")),
                                   "observed_count": 0, "evidence_ids": store.refs("runtime_collector_catalog")}
    for event in telemetry:
        for entry in event.get("collectors", []) if isinstance(event.get("collectors"), list) else []:
            if not isinstance(entry, dict):
                continue
            identifier = str(entry.get("collector_id") or "unknown")
            row = catalog.setdefault(identifier, {"collector_id": identifier, "observed_count": 0, "source": "runtime declared collector"})
            partial = isinstance(entry.get("metrics"), dict) and entry["metrics"].get("partial") is True
            observed_status = "truncated" if entry.get("truncated") or partial else status(entry.get("status"))
            # Preserve a known capture omission after subsequent healthy snapshots.
            if row.get("status") == "truncated" and observed_status == "measured":
                observed_status = "truncated"
            if entry.get("status") == "not_due" and row.get("status") in {"measured", "truncated"}:
                observed_status = row["status"]
            row.update({"status": observed_status, "cadence_seconds": number(entry.get("cadence_seconds")), "reason": entry.get("reason"),
                        "last_snapshot_status": entry.get("status"), "source": entry.get("source", row.get("source"))})
            if entry.get("metrics"):
                row["last_observed_overhead_ms"] = number(entry.get("elapsed_ms"))
            row["snapshot_count"] = row.get("snapshot_count", 0) + 1
            row["observed_count"] += sum(isinstance(value, dict) and number(value.get("value")) is not None for value in (entry.get("metrics") or {}).values()) if isinstance(entry.get("metrics"), dict) else 0
    # Limits are explicit coverage, not a healthy zero beyond the retained population.
    for provider, limit in (native.get("network_summary") or {}).items():
        if isinstance(limit, dict) and limit.get("truncated"):
            identifier = f"native.network.{provider}"
            catalog[identifier] = {"collector_id": identifier, "status": "truncated", "limits": limit, "source": "native extraction cap"}
    if native.get("timing_events_truncated") or native.get("hitch_windows_truncated"):
        catalog.setdefault("native.timing", {"collector_id": "native.timing"})["status"] = "truncated"
    if native.get("timing_scope_summary", {}).get("cpu_truncated") or native.get("timing_scope_summary", {}).get("gpu_truncated"):
        catalog.setdefault("native.timing_aggregate", {"collector_id": "native.timing_aggregate"})["status"] = "truncated"
    if any(item["row"].get("truncated") or item["row"].get("channels_truncated") for item in store.by_table.get("runtime_network_samples", [])):
        catalog["runtime.network"]["status"] = "truncated"
    for event in telemetry:
        if event.get("event_type") == "coverage" and (number(event.get("dropped_or_expired_rows")) or 0) > 0:
            catalog["runtime.telemetry"].update({"status": "truncated", "reason": "bounded telemetry history dropped or expired records; this is not packet loss"})
    return sorted(catalog.values(), key=lambda row: row["collector_id"])


class MetricBuilder:
    def __init__(self, context: dict[str, Any], store: EvidenceStore) -> None:
        self.context, self.store = context, store
        self.records: list[dict[str, Any]] = []

    def add(self, metric_id: str, domain: str, value: Any, unit: str, aggregation: str, tables: tuple[str, ...],
            *, collector: str | None = None, state: str | None = None, scope: dict[str, Any] | None = None,
            window: dict[str, Any] | None = None, evidence_ids: list[str] | None = None, notes: dict[str, Any] | None = None) -> None:
        value = number(value)
        state = status(state, "measured" if value is not None else "unavailable")
        if state == "measured" and value is None:
            state = "unavailable"
        if state in {"unavailable", "unsupported", "disabled"}:
            value = None
        scope = dict(scope) if scope else {"level": "session", "identity": self.context["session_id"], "role": self.context["process_role"]}
        scope.setdefault("role", self.context["process_role"])
        scope.setdefault("level", "unknown")
        scope.setdefault("identity", "unknown")
        if scope["level"] in {"session", "process", "run", "global"}:
            scope.setdefault("comparison_key", f"{scope['level']}:{scope['role']}")
        evidence_ids = evidence_ids if evidence_ids is not None else self.store.refs(*tables)
        window = window or self.store.window(*tables)
        collectors = sorted({TABLE_DOMAINS.get(table, ("", "unknown"))[1] for table in tables if self.store.by_table.get(table)})
        source_collector = collector or (collectors[0] if len(collectors) == 1 else (f"analysis.{domain}" if collectors else "unknown"))
        identity = f"metric:{self.context['session_id']}:{metric_id}:{digest({'scope': scope, 'window': window, 'unit': unit, 'aggregation': aggregation})}"
        self.records.append({"record_id": identity, "session_id": self.context["session_id"], "metric_id": metric_id, "domain": domain, "value": value, "unit": unit,
                             "aggregation": aggregation, "window": window, "scope": scope, "status": state,
                             "measurement_notes": notes or {},
                             "source": {"collector_id": source_collector, "collector_ids": collectors,
                                        "evidence_ids": evidence_ids}})


def metric_records(report: dict[str, Any], native: dict[str, Any], store: EvidenceStore, context: dict[str, Any], telemetry: list[dict[str, Any]]) -> list[dict[str, Any]]:
    builder = MetricBuilder(context, store)
    metrics = report.get("metrics", {})
    frame_tables = ("game_frames",) if metrics.get("frame_source") == "native_frames" else (("regular_fallback_samples",) if metrics.get("frame_source") == "regular_fallback_samples" else ("runtime_frame_samples",))
    frame_state = "measured" if metrics.get("frame_metrics_available") else "unavailable"
    for aggregate, value in (metrics.get("frame_ms") or {}).items():
        builder.add(f"frame.duration.{aggregate}", "frame", value, "ms", aggregate, frame_tables, state=frame_state)
    builder.add("frame.hitch.count", "frame", metrics.get("hitch_count"), "count", "count", frame_tables, state=frame_state)
    builder.add("frame.hitch.rate", "frame", metrics.get("hitches_per_minute"), "count/minute", "rate", frame_tables, state=frame_state)
    builder.add("memory.growth", "memory", metrics.get("memory_growth_mb"), "MiB", "end_minus_start",
                ("allocation_summary",) if metrics.get("memory_source") == "allocation_trace" else ("runtime_counters",))
    unit_map = {"peak_outgoing_kib_per_second": "KiB/s", "peak_incoming_kib_per_second": "KiB/s", "peak_rtt_ms": "ms", "peak_loss_percent": "%", "peak_reliable_backlog": "bunches"}
    for key, value in report.get("network", {}).get("metrics", {}).items():
        tables = ("runtime_network_samples",) if key in {"peak_rtt_ms", "peak_reliable_backlog"} else ("network_time_bins", "runtime_network_samples")
        if key == "peak_loss_percent":
            tables += ("network_connections",)
        builder.add(f"network.{key}", "network", value, unit_map.get(key, "unknown"), "connection_peak", tables,
                    notes={"semantics": "largest retained connection/bin observation; trace status share and ACK/NAK interval shares remain distinct evidence"} if key == "peak_loss_percent" else None)
    for item in store.by_table.get("network_runtime_connection_summaries", []):
        row = item["row"]
        scope = {"level": "connection", "identity": str(row.get("identity")), "role": str(row.get("net_mode") or context["process_role"]),
                 "comparison_policy": "capture-local identity; requires an explicit stable peer mapping to compare"}
        for field, unit in (("avg_rtt_ms", "ms"), ("jitter_ms", "ms"), ("in_bytes_per_second", "bytes/s"), ("out_bytes_per_second", "bytes/s"),
                            ("in_loss_percent", "%"), ("out_loss_percent", "%"), ("reliable_outstanding_bunches", "bunches"), ("queued_bits", "bits")):
            distribution = row.get(field) if isinstance(row.get(field), dict) else {}
            for aggregate in ("mean", "p95", "max"):
                builder.add(f"network.connection.{field}", "network", distribution.get(aggregate), unit, aggregate,
                            ("runtime_network_samples",), scope=scope, evidence_ids=[item["evidence_id"]])
        builder.add("network.connection.outgoing_nak_share", "network", row.get("out_notified_loss_percent"), "%", "sum_naks_over_sum_ack_nak_notifications",
                    ("runtime_network_samples",), scope=scope, evidence_ids=[item["evidence_id"]])
    for item in store.by_table.get("timing_scope_totals", []):
        row = item["row"]
        domain = "gpu" if row.get("kind") == "gpu" else "cpu"
        scope = {"level": "scope", "identity": f"{domain}:timer:{row.get('timer_id')}", "role": context["process_role"],
                 "name": row.get("timer"), "source_file": item["source_file"], "source_line": item["source_line"],
                 "attribution": "instrumented named scope; no inferred UObject/asset owner"}
        scope["comparison_key"] = f"{domain}:timer:{row.get('timer')}:{item['source_file']}:{item['source_line']}:{context['process_role']}"
        truncated = native.get("timing_scope_summary", {}).get(f"{domain}_truncated")
        aggregate_window = native.get("timing_scope_summary", {})
        scope_window = {"start_seconds": number(aggregate_window.get("start_seconds")), "end_seconds": number(aggregate_window.get("end_seconds")),
                        "timebase": "trace_seconds" if number(aggregate_window.get("end_seconds")) is not None else "unknown"}
        for field, unit, aggregate in (("exclusive_ms", "ms", "sum_exclusive"), ("inclusive_ms", "ms", "sum_inclusive_nested"), ("instance_count", "count", "count")):
            builder.add(f"{domain}.scope.{field}", domain, row.get(field), unit, aggregate, ("timing_scope_totals",),
                        scope=scope, window=scope_window, state="truncated" if truncated else None, evidence_ids=[item["evidence_id"]])
    for table, field_specs in {
        "package_loads": (("total_ms", "ms", "sum_parallel_thread_work"), ("serialized_bytes", "bytes", "sum")),
        "object_classes": (("count", "count", "gauge_final_snapshot"), ("total_bytes", "bytes", "sum_final_snapshot")),
        "network_content_costs": (("exclusive_bits", "bits", "sum_exclusive"), ("inclusive_bits", "bits", "sum_inclusive_nested")),
    }.items():
        for item in store.by_table.get(table, []):
            row = item["row"]
            domain = TABLE_DOMAINS[table][0]
            scope = {"level": "asset" if item["asset_path"] else ("class" if table == "object_classes" else "scope"),
                     "identity": str(row.get("id", row.get("class", f"{row.get('identity')}:type:{row.get('event_type_index')}"))),
                     "role": context["process_role"], "asset_path": item["asset_path"], "instance_id": item["instance_id"]}
            if item["asset_path"] or table == "object_classes":
                scope["comparison_key"] = f"{scope['level']}:{item['asset_path'] or row.get('class')}:{context['process_role']}"
            for field, unit, aggregate in field_specs:
                measurement_status = None
                snapshots = native.get("object_snapshots", [])
                if table == "object_classes" and field == "total_bytes" and snapshots and snapshots[-1].get("has_total_memory_sizes") is False:
                    measurement_status = "unavailable"
                if table == "network_content_costs" and native.get("network_summary", {}).get("content_scan_truncated"):
                    measurement_status = "truncated"
                builder.add(f"{domain}.{table}.{field}", domain, row.get(field), unit, aggregate, (table,), scope=scope,
                            state=measurement_status, evidence_ids=[item["evidence_id"]])
    for table, fields in {
        "tasks": (("queue_delay_ms", "ms", "scheduled_to_start"), ("execution_ms", "ms", "start_to_finish")),
        "file_activity": (("actual_bytes", "bytes", "sum_completed_operations"), ("total_ms", "ms", "sum_operation_duration"), ("max_ms", "ms", "max_operation_duration"), ("failure_count", "count", "count")),
        "load_requests": (("duration_ms", "ms", "request_lifetime"), ("package_count", "count", "count")),
        "memory_tags": (("growth_bytes", "bytes", "end_minus_start"), ("peak_bytes", "bytes", "peak_gauge"), ("last_bytes", "bytes", "final_gauge")),
        "allocation_summary": (("growth_bytes", "bytes", "end_minus_start"), ("peak_bytes", "bytes", "peak_gauge"), ("last_bytes", "bytes", "final_gauge")),
        "object_snapshots": (("object_count", "count", "snapshot_gauge"), ("traced_object_count", "count", "snapshot_gauge")),
        "network_connections": (("packet_count", "count", "count"), ("total_bytes", "bytes", "sum_traced_packet_bytes"), ("mean_bytes_per_second", "bytes/s", "capture_lifetime_mean"), ("reliable_bunch_count", "bunches", "count_traced_bunches")),
        "timer_statistics": (("total_time_ms", "ms", "sum_inclusive_nested"), ("exclusive_total_time_ms", "ms", "sum_exclusive"), ("max_time_ms", "ms", "max_inclusive"), ("count", "count", "count")),
    }.items():
        for item in store.by_table.get(table, []):
            row = item["row"]
            domain = TABLE_DOMAINS[table][0]
            scope = {"level": "connection" if table == "network_connections" else "observation", "identity": str(row.get("identity", row.get("id", row.get("path", row.get("name", f"{row.get('tracker')}:{row.get('tag_id')}"))))), "role": item["process_role"]}
            if table in {"file_activity", "memory_tags"}:
                scope["comparison_key"] = f"{table}:{scope['identity']}:{scope['role']}"
            for field, unit, aggregate in fields:
                builder.add(f"{domain}.{table}.{field}", domain, row.get(field), unit, aggregate, (table,), scope=scope,
                            state="unavailable" if table == "network_connections" and field == "reliable_bunch_count" and row.get("bunch_metadata_available") is not True else None,
                            window={"start_seconds": item["start_seconds"], "end_seconds": item["end_seconds"], "timebase": item["timebase"]}, evidence_ids=[item["evidence_id"]])
    for item in store.by_table.get("telemetry_events", []):
        event = item["row"]
        start, end, timebase = row_window("telemetry_events", event)
        window = {"start_seconds": start, "end_seconds": end, "timebase": timebase}
        scope = {"level": "operation", "identity": str(event.get("occurrence_id") or event.get("event_id") or item["evidence_id"]),
                 "role": item["process_role"], "operation": event.get("operation"), "transport": event.get("transport")}
        if event.get("event_type") == "transport_operation":
            for field, unit in (("sent_payload_bytes", "bytes"), ("received_payload_bytes", "bytes"), ("measured_wire_bytes", "bytes"), ("duration_ms", "ms"), ("retry_count", "count")):
                builder.add(f"transport.operation.{field}", "transport", event.get(field), unit, "observed_operation", ("telemetry_events",),
                            scope=scope, window=window, state="unsupported" if field == "measured_wire_bytes" and event.get("wire_bytes_measured") is not True else None,
                            collector=str(event.get("source") or "runtime.transport"), evidence_ids=[item["evidence_id"]])
        for entry in event.get("collectors", []) if isinstance(event.get("collectors"), list) else []:
            if not isinstance(entry, dict):
                continue
            declared_metrics = entry.get("metrics") if isinstance(entry.get("metrics"), dict) else {}
            partial = declared_metrics.get("partial") is True
            collector_scope = {"level": "collector", "identity": str(entry.get("collector_id")), "role": item["process_role"],
                               "comparison_key": f"collector:{entry.get('collector_id')}:{item['process_role']}"}
            builder.add(f"collector.{entry.get('collector_id')}.overhead", "collector", entry.get("elapsed_ms"), "ms", "observed_collection",
                        ("telemetry_events",), scope=collector_scope, window=window,
                        state="measured" if entry.get("status") in {"observed", "observed_budget_overrun", "unavailable"} and "metrics" in entry else status(entry.get("status")),
                        collector=str(entry.get("collector_id")), evidence_ids=[item["evidence_id"]])
            for key, declared in declared_metrics.items():
                if key in {"partial", "objects_examined"}:
                    continue
                spec = declared if isinstance(declared, dict) else {"value": declared, "unit": "unknown"}
                raw_scope = spec.get("scope")
                typed_scope = dict(raw_scope) if isinstance(raw_scope, dict) else {**collector_scope, "population": str(raw_scope or "collector")}
                measurement_status = spec.get("status", entry.get("status"))
                if (entry.get("truncated") or partial or "partial" in str(raw_scope)) and status(measurement_status) == "measured":
                    measurement_status = "truncated"
                builder.add(str(spec.get("metric_id") or f"{entry.get('collector_id')}.{key}"), str(spec.get("domain") or "runtime"), spec.get("value"),
                            str(spec.get("unit") or "unknown"), str(spec.get("aggregation") or "gauge"), ("telemetry_events",),
                            scope=typed_scope, window=window, state=measurement_status,
                            collector=str(entry.get("collector_id")), evidence_ids=[item["evidence_id"]],
                            notes={key: spec[key] for key in ("value_semantics", "unavailable_reason", "truncation_reason") if key in spec})
    for item in store.by_table.get("runtime_metric_measurements", []):
        spec = item["row"]
        raw_scope = spec.get("scope")
        scope = dict(raw_scope) if isinstance(raw_scope, dict) else {"level": str(raw_scope or "process"), "identity": item["instance_id"] or context["session_id"], "role": item["process_role"]}
        scope.update({"asset_path": item["asset_path"], "instance_id": item["instance_id"], "class_path": spec.get("class_path"),
                      "class_asset_path": spec.get("class_asset_path"), "owner_attribution_status": spec.get("owner_attribution_status"),
                      "attribution": "explicit runtime metric owner fields only"})
        start, end, timebase = row_window("runtime_metric_measurements", spec)
        builder.add(str(spec.get("metric_id") or "runtime.unknown"), str(spec.get("domain") or "runtime"), spec.get("value"),
                    str(spec.get("unit") or "unknown"), str(spec.get("aggregation") or "gauge"), ("runtime_metric_measurements",),
                    scope=scope, window={"start_seconds": start, "end_seconds": end, "timebase": timebase}, state=spec.get("status"),
                    collector=str(spec.get("source") or "runtime.instrumented_metrics"), evidence_ids=[item["evidence_id"]])
    transport_events = [item for item in store.by_table.get("telemetry_events", []) if item["row"].get("event_type") == "transport_operation"]
    for field in ("sent_payload_bytes", "received_payload_bytes", "measured_wire_bytes", "retry_count"):
        values = [value for item in transport_events if (value := number(item["row"].get(field))) is not None
                  and (field != "measured_wire_bytes" or item["row"].get("wire_bytes_measured") is True)]
        builder.add(f"transport.{field}.total", "transport", sum(values) if values else None, "count" if field == "retry_count" else "bytes",
                    "sum_observed_operations", ("telemetry_events",), state="truncated" if values and len(values) < len(transport_events) else None)
    # Duplicate gauge snapshots at an identical time/scope have one deterministic record.
    return sorted({row["record_id"]: row for row in builder.records}.values(), key=lambda row: (row["domain"], row["metric_id"], row["record_id"]))


def causal_observations(store: EvidenceStore, context: dict[str, Any]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    findings: list[dict[str, Any]] = []
    for item in store.by_table.get("telemetry_events", []):
        event = item["row"]
        if event.get("event_type") == "gameplay_operation" and event.get("occurrence_id"):
            grouped[f"{item['run_id']}:{event['occurrence_id']}"].append(item)
        elif event.get("event_type") == "transport_operation" and str(event.get("result", "")).lower() in {"failed", "failure", "error", "timeout"}:
            findings.append({"category": "transport_failure", "severity": "warning", "title": "Transport operation reported failure",
                             "evidence": [f"{event.get('transport')} / {event.get('operation')}: {event.get('result')}"],
                             "evidence_ids": [item["evidence_id"]], "suggested_next_step": "Inspect operation/retry/result records; payload bytes are not wire costs."})
    observations: list[dict[str, Any]] = []
    for identity, items in sorted(grouped.items()):
        stages = sorted({str(item["row"].get("stage") or "unknown") for item in items})
        terminal = any(stage in stages for stage in ("client_applied", "failed", "cancelled"))
        observations.append({"logical_operation_id": identity, "run_id": items[0]["run_id"], "occurrence_id": items[0]["row"].get("occurrence_id"),
                             "operation": items[0]["row"].get("operation"), "observed_stages": stages,
                             "completion_status": "terminal_observed" if terminal else "capture_incomplete", "evidence_ids": [item["evidence_id"] for item in items],
                             "cross_process_duration_ms": None, "duration_policy": "stage IDs prove a logical relationship; no inferred global clock"})
        if "failed" in stages or ("request" in stages and not terminal):
            failed = "failed" in stages
            findings.append({"category": "causal_failure" if failed else "causal_incomplete", "severity": "warning" if failed else "info",
                             "title": "Gameplay operation recorded failure" if failed else "Gameplay operation has no terminal observation within this capture",
                             "evidence": [f"Occurrence {items[0]['row'].get('occurrence_id')} stages: {', '.join(stages)}"],
                             "evidence_ids": [item["evidence_id"] for item in items],
                             "suggested_next_step": "Query the paired run by occurrence_id. Check capture boundaries, cancellation and application handlers before attributing the gap to packet loss."})
    return observations, findings


def normalize_findings(findings: list[dict[str, Any]], store: EvidenceStore, context: dict[str, Any]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for index, finding in enumerate(findings):
        row = dict(finding)
        category = str(row.get("category") or "summary")
        domain, level, expression = RULES.get(category, ("unknown", "hypothesis", "legacy diagnostic; rule specification unavailable"))
        refs: list[str] = []
        for selector in row.get("evidence_ids", []):
            refs.extend([selector] if str(selector).startswith("ev:") else store.resolve_legacy(str(selector)))
        if not refs:
            candidates = {"memory": ("runtime_counters", "allocation_summary"), "objects": ("runtime_counters", "object_snapshots"),
                          "frame": ("game_frames", "runtime_frame_samples", "spikes"), "cpu": ("timing_events", "timing_scope_totals"),
                          "gpu": ("timing_events", "timing_scope_totals"), "coverage": ("report_coverage", "metadata"),
                          "network": ("network_metrics", "runtime_network_samples", "network_connections")}
            candidates["timing"] = ("timer_statistics",)
            refs = store.refs(*candidates.get(domain, ()))
        row.update({"id": row.get("id") or f"PS-{index + 1:04d}", "session_id": context["session_id"], "domain": domain,
                    "rule_id": f"PS.{category}", "rule_version": RULESET_VERSION, "rule_expression": expression,
                    "evidence_level": level, "confidence": None, "status": "observed" if level != "hypothesis" else "hypothesis",
                    "evidence_ids": sorted(set(refs))})
        row["finding_id"] = f"finding:{context['session_id']}:{row['rule_id']}:{digest({'evidence': row['evidence_ids'], 'title': row.get('title')})}"
        result.append(row)
    return result


def finalize_agent_contract(report: dict[str, Any], native: dict[str, Any], metadata: dict[str, Any], extras: dict[str, Any]) -> dict[str, Any]:
    context = context_for(metadata, report)
    generation = uuid.uuid4().hex
    context["report_generation_id"] = generation
    report.update({"schema_version": 4, "schema_name": "perf_sentinel.report", "contract_version": CONTRACT_VERSION,
                   "ruleset_version": RULESET_VERSION, "report_generation_id": generation, "context": context})
    store = EvidenceStore(context)
    store.populate(native, {**extras, "metadata": metadata, "report_coverage": report.get("coverage", {}),
                            "report_metrics": report.get("metrics", {}), "network_metrics": report.get("network", {}).get("metrics", {})})
    telemetry = extras.get("telemetry_events") or []
    collectors = collector_catalog(report, native, telemetry, store, extras.get("runtime_collector_catalog"))
    records = metric_records(report, native, store, context, telemetry)
    operations, causal_findings = causal_observations(store, context)
    existing_findings = [row for row in report.get("findings", []) if not causal_findings or row.get("category") != "summary"]
    findings = normalize_findings(existing_findings + causal_findings, store, context)
    comparison = {"contract_version": CONTRACT_VERSION, "ruleset_version": RULESET_VERSION,
                  "capture_context": report.get("metrics", {}).get("capture_context", {}),
                  "collector_status": {row["collector_id"]: row["status"] for row in collectors},
                  "build_configuration": context["build_configuration"], "build_target": context["build_target"],
                  "engine_build_version": context["engine_build_version"], "platform": context["platform"],
                  "unknown_fields_policy": "unknown is not proof of equal hardware/build/workload", "build_id_policy": "recorded separately; differing builds are allowed for code regression"}
    readiness = comparison_readiness(comparison["capture_context"])
    report.update({"metric_records": records, "collector_catalog": collectors, "findings": findings, "causal_operations": operations,
                   "comparison_readiness": readiness,
                   "comparison_fingerprint": {"id": digest(comparison), "fields": comparison},
                   "diagnostic_rules": [{"rule_id": f"PS.{key}", "rule_version": RULESET_VERSION, "domain": value[0], "evidence_level": value[1], "expression": value[2]} for key, value in sorted(RULES.items())]})
    report["summary"]["finding_count"] = len(findings)
    report["summary"]["severity_counts"] = {severity: sum(row.get("severity") == severity for row in findings) for severity in ("error", "warning", "info")}
    evidence_index = {"schema_version": 4, "contract_version": CONTRACT_VERSION, "session_id": context["session_id"], "report_generation_id": generation,
                      "database": report.get("artifacts", {}).get("evidence_database"), "evidence_id_format": "ev:<session>:<table>:<canonical-row-sha256-128>[:duplicateN]",
                      "resolver": "SELECT * FROM evidence_items WHERE evidence_id = ?", "row_count": len(store.rows),
                      "tables": [{"table_name": table, "row_count": len(rows), "domain": rows[0]["domain"], "collector_id": rows[0]["collector_id"],
                                  "population_evidence_id": store.summaries.get(table)} for table, rows in sorted(store.by_table.items())]}
    priority = {"error": 0, "warning": 1, "info": 2}
    selected_metrics = [row for row in records if row["scope"]["level"] == "session"]
    overview = {"schema_version": 4, "contract_version": CONTRACT_VERSION, "ruleset_version": RULESET_VERSION, "report_generation_id": generation,
                "session_id": context["session_id"], "run_id": context["run_id"], "context": context,
                "comparison_fingerprint": report["comparison_fingerprint"], "coverage": {"collectors": collectors,
                "complete_for_requested_profile": report.get("coverage", {}).get("complete_for_requested_profile", False)},
                "collectors": collectors, "domains": sorted({row["domain"] for row in records}),
                "comparison_readiness": readiness,
                "metrics": selected_metrics[:64], "metrics_total": len(records), "session_metrics_truncated": len(selected_metrics) > 64,
                "top_findings": [{**{key: row.get(key) for key in ("finding_id", "rule_id", "rule_version", "domain", "severity", "evidence_level", "title", "suggested_next_step")},
                                  "evidence_ids": row["evidence_ids"][:20], "evidence_ids_truncated": len(row["evidence_ids"]) > 20}
                                                        for row in sorted(findings, key=lambda row: (priority.get(row.get("severity"), 3), row["finding_id"]))[:20]],
                "causal_operations": [{**row, "evidence_ids": row["evidence_ids"][:20], "evidence_ids_truncated": len(row["evidence_ids"]) > 20} for row in operations[:30]],
                "causal_operations_truncated": len(operations) > 30,
                "unresolved_measurements": [row["collector_id"] for row in collectors if row["status"] != "measured"],
                "capture_channels": {key: metadata.get(key, []) for key in ("channels", "enabled_channels", "active_channels", "unavailable_channels")},
                "limitations": report.get("network", {}).get("limitations", []) + ["Source/asset/instance attribution uses explicit provider fields only; scope names do not establish owners.",
                "Diagnostic evidence levels are deterministic rule categories, not calibrated likelihoods. Correlation and missing stages do not establish root cause."],
                "artifacts": {key: report.get("artifacts", {}).get(key) for key in ("evidence_database", "evidence_index_json", "network_summary_json")}}
    definitions: dict[tuple[str, str, str, str], dict[str, Any]] = {}
    for row in records:
        key = (row["metric_id"], row["domain"], row["unit"], row["aggregation"])
        entry = definitions.setdefault(key, {"metric_id": key[0], "domain": key[1], "unit": key[2], "aggregation": key[3], "record_count": 0, "statuses": {}})
        entry["record_count"] += 1
        entry["statuses"][row["status"]] = entry["statuses"].get(row["status"], 0) + 1
    metric_catalog = {"schema_version": 4, "contract_version": CONTRACT_VERSION, "session_id": context["session_id"], "report_generation_id": generation,
                      "record_count": len(records), "metric_count": len(definitions), "database": evidence_index["database"],
                      "resolver": "SELECT * FROM metric_records WHERE session_id = ? AND metric_id = ?", "metrics": list(definitions.values()),
                      "statuses": sorted(STATUSES), "missing_value_policy": "unknown values are null; measured zero requires an observation"}
    return {"store": store, "overview": safe_json(overview), "evidence_index": evidence_index, "metric_catalog": metric_catalog}


def persist_agent_contract(database: Path, report: dict[str, Any], bundle: dict[str, Any]) -> None:
    store: EvidenceStore = bundle["store"]
    connection = sqlite3.connect(database)
    try:
        connection.execute("CREATE TABLE evidence_items (evidence_id TEXT PRIMARY KEY, session_id TEXT, domain TEXT, collector_id TEXT, table_name TEXT, row_index INTEGER, start_seconds REAL, end_seconds REAL, timebase TEXT, source_file TEXT, source_line INTEGER, asset_path TEXT, instance_id TEXT, occurrence_id TEXT, run_id TEXT, stage TEXT, process_role TEXT, process_id INTEGER, row_json TEXT NOT NULL)")
        fields = ("evidence_id", "session_id", "domain", "collector_id", "table_name", "row_index", "start_seconds", "end_seconds", "timebase", "source_file", "source_line", "asset_path", "instance_id", "occurrence_id", "run_id", "stage", "process_role", "process_id")
        connection.executemany("INSERT INTO evidence_items VALUES (" + ",".join("?" for _ in range(len(fields) + 1)) + ")",
                               (tuple(row.get(field) for field in fields) + (json.dumps(row["row"], ensure_ascii=False, allow_nan=False),) for row in store.rows))
        for name, columns in {"evidence_domain_time": "session_id,domain,timebase,start_seconds", "evidence_table_time": "session_id,table_name,start_seconds",
                              "evidence_source": "source_file,source_line", "evidence_asset": "asset_path", "evidence_instance": "instance_id",
                              "evidence_occurrence": "run_id,occurrence_id,stage"}.items():
            connection.execute(f"CREATE INDEX idx_{name} ON evidence_items({columns})")
        connection.execute("CREATE TABLE metric_records (record_id TEXT PRIMARY KEY,session_id TEXT,metric_id TEXT,domain TEXT,value REAL,unit TEXT,aggregation TEXT,status TEXT,scope_identity TEXT,role TEXT,timebase TEXT,start_seconds REAL,end_seconds REAL,collector_id TEXT,record_json TEXT NOT NULL)")
        connection.executemany("INSERT INTO metric_records VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", [(row["record_id"], store.context["session_id"], row["metric_id"], row["domain"], row["value"], row["unit"], row["aggregation"], row["status"], row["scope"].get("identity"), row["scope"].get("role"), row["window"].get("timebase"), row["window"].get("start_seconds"), row["window"].get("end_seconds"), row["source"]["collector_id"], json.dumps(row, ensure_ascii=False, allow_nan=False)) for row in report["metric_records"]])
        connection.execute("CREATE INDEX idx_metric_domain_scope ON metric_records(session_id,domain,metric_id,scope_identity,status)")
        connection.execute("CREATE TABLE diagnostic_findings (finding_id TEXT PRIMARY KEY,session_id TEXT,rule_id TEXT,rule_version TEXT,domain TEXT,evidence_level TEXT,severity TEXT,status TEXT,finding_json TEXT NOT NULL)")
        connection.executemany("INSERT INTO diagnostic_findings VALUES (?,?,?,?,?,?,?,?,?)", [(row["finding_id"], store.context["session_id"], row["rule_id"], row["rule_version"], row["domain"], row["evidence_level"], row["severity"], row["status"], json.dumps(safe_json(row), ensure_ascii=False, allow_nan=False)) for row in report["findings"]])
        connection.execute("CREATE INDEX idx_diagnostic_domain_rule ON diagnostic_findings(session_id,domain,rule_id,severity)")
        connection.execute("CREATE TABLE collector_catalog (collector_id TEXT PRIMARY KEY,status TEXT,catalog_json TEXT NOT NULL)")
        connection.executemany("INSERT INTO collector_catalog VALUES (?,?,?)", [(row["collector_id"], row["status"], json.dumps(safe_json(row), allow_nan=False)) for row in report["collector_catalog"]])
        connection.execute("CREATE TABLE report_context (session_id TEXT PRIMARY KEY,context_json TEXT NOT NULL,fingerprint_json TEXT NOT NULL)")
        connection.execute("INSERT INTO report_context VALUES (?,?,?)", (store.context["session_id"], json.dumps(store.context), json.dumps(safe_json(report["comparison_fingerprint"]))))
        for key, value in (("report_generation_id", report["report_generation_id"]), ("context", store.context),
                           ("contract_version", CONTRACT_VERSION), ("ruleset_version", RULESET_VERSION)):
            connection.execute("INSERT OR REPLACE INTO session VALUES (?,?)", (key, json.dumps(safe_json(value), allow_nan=False)))
        connection.commit()
    finally:
        connection.close()


def publish_agent_contract(output: Path, bundle: dict[str, Any]) -> None:
    for key in ("metric_catalog", "evidence_index", "overview"):
        filename = "agent_overview.json" if key == "overview" else f"{key}.json"
        atomic_write_json(output / filename, bundle[key])
