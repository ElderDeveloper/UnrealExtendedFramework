"""PerfSentinel LLM brief.

Writes llm_brief.md (read this first) and llm_brief.json next to the other reports. The brief puts the
facts an engineer or a language model needs at the top, in a fixed order, with units and the population
each number covers: what was captured, what data is missing and why, frame time and which thread bound it,
a timeline, every hitch with its likely cause and the evidence around it, where steady-state frame time goes
per thread, rendering, memory, loading, gameplay churn and networking. Measured values and inferences are
kept apart: anything inferred says "likely" and names the evidence.

Inputs are the analyzer's own structures (native evidence after the capture-window filter, runtime sidecars,
the finished report), so the brief never re-reads the trace.
"""

from __future__ import annotations

import json
import math
import re
import statistics
from pathlib import Path
from typing import Any

BRIEF_VERSION = 1
CAPTURE_PROFILES = ["Custom", "Standard", "HitchDiagnosis", "LoadingStreaming", "MemoryLeak", "Multiplayer",
                    "UIAnimation", "ComprehensiveGameplay", "LightweightBaseline"]
MAX_HITCH_ROWS = 40
MAX_TIMELINE_ROWS = 30
NEARBY_SECONDS = 0.35

# Scopes that only wait for other threads or the GPU. They show where a thread was blocked, not what was slow.
WAIT_RE = re.compile(r"WaitFor|WaitImpl|WaitUntil|^Wait|_Wait|Stall|Idle|Sleep|LookingForWork|SyncPoint|ProcessUntilTasksComplete"
                     r"|FlushRenderingCommands|FlushRHIThread|EndOfPipeTask|BlockUntil", re.IGNORECASE)
# Ordered: the first rule whose scopes carry the most self time names the likely cause.
CAUSE_RULES: list[tuple[str, str, str]] = [
    ("shader_compilation", r"ShaderCompil|ShaderMap|FinishCompilation|ShaderJob|CompileShader",
     "shader compilation blocked the frame (editor; a cooked build ships compiled shaders)"),
    ("pso_creation", r"PipelineState|PSO|CreateGraphicsPipeline|CreateComputePipeline", "pipeline state (PSO) created on demand"),
    ("garbage_collection", r"CollectGarbage|GarbageCollect|IncrementalPurge|Reachability|BeginDestroy|FRealtimeGC|PurgeGarbage",
     "garbage collection"),
    ("synchronous_load", r"LoadPackage|FlushAsyncLoading|StaticLoadObject|LoadObject|SyncLoad|AsyncLoading|PostLoad", "synchronous or flushed asset loading"),
    ("level_streaming", r"LevelStreaming|AddToWorld|RemoveFromWorld|UpdateStreamingState|WorldPartition|StreamingLevel|BlockTillLevelStreaming",
     "level streaming (levels or world-partition cells added or removed)"),
    ("profiler_overhead", r"PerfSentinel_", "PerfSentinel's own collectors"),
    ("os_present_wait", r"^WinPumpMessages$|Present|SwapChain|^Slate::DrawWindow$",
     "the OS message pump or presenting the frame (window, focus, vsync, compositor); not game work"),
    ("actor_spawning", r"SpawnActor|PostActorConstruction|FinishSpawning|BeginPlay|InitializeComponents|RegisterComponent|ExecuteConstruction",
     "actor spawning and initialization"),
    ("physics", r"Chaos|Physics|PhysScene|Collision|Overlap|Sweep", "physics"),
    ("animation", r"Anim|Skeletal|Skinned|PoseTick|RefreshBoneTransforms", "animation"),
    ("vfx", r"Niagara|Particle", "Niagara / particles"),
    ("navigation", r"Navigation|NavMesh|Recast|PathFind", "navigation"),
    ("networking", r"Replicat|NetDriver|TickFlush|TickDispatch|NetBroadcast|ReceivedPacket|NetSerialize|Iris|RPC", "networking / replication"),
    ("ui", r"Slate|UMG|Widget|Paint", "UI (Slate/UMG)"),
    ("audio", r"Audio|MetaSound|Metasound|Sound", "audio"),
    ("blueprint", r"Blueprint|K2Node|ProcessEvent|ScriptTime|BP_", "Blueprint / script"),
    ("rendering", r"Render|RDG|RHI|Draw|Mesh|Nanite|Lumen|Shadow|Scene|GPU|Texture|D3D12|DLSS|PostProcess", "rendering"),
]
CAUSE_PATTERNS = [(key, re.compile(pattern, re.IGNORECASE), text) for key, pattern, text in CAUSE_RULES]
NETWORK_SCOPE_RE = re.compile(r"ServerReplicateActors|ReplicateActor|NetDriver|TickFlush|TickDispatch|NetBroadcastTick|ReceivedPacket"
                              r"|ProcessRemoteFunction|RepLayout|NetSerialize|Iris|ReplicationSystem|ActorChannel", re.IGNORECASE)
RESIZE_TEXT_RE = re.compile(r"ToggleImmersive|Fullscreen|Resize|ViewportResized|WindowMode", re.IGNORECASE)
CAPTURE_MARKERS = {"PerfSentinel.CaptureStart", "PerfSentinel.CaptureStop", "PerfSentinel.Unfocused.Begin", "PerfSentinel.Unfocused.End"}
RENDER_COUNTERS = [
    ("STAT_RHIDrawPrimitiveCalls", "RHI draw calls"), ("STAT_RHITriangles", "RHI triangles"), ("STAT_MeshDrawCalls", "mesh draw calls"),
    ("STAT_ProcessedPrimitives", "primitives processed"), ("STAT_CulledPrimitives", "primitives culled"), ("STAT_OccludedPrimitives", "primitives occluded"),
    ("STAT_VisibleDynamicPrimitives", "visible dynamic primitives"), ("Scene/Primitives", "scene primitives"), ("Scene/Lights", "scene lights"),
    ("STAT_NumShadowedLights", "shadowed lights"), ("STAT_WholeSceneShadows", "whole-scene shadows"), ("STAT_VSMNaniteViewsPrimary", "VSM Nanite views"),
    ("STAT_SlateTotalWidgetsPerFrame", "Slate widgets per frame"), ("STAT_RenderTargetPoolCount", "render targets pooled"),
    ("STAT_RDG_TransientTextureCount", "RDG transient textures"), ("STAT_NaniteStreaming31_VisibleStreamingPoolPercentage", "Nanite streaming pool %"),
    ("STAT_NaniteStreaming20_PageRequests", "Nanite page requests"), ("STAT_Streaming05_Pending2DUpdate", "texture streaming pending updates"),
    ("STAT_VirtualTexture_PendingFeedback", "virtual texture pending feedback"),
]


def _num(value: Any) -> float | None:
    if isinstance(value, bool) or value is None:
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def _percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    low = math.floor(position)
    high = math.ceil(position)
    return ordered[low] if low == high else ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def _distribution(values: list[float]) -> dict[str, Any] | None:
    values = [value for value in values if value is not None and math.isfinite(value)]
    if not values:
        return None
    return {"count": len(values), "mean": statistics.fmean(values), "p50": _percentile(values, 0.5), "p95": _percentile(values, 0.95),
            "p99": _percentile(values, 0.99), "max": max(values)}


def _ms(value: Any, digits: int = 1) -> str:
    number = _num(value)
    return "-" if number is None else f"{number:.{digits}f}"


def _mb(value: Any) -> str:
    number = _num(value)
    return "-" if number is None else f"{number / (1024.0 * 1024.0):,.0f} MB"


def _int(value: Any) -> str:
    number = _num(value)
    return "-" if number is None else f"{number:,.0f}"


def _cell(value: Any) -> str:
    return str(value).replace("|", "/").replace("\n", " ")


def compute_time_alignment(native: dict[str, Any], metadata: dict[str, Any]) -> dict[str, Any]:
    """Offset from sidecar platform seconds to trace seconds, anchored on the capture bookmarks.

    Trace timestamps and FPlatformTime::Seconds() read the same clock, so a bookmark's trace time minus the
    platform time recorded next to it is a constant offset; the stop anchor measures any disagreement.
    """
    marks = [(float(row["time_seconds"]), str(row.get("text") or "")) for row in native.get("bookmarks", [])
             if isinstance(row, dict) and _num(row.get("time_seconds")) is not None]
    # The trace's buffered history can hold an earlier capture's markers: this capture is the last start.
    all_starts = sorted(time for time, text in marks if text == "PerfSentinel.CaptureStart")
    starts = all_starts[-1:]
    stops = sorted(time for time, text in marks if text == "PerfSentinel.CaptureStop" and (not starts or time >= starts[0]))
    start_platform = _num(metadata.get("capture_start_bookmark_platform_seconds"))
    stop_platform = _num(metadata.get("capture_stop_bookmark_platform_seconds"))
    if starts and start_platform is not None:
        offset = starts[0] - start_platform
        drift_ms = ((stops[-1] - stop_platform) - offset) * 1000.0 if stops and stop_platform is not None else None
        return {"status": "anchored", "offset_seconds": offset, "capture_start_trace_seconds": starts[0],
                "stop_anchor_disagreement_ms": drift_ms, "uncertainty_ms": max(1.0, abs(drift_ms)) if drift_ms is not None else 1.0}
    monitor_start = _num(metadata.get("capture_start_platform_seconds"))
    if starts and monitor_start is not None:
        # Metadata without the bookmark anchor: the runtime monitor activates a few milliseconds after the bookmark.
        return {"status": "approximate", "offset_seconds": starts[0] - monitor_start, "capture_start_trace_seconds": starts[0], "uncertainty_ms": 20.0}
    if starts:
        return {"status": "capture_start_only", "capture_start_trace_seconds": starts[0], "uncertainty_ms": 50.0}
    return {"status": "unavailable", "reason": "no PerfSentinel.CaptureStart bookmark (older capture)"}


def _to_trace(row: dict[str, Any], alignment: dict[str, Any]) -> float | None:
    if alignment.get("status") in ("anchored", "approximate") and _num(row.get("platform_seconds")) is not None:
        return float(row["platform_seconds"]) + float(alignment["offset_seconds"])
    if alignment.get("status") in ("anchored", "approximate", "capture_start_only") and _num(row.get("capture_elapsed_seconds")) is not None:
        return float(alignment["capture_start_trace_seconds"]) + float(row["capture_elapsed_seconds"])
    return None


def _classify_bound(sample: dict[str, Any]) -> str:
    frame = _num(sample.get("frame_time_ms"))
    if not frame or frame <= 0:
        return "unknown"
    idle = _num(sample.get("idle_ms")) or 0.0
    if idle >= 0.25 * frame:
        return "frame_rate_cap"
    threads = {"game_thread": _num(sample.get("game_thread_ms")), "render_thread": _num(sample.get("render_thread_ms")),
               "rhi_thread": _num(sample.get("rhi_thread_ms")), "gpu": _num(sample.get("gpu_ms"))}
    valid = {key: value for key, value in threads.items() if value is not None and 0.0 <= value <= frame * 4.0}
    if not valid:
        return "unknown"
    key, value = max(valid.items(), key=lambda item: item[1])
    return key if value >= 0.8 * (frame - idle) else "unattributed_wait_or_present"


def _category(timer: str) -> tuple[str, str] | None:
    for key, pattern, text in CAUSE_PATTERNS:
        if pattern.search(timer):
            return key, text
    return None


def _match_sample(frame: dict[str, Any], samples: list[tuple[float, dict[str, Any]]]) -> dict[str, Any] | None:
    """Runtime sample written at the end of this frame: nearest by time, preferring a matching frame time."""
    end = _num(frame.get("end_seconds"))
    duration = _num(frame.get("duration_ms")) or 0.0
    if end is None or not samples:
        return None
    best: tuple[float, dict[str, Any]] | None = None
    for time, sample in samples:
        if abs(time - end) > max(0.1, duration / 1000.0 * 2.0):
            continue
        score = abs(time - end) * 1000.0 + abs((_num(sample.get("frame_time_ms")) or 0.0) - duration)
        if best is None or score < best[0]:
            best = (score, sample)
    return best[1] if best else None


def _hitch_rows(native: dict[str, Any], self_events: list[dict[str, Any]], hitch_ms: float, origin: float,
                samples: list[tuple[float, dict[str, Any]]], timeline_events: list[dict[str, Any]]) -> list[dict[str, Any]]:
    frames = [row for row in native.get("game_frames", []) if isinstance(row, dict) and (_num(row.get("duration_ms")) or 0.0) >= hitch_ms]
    by_frame: dict[int, list[dict[str, Any]]] = {}
    # Exact per-timer self time per frame and thread from TraceServices (newer extractor) beats self time rebuilt from
    # the raw event list, which drops sub-50 us scopes and lists repeated calls one by one.
    for row in native.get("hitch_scope_totals", []) or []:
        if isinstance(row, dict):
            by_frame.setdefault(int(_num(row.get("frame_index")) or -1), []).append(
                {"thread": row.get("thread"), "timer": row.get("timer"), "self_time_ms": row.get("exclusive_ms"), "aggregated": True})
    aggregated_frames = set(by_frame)
    for event in self_events:
        frame_index = int(_num(event.get("frame_index")) or -1)
        if frame_index not in aggregated_frames:
            by_frame.setdefault(frame_index, []).append(event)
    rows: list[dict[str, Any]] = []
    for frame in sorted(frames, key=lambda row: _num(row.get("start_seconds")) or 0.0):
        start = _num(frame.get("start_seconds")) or 0.0
        end = _num(frame.get("end_seconds")) or start
        duration = _num(frame.get("duration_ms")) or 0.0
        events = by_frame.get(int(_num(frame.get("index")) or -1), [])
        categories: dict[str, float] = {}
        game_scopes: list[tuple[float, str]] = []
        other_scopes: list[tuple[float, str, str]] = []
        game_wait_ms = 0.0
        gpu_busy_ms = 0.0
        for event in events:
            timer = str(event.get("timer") or "")
            thread = str(event.get("thread") or "")
            self_ms = _num(event.get("self_time_ms")) or 0.0
            if self_ms <= 0.05:
                continue
            is_gpu = thread.startswith("GPU")
            if WAIT_RE.search(timer):
                if thread == "GameThread":
                    game_wait_ms += self_ms
                continue
            if is_gpu:
                gpu_busy_ms += self_ms
            if thread == "GameThread":
                game_scopes.append((self_ms, timer))
            elif thread.startswith("RenderThread") or thread == "RHIThread" or is_gpu:
                other_scopes.append((self_ms, thread, timer))
            if thread == "GameThread" or thread.startswith("RenderThread") or thread == "RHIThread":
                category = _category(timer)
                if category:
                    categories[category[0]] = categories.get(category[0], 0.0) + self_ms
        nearby = [event for event in timeline_events if start - NEARBY_SECONDS <= event["t_trace"] <= end + NEARBY_SECONDS]
        if 0.0 <= start - origin < 1.0:
            # The first second of a capture carries PerfSentinel's own start-up work (trace start, first snapshots).
            nearby = [{"label": "capture start (PerfSentinel start-up work)", "event": "capture_start", "t_trace": origin}] + nearby
        cause_key, cause_ms = (max(categories.items(), key=lambda item: item[1]) if categories else ("", 0.0))
        cause_text = next((text for key, _, text in CAUSE_PATTERNS if key == cause_key), "")
        gc = next((event for event in nearby if event["event"] == "gc"), None)
        resize = next((event for event in timeline_events if start - 1.0 <= event["t_trace"] <= end
                       and (event["event"] == "viewport_resized" or (event["event"] == "bookmark" and RESIZE_TEXT_RE.search(event["label"])))), None)
        basis = "instrumented scopes"
        if gc and (_num(gc.get("duration_ms")) or 0.0) >= 0.3 * duration:
            cause_key, cause_text, basis = "garbage_collection", "garbage collection", f"GC event {_ms(gc.get('duration_ms'))} ms in this frame"
        elif resize and (cause_key in ("", "rendering", "ui", "os_present_wait") or cause_ms < 0.5 * duration):
            # A resize rebuilds render targets and the upscaler: the cost shows up as rendering or UI scopes.
            cause_key, cause_text, basis = "viewport_resize", "viewport resize (render targets and upscaler rebuilt)", f"'{resize['label']}' just before"
        elif cause_ms < 0.25 * duration:
            if gpu_busy_ms >= 0.7 * duration:
                cause_key, cause_text, basis = "gpu", "GPU work", f"GPU queues busy {gpu_busy_ms:.0f} ms"
            elif game_wait_ms >= 0.4 * duration:
                cause_key, cause_text, basis = "waiting_on_other_threads", "the game thread waited on render/RHI/GPU", f"game-thread waits {game_wait_ms:.0f} ms"
            else:
                cause_key, cause_text, basis = "unexplained", "no instrumented work explains it", "check waits, OS scheduling (context switches need an elevated editor)"
        sample = _match_sample(frame, samples)
        rows.append({
            "t_seconds": round(start - origin, 3), "frame_index": frame.get("index"), "duration_ms": round(duration, 2),
            "likely_cause": cause_key or "unexplained", "cause_text": cause_text, "cause_basis": basis,
            "cause_ms": round(cause_ms, 2),
            "game_thread_top": [{"timer": timer, "self_ms": round(ms, 2)} for ms, timer in sorted(game_scopes, reverse=True)[:3]],
            # CPU render/RHI scopes first: GPU queue scopes span idle gaps, so their self time overstates GPU work.
            "other_threads_top": [{"thread": thread, "timer": timer, "self_ms": round(ms, 2)}
                                  for ms, thread, timer in (sorted((item for item in other_scopes if not item[1].startswith("GPU")), reverse=True)[:2]
                                                            + sorted((item for item in other_scopes if item[1].startswith("GPU")), reverse=True)[:1])],
            "game_thread_wait_ms": round(game_wait_ms, 2),
            "runtime_thread_ms": None if not sample else {key: _num(sample.get(key)) for key in ("game_thread_ms", "render_thread_ms", "rhi_thread_ms", "gpu_ms", "idle_ms")},
            "nearby_events": [event["label"] for event in nearby][:6],
            "scope_data": ("exact_per_timer_totals" if any(event.get("aggregated") for event in events) else "raw_events") if events else "not_retained",
        })
    return rows


def _timeline(frames: list[dict[str, Any]], origin: float, hitch_ms: float, events: list[dict[str, Any]], unfocused: list[dict[str, Any]]) -> list[dict[str, Any]]:
    if not frames:
        return []
    last = max(_num(row.get("end_seconds")) or 0.0 for row in frames)
    duration = max(1.0, last - origin)
    bucket = max(5.0, math.ceil(duration / MAX_TIMELINE_ROWS / 5.0) * 5.0)
    buckets: dict[int, list[float]] = {}
    for row in frames:
        start = _num(row.get("start_seconds"))
        if start is None:
            continue
        buckets.setdefault(int((start - origin) // bucket), []).append(_num(row.get("duration_ms")) or 0.0)
    rows = []
    for index in range(int(duration // bucket) + 1):
        values = buckets.get(index, [])
        low, high = origin + index * bucket, origin + (index + 1) * bucket
        labels: dict[str, int] = {}
        for event in events:
            if low <= event["t_trace"] < high:
                labels[event["short"]] = labels.get(event["short"], 0) + 1
        unfocused_s = sum(max(0.0, min(high, span["end_seconds"]) - max(low, span["start_seconds"])) for span in unfocused)
        rows.append({
            "from_s": round(index * bucket, 1), "to_s": round((index + 1) * bucket, 1), "frames": len(values),
            "mean_ms": round(statistics.fmean(values), 2) if values else None, "p95_ms": round(_percentile(values, 0.95), 2) if values else None,
            "max_ms": round(max(values), 2) if values else None, "hitches": sum(value >= hitch_ms for value in values),
            "unfocused_s": round(unfocused_s, 1),
            "events": ", ".join(f"{name}×{count}" if count > 1 else name for name, count in sorted(labels.items(), key=lambda item: -item[1])[:5]),
        })
    return rows


def _timeline_events(native: dict[str, Any], events: list[dict[str, Any]], alignment: dict[str, Any]) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    for row in native.get("bookmarks", []):
        if not isinstance(row, dict) or _num(row.get("time_seconds")) is None:
            continue
        text = str(row.get("text") or "")
        if text in CAPTURE_MARKERS or text.startswith("PerfSentinelSpike_"):
            continue
        short = "GC" if text == "GC" else text.replace("Cmd: ", "").replace("PerfSentinel.", "")[:40]
        out.append({"t_trace": float(row["time_seconds"]), "event": "bookmark", "label": text[:80], "short": short})
    for row in events:
        kind = str(row.get("event") or "")
        if kind in ("recording_started", "recording_stopped", "focus_gained", "focus_lost", "game_world_observed"):
            continue
        time = _to_trace(row, alignment)
        if time is None:
            continue
        if kind == "gc":
            label, short = f"GC {_ms(row.get('duration_ms'))} ms", "GC"
        elif kind in ("level_added", "level_removed"):
            label, short = f"{kind.replace('_', ' ')} {str(row.get('level') or '').rsplit('/', 1)[-1]}", kind.replace("_", " ")
        elif kind == "actor_churn":
            label, short = f"spawned {_int(row.get('spawned'))}, destroyed {_int(row.get('destroyed'))}", "actor churn"
            if (_num(row.get("spawned")) or 0) + (_num(row.get("destroyed")) or 0) < 20:
                continue
        elif kind == "viewport_resized":
            if not row.get("game_viewport"):
                continue
            label, short = f"viewport resized {_int(row.get('width'))}x{_int(row.get('height'))}", "viewport resize"
        elif kind in ("network_failure", "travel_failure"):
            label, short = f"{kind.replace('_', ' ')}: {row.get('failure')}", kind.replace("_", " ")
        elif kind in ("player_login", "player_logout"):
            label, short = f"{kind.replace('_', ' ')} (players {row.get('players')})", kind.replace("_", " ")
        else:
            label, short = kind.replace("_", " "), kind.replace("_", " ")
        out.append({**row, "t_trace": time, "event": kind, "label": label, "short": short})
    return sorted(out, key=lambda row: row["t_trace"])


def _thread_costs(native: dict[str, Any], frame_count: int) -> list[dict[str, Any]]:
    rows = [row for row in native.get("thread_scope_totals", []) if isinstance(row, dict)]
    summaries = {str(row.get("thread")): row for row in (native.get("thread_scope_summary") or {}).get("threads", []) if isinstance(row, dict)}
    if frame_count <= 0:
        return []
    if not rows:
        # Older extractor: only whole-process totals over the whole trace exist. Report them per trace second, labelled.
        seconds = _num(native.get("duration_seconds"))
        # Scopes open for seconds at a time (scheduler loops such as Houdini's) are not per-frame work.
        totals = sorted((row for row in native.get("timing_scope_totals", []) if isinstance(row, dict) and row.get("kind") == "cpu"
                         and (_num(row.get("max_inclusive_ms")) or 0.0) < 5000.0),
                        key=lambda row: -(_num(row.get("exclusive_ms")) or 0.0))[:15]
        if not totals or not seconds:
            return []
        return [{"thread": "All CPU threads (whole trace, older extractor; ms per trace second)", "kind": "cpu_all", "busy_ms_per_frame": None,
                 "wait_ms_per_frame_in_top_rows": None,
                 "top": [{"timer": row.get("timer"), "ms_per_frame": round((_num(row.get("exclusive_ms")) or 0.0) / seconds, 3),
                          "calls_per_frame": round((_num(row.get("instance_count")) or 0.0) / seconds, 1),
                          "max_ms": round(_num(row.get("max_inclusive_ms")) or 0.0, 2), "wait": bool(WAIT_RE.search(str(row.get("timer") or "")))}
                         for row in totals]}]
    order = ["GameThread", "RenderThread 0", "RHIThread"]
    threads = sorted({str(row.get("thread")) for row in rows},
                     key=lambda name: (order.index(name) if name in order else (3 if name.startswith("RenderThread") else (4 if name.startswith("GPU") else 5)), name))
    out = []
    for thread in threads:
        scoped = sorted((row for row in rows if str(row.get("thread")) == thread), key=lambda row: -(_num(row.get("exclusive_ms")) or 0.0))
        total = _num((summaries.get(thread) or {}).get("total_exclusive_ms"))
        waits = sum(_num(row.get("exclusive_ms")) or 0.0 for row in scoped if WAIT_RE.search(str(row.get("timer") or "")))
        limit = 8 if thread == "Task workers" or thread.startswith("GPU") and "Graphics" not in thread else 12
        out.append({
            "thread": thread, "kind": scoped[0].get("kind") if scoped else "",
            "busy_ms_per_frame": round(total / frame_count, 3) if total is not None else None,
            "wait_ms_per_frame_in_top_rows": round(waits / frame_count, 3),
            "top": [{"timer": row.get("timer"), "ms_per_frame": round((_num(row.get("exclusive_ms")) or 0.0) / frame_count, 3),
                     "calls_per_frame": round((_num(row.get("instance_count")) or 0.0) / frame_count, 1),
                     "max_ms": round(_num(row.get("max_exclusive_ms")) or 0.0, 2), "wait": bool(WAIT_RE.search(str(row.get("timer") or "")))}
                    for row in scoped[:limit]],
        })
    return out


def _counter_lookup(native: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(row.get("name")): row for row in native.get("counters", []) if isinstance(row, dict) and row.get("name")}


def _series(rows: list[dict[str, Any]], key: str) -> list[float]:
    return [value for row in rows if (value := _num(row.get(key))) is not None]


def _network_section(network_samples: list[dict[str, Any]], events: list[dict[str, Any]], native: dict[str, Any], network: dict[str, Any],
                     thread_costs: list[dict[str, Any]], frame_count: int) -> dict[str, Any]:
    connections: dict[str, list[dict[str, Any]]] = {}
    drivers: dict[str, list[dict[str, Any]]] = {}
    disconnects = 0
    for sample in network_samples:
        disconnects += sum(1 for row in sample.get("disconnected_connections", []) if isinstance(row, dict) and row.get("state") == "disconnected")
        for world in sample.get("worlds", []) or []:
            for driver in world.get("drivers", []) or []:
                driver_key = f"{world.get('net_mode')}/{driver.get('driver_name')}#{driver.get('driver_id')}"
                drivers.setdefault(driver_key, []).append({**driver, "_t": _num(sample.get("capture_elapsed_seconds"))})
                for connection in driver.get("connections", []) or []:
                    key = f"{driver_key}/conn{connection.get('connection_id')}{' (server conn)' if connection.get('is_server_connection') else ''}"
                    connections.setdefault(key, []).append(connection)
    connection_rows = []
    for key, rows in connections.items():
        valid = [row for row in rows if row.get("counter_status") == "valid"]
        rtt = _series(rows, "avg_rtt_ms")
        connection_rows.append({
            "connection": key, "samples": len(rows), "last_state": rows[-1].get("state"),
            "rtt_ms": _distribution(rtt), "jitter_ms": _distribution(_series(rows, "jitter_ms")),
            "in_kib_s": _distribution([value / 1024.0 for value in _series(valid, "in_bytes_per_second")]),
            "out_kib_s": _distribution([value / 1024.0 for value in _series(valid, "out_bytes_per_second")]),
            "in_loss_pct_max": max(_series(valid, "in_loss_percent"), default=None),
            "out_loss_pct_max": max(_series(valid, "out_loss_percent"), default=None),
            "not_net_ready_samples": sum(1 for row in rows if row.get("is_net_ready") is False),
            "queued_bits_positive_samples": sum(1 for row in rows if (_num(row.get("queued_bits")) or 0) > 0),
            "reliable_backlog_max": max(_series(rows, "reliable_outstanding_bunches"), default=None),
            "seconds_since_last_receive_max": max(_series(rows, "seconds_since_last_receive"), default=None),
            "dormant_actors_max": max(_series(rows, "dormant_actors"), default=None),
            "actor_channels_max": max(_series(rows, "actor_channels"), default=None),
            "delayed_rpcs_max": max(_series(rows, "total_delayed_rpcs"), default=None),
        })
    driver_rows = []
    for key, rows in drivers.items():
        times = [row["_t"] for row in rows if row.get("_t") is not None]
        span = (max(times) - min(times)) if len(times) >= 2 else None
        def rate(field: str) -> float | None:
            values = _series(rows, field)
            if span and len(values) >= 2 and values[-1] >= values[0]:
                return (values[-1] - values[0]) / span
            return None
        driver_rows.append({"driver": key, "samples": len(rows), "uses_iris": rows[-1].get("uses_iris"), "is_server": rows[-1].get("is_server"),
                            "net_server_max_tick_rate": rows[-1].get("net_server_max_tick_rate"),
                            "rpcs_per_second": rate("total_rpcs_called"), "reliable_bunches_per_second": rate("total_reliable_bunches_sent"),
                            "network_objects_max": max(_series(rows, "network_objects"), default=None),
                            "active_network_objects_max": max(_series(rows, "active_network_objects"), default=None),
                            "packet_simulation": rows[-1].get("packet_simulation")})
    net_events = [row for row in events if str(row.get("event")) in ("network_failure", "travel_failure", "player_login", "player_logout")]
    cpu_rows = []
    for thread in thread_costs:
        for row in thread.get("top", []):
            if NETWORK_SCOPE_RE.search(str(row.get("timer") or "")):
                cpu_rows.append({"thread": thread["thread"], **row})
    if not cpu_rows and frame_count > 0:
        for row in native.get("timing_scope_totals", []) or []:
            if isinstance(row, dict) and row.get("kind") == "cpu" and NETWORK_SCOPE_RE.search(str(row.get("timer") or "")):
                cpu_rows.append({"thread": "all CPU threads", "timer": row.get("timer"),
                                 "ms_per_frame": round((_num(row.get("exclusive_ms")) or 0.0) / frame_count, 3)})
    cpu_rows = sorted((row for row in cpu_rows if (row.get("ms_per_frame") or 0.0) >= 0.01), key=lambda row: -(row.get("ms_per_frame") or 0.0))[:10]
    counters = [row for row in native.get("counters", []) if isinstance(row, dict) and row.get("group") == "STATGROUP_Net" and (_num(row.get("max")) or 0) > 0]
    content = [{"name": row.get("name") or row.get("event_type") or row.get("content"), "direction": row.get("direction"),
                "exclusive_bits": row.get("exclusive_bits"), "count": row.get("count") or row.get("instance_count")}
               for row in (network.get("top_content_costs") or []) if isinstance(row, dict)]
    return {"connections": connection_rows, "drivers": driver_rows, "events": net_events, "disconnects_observed": disconnects,
            "cpu_scopes": cpu_rows, "trace_net_counters": sorted(counters, key=lambda row: -(_num(row.get("max")) or 0))[:20],
            "content": content[:10] if isinstance(content, list) else [], "status": network.get("status")}


def build_brief(*, report: dict[str, Any], native: dict[str, Any], metadata: dict[str, Any], runtime_context: dict[str, Any],
                frame_samples: list[dict[str, Any]], counters: list[dict[str, Any]], events: list[dict[str, Any]],
                network_samples: list[dict[str, Any]], self_events: list[dict[str, Any]], capture_window: dict[str, Any],
                hitch_threshold_ms: float, frame_budget_ms: float) -> dict[str, Any]:
    alignment = compute_time_alignment(native, metadata)
    frames = [row for row in native.get("game_frames", []) if isinstance(row, dict) and _num(row.get("duration_ms")) is not None]
    origin = _num(alignment.get("capture_start_trace_seconds"))
    if origin is None:
        origin = min((_num(row.get("start_seconds")) or 0.0 for row in frames), default=0.0)
    frame_values = [float(row["duration_ms"]) for row in frames]
    frame_dist = _distribution(frame_values)
    if frame_dist is None:
        frame_dist = _distribution(_series(frame_samples, "frame_time_ms"))

    def plausible(sample: dict[str, Any], key: str) -> float | None:
        # A thread time far beyond its frame is stale state (one first sample reported 8175 ms in an 8 ms frame).
        value, frame = _num(sample.get(key)), _num(sample.get("frame_time_ms"))
        if value is None or key in ("frame_time_ms", "draw_calls", "primitives") or frame is None:
            return value
        return value if value <= frame * 2.0 + 50.0 else None
    thread_dists = {key: _distribution([value for sample in frame_samples if (value := plausible(sample, key)) is not None]) for key in
                    ("frame_time_ms", "game_thread_ms", "render_thread_ms", "rhi_thread_ms", "gpu_ms", "idle_ms", "draw_calls", "primitives")}
    bounds: dict[str, int] = {}
    for sample in frame_samples:
        bound = _classify_bound(sample)
        bounds[bound] = bounds.get(bound, 0) + 1
    bound_total = sum(bounds.values())
    bound_share = {key: round(100.0 * value / bound_total, 1) for key, value in sorted(bounds.items(), key=lambda item: -item[1])} if bound_total else {}

    samples_in_trace = sorted(((time, sample) for sample in frame_samples if (time := _to_trace(sample, alignment)) is not None), key=lambda item: item[0])
    timeline_events = _timeline_events(native, events, alignment)
    hitches = _hitch_rows(native, self_events, hitch_threshold_ms, origin, samples_in_trace, timeline_events)
    cause_counts: dict[str, int] = {}
    for row in hitches:
        cause_counts[row["likely_cause"]] = cause_counts.get(row["likely_cause"], 0) + 1
    timeline = _timeline(frames, origin, hitch_threshold_ms, timeline_events, capture_window.get("unfocused_intervals") or [])
    thread_costs = _thread_costs(native, len(frames))

    counter_lookup = _counter_lookup(native)
    render_counters = []
    for name, label in RENDER_COUNTERS:
        row = counter_lookup.get(name)
        if row and (_num(row.get("max")) or 0) > 0:
            render_counters.append({"counter": name, "label": label, "mean": _num(row.get("mean")), "max": _num(row.get("max")), "last": _num(row.get("last"))})

    gc_events = [row for row in events if row.get("event") == "gc"]
    gc_durations = [value for row in gc_events if (value := _num(row.get("duration_ms"))) is not None]
    level_events = [row for row in events if row.get("event") in ("level_added", "level_removed")]
    churn = [row for row in events if row.get("event") == "actor_churn"]
    spawned_by_class: dict[str, int] = {}
    destroyed_by_class: dict[str, int] = {}
    for row in churn:
        for entry in row.get("top_spawned_classes", []) or []:
            spawned_by_class[str(entry.get("class"))] = spawned_by_class.get(str(entry.get("class")), 0) + int(_num(entry.get("count")) or 0)
        for entry in row.get("top_destroyed_classes", []) or []:
            destroyed_by_class[str(entry.get("class"))] = destroyed_by_class.get(str(entry.get("class")), 0) + int(_num(entry.get("count")) or 0)
    analysed_seconds = _num(capture_window.get("analyzed_seconds")) or (sum(frame_values) / 1000.0 if frame_values else None)

    def first_last(key: str) -> dict[str, Any] | None:
        values = _series(counters, key)
        return {"first": values[0], "last": values[-1], "min": min(values), "max": max(values)} if values else None

    memory = {"physical_bytes": first_last("memory_used_physical_bytes"), "available_physical_bytes": first_last("memory_available_physical_bytes"),
              "virtual_bytes": first_last("memory_used_virtual_bytes"), "uobjects": first_last("uobject_count"), "actors": first_last("actor_count"),
              "components": first_last("component_count"), "ticking_actors": first_last("ticking_actor_count"),
              "ticking_components": first_last("ticking_component_count"),
              "class_growth": (report.get("counters_summary") or {}).get("uobject_class_growth", [])[:8],
              "actor_class_growth": (report.get("counters_summary") or {}).get("actor_class_growth", [])[:8],
              "gc": {"count": len(gc_events), "total_ms": round(sum(gc_durations), 2) if gc_durations else None,
                     "max_ms": round(max(gc_durations), 2) if gc_durations else None,
                     "at_seconds": [round(t - origin, 1) for row in gc_events if (t := _to_trace(row, alignment)) is not None][:20]}}
    loading = {key: first_last(key) for key in ("async_packages_loading", "streaming_levels_loading", "shader_jobs_remaining", "asset_compiles_remaining",
                                               "pso_precache_requests_active", "streaming_over_budget_bytes", "streaming_pool_bytes",
                                               "gpu_streaming_texture_bytes", "gpu_nonstreaming_texture_bytes", "process_cpu_percent")}
    level_names: dict[str, int] = {}
    for row in level_events:
        name = f"{row.get('event')}: {str(row.get('level') or '').rsplit('/', 1)[-1]}"
        level_names[name] = level_names.get(name, 0) + 1
    collector_overhead = _distribution(_series(counters, "collector_overhead_ms"))
    network_section = _network_section(network_samples, events, native, report.get("network") or {}, thread_costs, len(frames))

    observations = _observations(frame_dist, bound_share, hitches, cause_counts, memory, loading, network_section, thread_dists,
                                 collector_overhead, capture_window, hitch_threshold_ms, frame_budget_ms, analysed_seconds)
    for thread in thread_costs:
        if thread.get("busy_ms_per_frame") is None or thread["thread"] not in ("GameThread", "RenderThread 0", "RHIThread") and "Graphics" not in thread["thread"]:
            continue
        work = [item for item in thread["top"] if not item["wait"]][:4]
        if work:
            observations.append({"severity": "info", "text": f"{thread['thread']} instrumented {thread['busy_ms_per_frame']:.2f} ms/frame; largest self time: "
                                 + ", ".join(f"{item['timer']} {item['ms_per_frame']:.2f}" for item in work) + " ms/frame."})
    return {
        "brief_version": BRIEF_VERSION,
        "context": _context(report, metadata, runtime_context, capture_window, alignment, analysed_seconds),
        "observations": observations,
        "coverage": _coverage(report.get("coverage") or {}, native, metadata),
        "frame_time": {"population": "native trace game frames inside the capture window, unfocused frames excluded" if frames else "runtime frame samples",
                       "distribution_ms": frame_dist, "hitch_threshold_ms": hitch_threshold_ms, "frame_budget_ms": frame_budget_ms,
                       "frames_over_budget_pct": round(100.0 * sum(value > frame_budget_ms for value in frame_values) / len(frame_values), 2) if frame_values else None,
                       "runtime_thread_ms": thread_dists, "bound_share_pct": bound_share,
                       "bound_semantics": "per runtime frame sample: frame_rate_cap if idle >= 25% of the frame; else the thread or GPU whose time is >= 80% of the busy frame; otherwise unattributed (waits, present, OS)."},
        "timeline": timeline,
        "hitches": {"count": len(hitches), "by_likely_cause": dict(sorted(cause_counts.items(), key=lambda item: -item[1])),
                    "rows": hitches[:MAX_HITCH_ROWS], "rows_truncated": len(hitches) > MAX_HITCH_ROWS},
        "thread_costs": thread_costs,
        "rendering": {"runtime": {key: thread_dists.get(key) for key in ("draw_calls", "primitives", "gpu_ms")}, "trace_counters": render_counters},
        "memory": memory,
        "loading_and_streaming": {**loading, "level_events": len(level_events),
                                  "top_level_events": [{"level": name, "count": count} for name, count in sorted(level_names.items(), key=lambda item: -item[1])[:10]],
                                  "map_loads": [row.get("world") for row in events if row.get("event") == "map_loaded"]},
        "gameplay": {"actor_spawned_total": sum(int(_num(row.get("spawned")) or 0) for row in churn),
                     "actor_destroyed_total": sum(int(_num(row.get("destroyed")) or 0) for row in churn),
                     "top_spawned_classes": sorted(spawned_by_class.items(), key=lambda item: -item[1])[:10],
                     "top_destroyed_classes": sorted(destroyed_by_class.items(), key=lambda item: -item[1])[:10]},
        "networking": network_section,
        "profiler_overhead": {"collector_overhead_ms": collector_overhead},
        "findings": [{"severity": row.get("severity"), "title": row.get("title"), "evidence": (row.get("evidence") or [None])[0],
                      "next_step": row.get("suggested_next_step")} for row in report.get("findings", []) if isinstance(row, dict)],
    }


def _context(report: dict[str, Any], metadata: dict[str, Any], runtime: dict[str, Any], window: dict[str, Any], alignment: dict[str, Any],
             analysed_seconds: float | None) -> dict[str, Any]:
    profile = metadata.get("capture_profile")
    profile_name = CAPTURE_PROFILES[int(profile)] if isinstance(profile, (int, float)) and 0 <= int(profile) < len(CAPTURE_PROFILES) else profile
    variables = runtime.get("console_variables") or {}
    return {
        "project": report.get("project"), "scenario": report.get("scenario"), "map": runtime.get("map_name"), "world_type": runtime.get("world_type"),
        "net_mode": runtime.get("net_mode"), "build": f"{metadata.get('build_configuration')} {metadata.get('build_target')}",
        "engine": metadata.get("engine_version"), "platform": runtime.get("platform"), "rhi": runtime.get("rhi"),
        "cpu": str(runtime.get("cpu_brand") or "").strip(), "gpu": runtime.get("gpu_brand"), "hardware": runtime.get("hardware") or {},
        "viewport": [runtime.get("viewport_width"), runtime.get("viewport_height")], "frame_pacing": runtime.get("frame_pacing") or {},
        "console_variables": variables, "capture_profile": profile_name, "active_channels": metadata.get("active_channels") or [],
        "capture_seconds": metadata.get("duration_seconds"), "analysed_seconds": analysed_seconds, "capture_window": window,
        "time_alignment": alignment, "keep_full_speed_when_unfocused": (metadata.get("settings") or {}).get("keep_full_speed_when_unfocused"),
    }


COVERAGE_HINTS = {
    "context_switches": "run the editor or game elevated (administrator); Windows supplies these through ETW",
    "stack_samples": "run elevated; symbol resolution in the extractor needs UE 5.8",
    "allocations": "relaunch with -trace=memory (Memory Leak profile); allocation tracing cannot start late",
    "memory_tags": "relaunch with -trace=memory,memtag (Memory Leak profile)",
    "network": "no NetDriver traffic: standalone, or no clients connected",
    "objects": "object snapshots need the UE 5.8 extractor",
    "tasks": "add the task channel (Standard has it)", "file_activity": "add the file channel (Standard has it)",
    "load_time": "add the loadtime channel (Standard has it)", "logs": "add the log channel (Standard has it)",
}


def _coverage(coverage: dict[str, Any], native: dict[str, Any], metadata: dict[str, Any]) -> dict[str, Any]:
    providers = coverage.get("providers") or {}
    present = sorted(name for name, row in providers.items() if isinstance(row, dict) and row.get("status") == "available")
    gaps = [{"provider": name, "status": row.get("status"), "how_to_get_it": COVERAGE_HINTS.get(name, "")}
            for name, row in sorted(providers.items()) if isinstance(row, dict) and row.get("status") in ("missing", "empty", "not_requested")]
    limits = []
    if native.get("timing_events_truncated"):
        limits.append(f"timing events stopped at {native.get('timing_events_limit')} rows; later slow frames have no retained scopes")
    if native.get("hitch_windows_truncated"):
        limits.append(f"only the {native.get('hitch_windows_limit')} slowest frames kept their scopes ({native.get('hitch_windows_seen')} slow frames)")
    if (metadata.get("dropped_event_rows") or 0) > 0:
        limits.append(f"{metadata.get('dropped_event_rows')} engine event rows dropped at the row cap")
    if not native:
        limits.append("native trace extraction did not run; frame data comes from runtime samples only")
    return {"present": present, "gaps": gaps, "limits": limits}


def _observations(frame_dist, bound_share, hitches, cause_counts, memory, loading, network, thread_dists, overhead, window,
                  hitch_ms, budget_ms, analysed_seconds) -> list[dict[str, Any]]:
    """Ranked plain statements. Each says what was measured and, when it is an inference, the evidence."""
    notes: list[tuple[int, str, str]] = []
    if frame_dist:
        fps = 1000.0 / frame_dist["mean"] if frame_dist["mean"] else None
        notes.append((0, "info", f"Frame time mean {frame_dist['mean']:.1f} ms ({fps:.0f} fps), p95 {frame_dist['p95']:.1f}, p99 {frame_dist['p99']:.1f}, "
                                 f"max {frame_dist['max']:.1f} ms over {frame_dist['count']:,} frames"
                                 + (f" ({analysed_seconds:.1f} s analysed)." if analysed_seconds else ".")))
    if bound_share:
        top = [f"{key.replace('_', ' ')} {value:.0f}%" for key, value in list(bound_share.items())[:4]]
        notes.append((1, "info", "Frames bound by: " + ", ".join(top) + " (runtime thread times; see bound_semantics)."))
    if hitches:
        rate = len(hitches) / (analysed_seconds / 60.0) if analysed_seconds else None
        causes = ", ".join(f"{count} {key.replace('_', ' ')}" for key, count in sorted(cause_counts.items(), key=lambda item: -item[1]))
        notes.append((0, "warning" if len(hitches) >= 3 else "info",
                      f"{len(hitches)} hitches >= {hitch_ms:.0f} ms" + (f" ({rate:.1f}/min)" if rate else "") + f"; likely causes: {causes}."))
        shader = cause_counts.get("shader_compilation", 0)
        if shader:
            notes.append((1, "info", f"{shader} hitch(es) are editor shader compilation; they do not happen in a cooked build, but PSO precaching should be checked there."))
        overhead_hitches = cause_counts.get("profiler_overhead", 0)
        if overhead_hitches:
            notes.append((1, "info", f"{overhead_hitches} hitch(es) come from PerfSentinel's own collectors; discount them."))
    gc = memory.get("gc") or {}
    if gc.get("count"):
        notes.append((2, "warning" if (gc.get("max_ms") or 0) >= hitch_ms else "info",
                      f"{gc['count']} garbage collection(s), longest {gc.get('max_ms')} ms, total {gc.get('total_ms')} ms."))
    physical = memory.get("physical_bytes")
    if physical and physical["max"] - physical["first"] > 200 * 1024 * 1024:
        notes.append((1, "warning", f"Process memory grew {(physical['last'] - physical['first']) / 1048576:,.0f} MB (peak {(physical['max']) / 1048576:,.0f} MB)."))
    available = memory.get("available_physical_bytes")
    if available and available["min"] < 4 * 1024 ** 3:
        notes.append((1, "warning", f"System memory ran low: {available['min'] / 1073741824:.1f} GB available at the lowest point."))
    over = loading.get("streaming_over_budget_bytes")
    if over and over["max"] > 0:
        notes.append((1, "warning", f"Texture/mesh streaming pool over budget by up to {over['max'] / 1048576:,.0f} MB (lower mips than wanted, streaming churn)."))
    for key, label in (("shader_jobs_remaining", "shader jobs"), ("asset_compiles_remaining", "asset compiles"), ("async_packages_loading", "async packages")):
        value = loading.get(key)
        if value and value["max"] > 0:
            notes.append((3, "info", f"Up to {value['max']:,.0f} {label} were pending during the capture."))
    cpu = loading.get("process_cpu_percent")
    if cpu and cpu["max"] >= 90:
        notes.append((1, "warning", f"Process CPU reached {cpu['max']:.0f}% of all cores."))
    draws = thread_dists.get("draw_calls")
    if draws and draws["p95"] and draws["p95"] > 4000:
        notes.append((2, "warning", f"Draw calls p95 {draws['p95']:,.0f} (max {draws['max']:,.0f})."))
    for row in network.get("connections", []):
        stall = row.get("seconds_since_last_receive_max")
        if stall is not None and stall >= 1.0:
            notes.append((0, "error" if stall >= 5 else "warning", f"{row['connection']}: no packet received for {stall:.1f} s at worst (stall toward timeout)."))
        rtt = row.get("rtt_ms") or {}
        if rtt.get("p95") and rtt["p95"] >= 150:
            notes.append((1, "warning", f"{row['connection']}: RTT p95 {rtt['p95']:.0f} ms (max {rtt['max']:.0f})."))
        loss = max(value for value in (row.get("in_loss_pct_max"), row.get("out_loss_pct_max"), 0.0) if value is not None)
        if loss >= 1.0:
            notes.append((1, "warning", f"{row['connection']}: packet loss up to {loss:.1f}% in a sample interval."))
        if row.get("not_net_ready_samples"):
            notes.append((1, "warning", f"{row['connection']}: saturated (not net-ready) in {row['not_net_ready_samples']} of {row['samples']} samples."))
        if (row.get("reliable_backlog_max") or 0) >= 64:
            notes.append((1, "warning", f"{row['connection']}: reliable backlog reached {row['reliable_backlog_max']:.0f} bunches."))
    for event in network.get("events", []):
        if event.get("event") in ("network_failure", "travel_failure"):
            notes.append((0, "error", f"{event.get('event').replace('_', ' ')}: {event.get('failure')} — {event.get('message', '')}"))
    if network.get("disconnects_observed"):
        notes.append((1, "warning", f"{network['disconnects_observed']} connection(s) disconnected during the capture."))
    if overhead and overhead.get("max", 0) >= 5:
        notes.append((3, "info", f"PerfSentinel counter collection took up to {overhead['max']:.1f} ms on a sampled frame (mean {overhead['mean']:.1f} ms)."))
    if window and (window.get("frames_unfocused") or window.get("frames_before_capture")):
        notes.append((3, "info", f"Left out of frame statistics: {window.get('frames_before_capture', 0)} frames before the capture, "
                                 f"{window.get('frames_unfocused', 0)} unfocused frames ({window.get('unfocused_seconds', 0)} s)."))
    order = {"error": 0, "warning": 1, "info": 2}
    notes.sort(key=lambda item: (item[0], order.get(item[1], 3)))
    return [{"severity": severity, "text": text} for _, severity, text in notes]


def render_brief_markdown(brief: dict[str, Any]) -> str:
    context = brief["context"]
    lines: list[str] = []
    add = lines.append
    add(f"# PerfSentinel brief: {context.get('project') or '-'} / {context.get('map') or '-'} / {context.get('scenario') or '-'}")
    add("")
    add("Read this first. Times are seconds from capture start. Numbers are measured unless a line says \"likely\", which marks an inference with its evidence. "
        "llm_brief.json has the same content as data; evidence.sqlite has every row.")
    add("")
    add("## Key observations")
    add("")
    for note in brief["observations"] or [{"severity": "info", "text": "Nothing notable was measured."}]:
        add(f"- [{note['severity']}] {note['text']}")
    add("")

    add("## Capture")
    add("")
    hardware = context.get("hardware") or {}
    add(f"- {context.get('build')} · UE {context.get('engine')} · {context.get('platform')} {context.get('rhi')} · world {context.get('world_type')} · net mode {context.get('net_mode') or '-'}")
    add(f"- CPU {context.get('cpu') or '-'} ({hardware.get('physical_cores', '-')} cores / {hardware.get('logical_cores', '-')} threads), "
        f"RAM {hardware.get('total_physical_memory_gb', '-')} GB, GPU {context.get('gpu') or '-'} "
        f"({_mb(hardware.get('gpu_dedicated_video_memory_bytes'))} VRAM, driver {hardware.get('gpu_driver_version', '-')}), OS {hardware.get('os_version', '-')}")
    viewport = context.get("viewport") or []
    variables = context.get("console_variables") or {}
    pacing = context.get("frame_pacing") or {}
    interesting = ", ".join(f"{key}={value}" for key, value in variables.items()
                            if key in ("t.MaxFPS", "r.VSync", "r.VSyncEditor", "r.ScreenPercentage", "r.DynamicRes.OperationMode", "r.AntiAliasingMethod",
                                       "r.NGX.DLSS.Enable", "r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.RayTracing", "r.Shadow.Virtual.Enable",
                                       "r.Nanite", "sg.ShadowQuality", "sg.GlobalIlluminationQuality", "net.Iris.UseIrisReplication"))
    add(f"- Viewport {viewport[0] if viewport else '-'}x{viewport[1] if len(viewport) > 1 else '-'}; frame pacing: max tick {pacing.get('max_tick_rate_fps', '-')}, "
        f"smoothing {pacing.get('smooth_frame_rate', '-')}, fixed {pacing.get('fixed_frame_rate', '-')}; {interesting or 'render settings not recorded (older capture)'}")
    window = context.get("capture_window") or {}
    add(f"- Profile {context.get('capture_profile')}; captured {_ms(context.get('capture_seconds'))} s, analysed {_ms(context.get('analysed_seconds'))} s "
        f"({window.get('frames_analyzed', '-')} of {window.get('frames_total', '-')} frames; {window.get('frames_before_capture', 0)} before capture, "
        f"{window.get('frames_unfocused', 0)} unfocused left out)")
    alignment = context.get("time_alignment") or {}
    add(f"- Sidecar ↔ trace time: {alignment.get('status')}" + (f" (±{alignment.get('uncertainty_ms', 0):.0f} ms)" if alignment.get("status") != "unavailable" else f" — {alignment.get('reason', '')}"))
    add("")

    coverage = brief["coverage"]
    add("## Data coverage")
    add("")
    add(f"- Present: {', '.join(coverage['present']) or 'none'}")
    for gap in coverage["gaps"]:
        add(f"- Missing {gap['provider']} ({gap['status']}){': ' + gap['how_to_get_it'] if gap['how_to_get_it'] else ''}")
    for limit in coverage["limits"]:
        add(f"- Limit: {limit}")
    add("")

    frame = brief["frame_time"]
    add("## Frame time")
    add("")
    add(f"Population: {frame['population']}. Budget {frame['frame_budget_ms']:.2f} ms, hitch ≥ {frame['hitch_threshold_ms']:.0f} ms"
        + (f"; {frame['frames_over_budget_pct']}% of frames over budget." if frame.get("frames_over_budget_pct") is not None else "."))
    add("")
    add("| Measure | mean | p50 | p95 | p99 | max | samples |")
    add("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    rows = [("Frame (trace)", frame.get("distribution_ms"))] + [(label, frame["runtime_thread_ms"].get(key)) for key, label in (
        ("game_thread_ms", "Game thread ms"), ("render_thread_ms", "Render thread ms"), ("rhi_thread_ms", "RHI thread ms"), ("gpu_ms", "GPU ms"),
        ("idle_ms", "Idle (frame cap) ms"), ("draw_calls", "Draw calls"), ("primitives", "Primitives"))]
    for label, dist in rows:
        if dist:
            digits = 0 if label in ("Draw calls", "Primitives") else 1
            add(f"| {label} | {_ms(dist['mean'], digits)} | {_ms(dist['p50'], digits)} | {_ms(dist['p95'], digits)} | {_ms(dist['p99'], digits)} | {_ms(dist['max'], digits)} | {dist['count']:,} |")
    if frame.get("bound_share_pct"):
        add("")
        add("Bound by (share of runtime frame samples): " + ", ".join(f"{key.replace('_', ' ')} {value}%" for key, value in frame["bound_share_pct"].items()))
    add("")

    if brief["timeline"]:
        add("## Timeline")
        add("")
        add("| t (s) | frames | mean ms | p95 ms | max ms | hitches | unfocused s | events |")
        add("| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |")
        for row in brief["timeline"]:
            add(f"| {row['from_s']:.0f}–{row['to_s']:.0f} | {row['frames']} | {_ms(row['mean_ms'])} | {_ms(row['p95_ms'])} | {_ms(row['max_ms'])} | {row['hitches']} | {row['unfocused_s'] or ''} | {_cell(row['events'])} |")
        add("")

    hitches = brief["hitches"]
    add(f"## Hitches ({hitches['count']})")
    add("")
    if hitches["rows"]:
        add("Likely causes: " + ", ".join(f"{key.replace('_', ' ')} {count}" for key, count in hitches["by_likely_cause"].items()) + ".")
        add("")
        add("| t (s) | ms | likely cause (basis) | game thread top self time | other threads | GT/RT/RHI/GPU ms | nearby |")
        add("| ---: | ---: | --- | --- | --- | --- | --- |")
        for row in hitches["rows"]:
            game = "; ".join(f"{item['timer']} {item['self_ms']:.1f}" for item in row["game_thread_top"]) or ("(no scopes retained)" if row["scope_data"] == "not_retained" else "-")
            other = "; ".join(f"{item['thread']}: {item['timer']} {item['self_ms']:.1f}" for item in row["other_threads_top"]) or "-"
            threads = row.get("runtime_thread_ms") or {}
            thread_text = "/".join(_ms(threads.get(key), 0) for key in ("game_thread_ms", "render_thread_ms", "rhi_thread_ms", "gpu_ms")) if threads else "-"
            add(f"| {row['t_seconds']:.2f} | {row['duration_ms']:.0f} | {_cell(row['cause_text'])} ({_cell(row['cause_basis'])}) | {_cell(game)} | {_cell(other)} | {thread_text} | {_cell(', '.join(row['nearby_events']))} |")
        if hitches["rows_truncated"]:
            add("")
            add(f"Showing {MAX_HITCH_ROWS} of {hitches['count']}; llm_brief.json and evidence.sqlite (game_frames, timing_events) have the rest.")
    else:
        add("No frame reached the hitch threshold.")
    add("")

    if brief["thread_costs"]:
        add("## Where frame time goes (steady state, per thread)")
        add("")
        add("Exclusive (self) time per analysed frame. Rows marked (wait) are a thread blocked on another thread or the GPU.")
        add("")
        for thread in brief["thread_costs"]:
            busy = thread.get("busy_ms_per_frame")
            add(f"- **{thread['thread']}**" + (f" — instrumented {_ms(busy, 2)} ms/frame" if busy is not None else "") + ": "
                + "; ".join(f"{item['timer']}{' (wait)' if item['wait'] else ''} {item['ms_per_frame']:.2f}" for item in thread["top"]))
        add("")

    rendering = brief["rendering"]
    if rendering["trace_counters"]:
        add("## Rendering counters (trace, capture window)")
        add("")
        add("| Counter | mean | max |")
        add("| --- | ---: | ---: |")
        for row in rendering["trace_counters"]:
            add(f"| {row['label']} ({row['counter']}) | {_ms(row['mean'], 0)} | {_ms(row['max'], 0)} |")
        add("")

    memory = brief["memory"]
    add("## Memory and objects")
    add("")
    physical = memory.get("physical_bytes")
    if physical:
        add(f"- Process physical memory {_mb(physical['first'])} → {_mb(physical['last'])} (peak {_mb(physical['max'])}); "
            f"system available min {_mb((memory.get('available_physical_bytes') or {}).get('min'))}")
    for key, label in (("uobjects", "UObjects"), ("actors", "Actors"), ("components", "Components"), ("ticking_actors", "Ticking actors"), ("ticking_components", "Ticking components")):
        values = memory.get(key)
        if values:
            add(f"- {label} {_int(values['first'])} → {_int(values['last'])} (max {_int(values['max'])})")
    gc = memory.get("gc") or {}
    if gc.get("count"):
        add(f"- GC: {gc['count']} collections, longest {gc.get('max_ms')} ms, at t = {', '.join(str(value) for value in gc.get('at_seconds', []))}")
    if memory.get("class_growth"):
        add("- Growing UObject classes: " + ", ".join(f"{row.get('class')} +{row.get('delta')}" for row in memory["class_growth"] if isinstance(row, dict)))
    add("")

    loading = brief["loading_and_streaming"]
    add("## Loading, streaming and compiles")
    add("")
    if not any(loading.get(key) for key in ("async_packages_loading", "streaming_levels_loading", "shader_jobs_remaining", "streaming_over_budget_bytes")) \
            and not loading.get("level_events") and not loading.get("map_loads"):
        add("- No loading, streaming or compile-queue samples (capture recorded before these counters existed).")
    for key, label in (("async_packages_loading", "Async packages loading"), ("streaming_levels_loading", "Streaming levels loading"),
                       ("shader_jobs_remaining", "Shader jobs pending"), ("asset_compiles_remaining", "Asset compiles pending"),
                       ("pso_precache_requests_active", "PSO precache requests")):
        values = loading.get(key)
        if values:
            add(f"- {label}: max {_int(values['max'])}")
    over = loading.get("streaming_over_budget_bytes")
    if over:
        add(f"- Streaming pool over budget: max {_mb(over['max'])} (pool {_mb((loading.get('streaming_pool_bytes') or {}).get('last'))})")
    if loading.get("level_events"):
        add(f"- Level events: {loading['level_events']} — " + ", ".join(f"{row['level']}" + (f" ×{row['count']}" if row['count'] > 1 else "") for row in loading["top_level_events"]))
    if loading.get("map_loads"):
        add(f"- Map loads: {', '.join(str(value) for value in loading['map_loads'])}")
    add("")

    gameplay = brief["gameplay"]
    if gameplay["actor_spawned_total"] or gameplay["actor_destroyed_total"]:
        add("## Gameplay churn")
        add("")
        add(f"- Actors spawned {gameplay['actor_spawned_total']:,}, destroyed {gameplay['actor_destroyed_total']:,}")
        if gameplay["top_spawned_classes"]:
            add("- Most spawned: " + ", ".join(f"{name} {count}" for name, count in gameplay["top_spawned_classes"]))
        if gameplay["top_destroyed_classes"]:
            add("- Most destroyed: " + ", ".join(f"{name} {count}" for name, count in gameplay["top_destroyed_classes"]))
        add("")

    network = brief["networking"]
    add("## Networking")
    add("")
    if not network["connections"] and not network["drivers"]:
        add(f"- No NetDriver connections were sampled (status: {network.get('status') or 'not_applicable'}). Standalone play has no gameplay traffic; "
            "use a listen server with clients, or separate server/client processes, to measure it.")
    for row in network["drivers"]:
        add(f"- Driver {row['driver']}: {'server' if row.get('is_server') else 'client'}, Iris {row.get('uses_iris')}, server tick cap {row.get('net_server_max_tick_rate')}, "
            f"RPC/s {_ms(row.get('rpcs_per_second'))}, reliable bunches/s {_ms(row.get('reliable_bunches_per_second'))}, "
            f"replicated objects max {_int(row.get('network_objects_max'))} (active {_int(row.get('active_network_objects_max'))})"
            + (f", packet simulation {row['packet_simulation']}" if row.get("packet_simulation") else ""))
    if network["connections"]:
        add("")
        add("| Connection | samples | RTT p50/p95/max ms | jitter p95 | in KiB/s p95 | out KiB/s p95 | loss max in/out % | not net-ready | reliable backlog max | max s since receive | dormant max |")
        add("| --- | ---: | --- | ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: |")
        for row in network["connections"]:
            rtt = row.get("rtt_ms") or {}
            add(f"| {_cell(row['connection'])} | {row['samples']} | {_ms(rtt.get('p50'), 0)}/{_ms(rtt.get('p95'), 0)}/{_ms(rtt.get('max'), 0)} | {_ms((row.get('jitter_ms') or {}).get('p95'))} | "
                f"{_ms((row.get('in_kib_s') or {}).get('p95'))} | {_ms((row.get('out_kib_s') or {}).get('p95'))} | {_ms(row.get('in_loss_pct_max'))}/{_ms(row.get('out_loss_pct_max'))} | "
                f"{row.get('not_net_ready_samples')} | {_int(row.get('reliable_backlog_max'))} | {_ms(row.get('seconds_since_last_receive_max'))} | {_int(row.get('dormant_actors_max'))} |")
    for event in network["events"]:
        add(f"- {event.get('event')}: {event.get('failure') or ''} {event.get('message') or ''} (players {event.get('players', '-')})")
    if network["cpu_scopes"]:
        add("- Network CPU: " + "; ".join(f"{row['thread']}: {row['timer']} {row['ms_per_frame']:.2f} ms/frame" for row in network["cpu_scopes"]))
    if network["trace_net_counters"]:
        add("- Net stat counters (max): " + ", ".join(f"{row.get('name')} {_ms(row.get('max'), 0)}" for row in network["trace_net_counters"][:12]))
    if network["content"]:
        add("- Largest replicated content (NetTrace exclusive bits): " + "; ".join(
            f"{row.get('name')}{' ' + str(row.get('direction')) if row.get('direction') else ''} {_int(row.get('exclusive_bits'))} bits" for row in network["content"]))
    add("")

    if brief["findings"]:
        add("## Analyzer findings")
        add("")
        for row in brief["findings"]:
            add(f"- [{row['severity']}] {row['title']}: {row['evidence'] or ''} Next: {row['next_step'] or ''}")
        add("")

    add("## Dig further")
    add("")
    add("- evidence.sqlite: game_frames (analysed frames), timing_events (scopes in slow frames), timing_scope_totals, counters, bookmarks, logs, network_* tables.")
    add("- native_evidence*.json: thread_scope_totals (all rows per thread), analysis_window. Sidecars: *_frames.ndjson (every frame), *_runtime_counters.ndjson (1 s), *_events.ndjson (engine events), *_network_samples.ndjson (1 s).")
    add("- Unreal Insights: open the .utrace; PerfSentinel bookmarks mark capture start/stop, focus changes, viewport resizes, map loads, network failures and logins.")
    add("")
    return "\n".join(lines)


def write_llm_brief(out_dir: Path, **inputs: Any) -> tuple[Path, Path]:
    brief = build_brief(**inputs)
    json_path = out_dir / "llm_brief.json"
    md_path = out_dir / "llm_brief.md"
    json_path.write_text(json.dumps(brief, indent=1, default=str), encoding="utf-8")
    md_path.write_text(render_brief_markdown(brief), encoding="utf-8")
    return md_path, json_path
