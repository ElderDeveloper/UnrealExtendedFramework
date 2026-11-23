# Extended Perf Sentinel

Capture an Unreal Insights trace together with runtime sidecars, then analyze it into reports and an indexed evidence database. Agents and people start with `llm_brief.md` in the report folder: a short, ordered summary of the capture (observations, frame time and which thread bound it, timeline, hitch causes, per-thread costs, rendering, memory, loading, networking). For detail, agents inspect `agent_overview.json` and coverage, and resolve metric/evidence records through the native `UnrealExtendedPerfSentinelEditor.PerfSentinelAgentToolset`. Editor tools live in the PerfSentinel menu; packaged Development builds also expose console commands.

The native investigation skill is discoverable through Unreal's `AgentSkillToolset`. It teaches the overview → coverage → metrics → evidence → comparison workflow, including missing-data and clock rules. See [Agent performance workflow](Docs/AgentPerformanceWorkflow.md) for the contract, tool API, instrumentation, limits, and implementation checklist.

## Engine versions

The plugin builds on UE 5.6 and 5.8. Some pieces need 5.8: the agent toolset and skill (Unreal's ToolsetRegistry), the `raw_ping_ms` network field, and the analyzer's object snapshots and stack samples (TraceServices' object provider and stack frames). On older engines those are left out: the agent toolset is not built, and the report marks `objects` and `stack_samples` coverage `not_applicable` rather than empty.

## Capture a useful play session

1. Use separate server/client processes for multiplayer investigations. Each process records its own capture; one client's capture cannot describe every server connection.
2. Standard collects everything that can be switched on while the process runs: CPU, GPU, frames, counters, stats, logs, tasks, loading, files, asset metadata, Slate, animation, network, context switches and stack sampling. Context switches and stack sampling come from the OS and only arrive when the process is elevated. Memory allocations must start with the process, so they stay in the MemoryLeak profile. LightweightBaseline and the focused profiles record less, which costs less. Increase Network Trace Verbosity only when finer content detail is needed.
3. Start capture, play the scenario, and mark important moments. A capture runs until it is stopped; Auto Stop Capture limits it to a fixed duration. Stop before analyzing.

Capture only collects. Spike detection, spike screenshots and stats harvesting are off by default (Record Spike Events turns them back on); the analysis finds slow frames afterwards. Frames recorded while another application has focus are marked in the trace (`PerfSentinel.Unfocused.Begin`/`.End` bookmarks and a `PerfSentinel Unfocused` region) and in the frame samples (`app_foreground`, plus `idle_ms` for time the engine slept to hold a frame-rate limit), and the analysis leaves them out of frame statistics. While capturing, Keep Full Speed When Unfocused switches off the editor's "Use Less CPU when in Background" and `t.IdleWhenNotForeground`, then restores them when the capture stops. The trace file also opens with the trace system's history from before the capture; frames before the `PerfSentinel.CaptureStart` bookmark are left out too.

```text
PerfSentinel.SetCaptureProfile ComprehensiveGameplay
PerfSentinel.StartCapture HighLatencyRevive
PerfSentinel.Bookmark ReviveStarted
PerfSentinel.Bookmark ReviveDidNotComplete
PerfSentinel.StopCapture
PerfSentinel.AnalyzeLastTrace
PerfSentinel.AnalysisStatus
```

SetCaptureProfile changes the current process settings without saving project configuration. Project Settings → Extended Framework → PerfSentinel controls persistent settings. MemoryLeak and HitchDiagnosis advertise launch requirements; use their dedicated launch workflow when allocation callstacks or platform scheduling are needed.

## What the evidence measures

| Area | Evidence |
| --- | --- |
| Frame performance | Frame distributions, frame budgets, hitches with likely causes, per-frame game/render/RHI/GPU/idle times, draw calls and primitives, the thread that bound each frame, scopes around affected frames |
| Steady CPU/GPU work per thread | Exclusive scope time per analysed frame for the game, render and RHI threads, task workers and each GPU queue, limited to the capture window (`thread_scope_totals`) |
| Engine events | `*_events.ndjson`: GC with duration, levels added/removed, actor spawn/destroy churn per class, viewport resizes, map loads, focus changes, network and travel failures, player logins/logouts; mapped onto trace time through the capture-start bookmark |
| Capture context | Hardware (cores, RAM, GPU, VRAM, driver, OS), frame pacing, and the render, scalability, GC and network console variables that change what frame times mean |
| Steady CPU/GPU work | Whole-session named-scope aggregates; nested inclusive totals must not be added together as elapsed frame time |
| Memory and lifetime | Runtime physical/virtual memory, GC observations, object/class counts; allocation/tag/callstack detail requires its providers |
| Loading and scheduling | Available load/file/task/platform providers, with requested-but-missing coverage reported |
| Connection health | Sampled native gameplay NetDriver/connection counters: bytes and packets per interval, RTT and jitter, loss indicators, reliable outstanding bunches, channel counts, bandwidth readiness, seconds since the last packet was received or sent (stalls), dormant actors per connection, replicated object counts per driver, client rate limits, effective driver settings |
| Network payload | Packet counts/sizes/statuses, throughput bins, named actor/property/RPC trace scopes, inclusive/exclusive content bits, and activity near hitches |
| Subsystem gauges | Process CPU, render/RHI texture and buffer memory, VRAM, texture/mesh streaming pool size and over-budget bytes, process memory, async-loading and streaming counts, PSO precache requests, editor shader and asset compile queues; bounded physics, animation, AI, and audio workload counts |
| Instrumented gameplay/transports | Shared occurrence stages, application payload bytes, operation duration/retries/result, explicit clock observations, and custom metrics with optional real object/class attribution |
| Capture quality | Actual channel/verbosity coverage, collector catalog/overhead, sample/extraction limits, missing evidence, launch requirements, and baseline compatibility |

Connection sampling defaults to once per second and is independent of generic runtime counters. First samples establish baselines; reset/wrapped counters invalidate that interval. Sampling limits bound worlds, drivers, connections, and channels. Higher trace verbosity and heavy inventories can affect the observed workload.

## Network interpretation

- Directions are relative to the recorded process. Keep world, driver, connection, and game-instance identities separate.
- Runtime bandwidth is derived from connection lifetime-counter deltas. Trace throughput is a separate measurement; do not sum the two.
- UE 5.8 trace `Delivered` means a packet event was recorded; outgoing status is not proof of remote acknowledgement. Incoming sequence gaps can appear as zero-byte dropped placeholders at the next received packet's time.
- Trace packet timestamps do not measure RTT. Runtime RTT comes from native connection statistics when valid. No observation is unavailable, rather than a fabricated zero.
- QueuedBits is bandwidth-budget debt, not a count of queued payload bits. Reliable outstanding bunches are a separate measure of pressure.
- Outgoing sampled loss uses ACK/NAK-notified packets in the interval, excluding packets still in flight. It describes notifications resolved locally during that interval, rather than a cohort of packets sent during it.
- Content costs are named nested scopes. Exclusive bits avoid counting children twice; inclusive bits help drill down. A name alone does not prove that a scope is an RPC or a property.
- Native network evidence measures gameplay NetDriver traffic. EOS RTC/voice, lobby and authentication operations, HTTP/PlayFab requests, platform SDK traffic, relay encapsulation, and OS/NIC traffic need their own instrumentation or packet/transport diagnostics. `ReportTransportOperation` records measured integration payloads, duration, retries, and outcomes; wire bytes remain unknown unless explicitly measured.
- Native trace evidence covers recording time. A bounded recent history of explicit telemetry API events can also accompany the capture; it does not recover earlier native trace events. To attribute connection setup and initial replication, start before joining/spawning. Lost or unrecorded history cannot be reconstructed.
- Sampling can miss connections that open and close between ticks. Legacy reliable-bunch counts do not describe every Iris reliability queue. Detailed packet exports are bounded examples; consult truncation warnings before extrapolating them.

## Budgets and comparison

Networking budgets default to zero (disabled). Establish representative measurements, then set outgoing/incoming KiB/s, RTT, loss, reliable-backlog, and regression limits in settings. Enabled limits with missing measurements are reported as unevaluable and must not pass CI as healthy zeros. Compare the same map, role, capture profile, hardware, player count, and scenario.

The reports retain native evidence and structured tables for follow-up in Unreal Insights or SQLite. Schema v4 metric records declare unit, aggregation, time window, scope, source, and measurement status. Unknown values are `null`; measured zero requires an observation. Findings cite stable evidence IDs and declared diagnostic rules, with measured/attributed/correlated/hypothesis evidence levels. Network CSVs contain bounded exported detail; consult coverage and truncation fields before extrapolating rankings or samples. Empty captures and absent providers are visible.

## Practical workflow improvements

- Keep a small named scenario set: idle baseline, level transition, AI wave, interaction/revive, and worst multiplayer case.
- Capture server and client with the same scenario name and shared gameplay markers; record player counts and latency/loss conditions externally.
- Compare a normal capture with collector-heavy settings disabled to measure observer cost.
- Use MemoryLeak for a long lifetime investigation and short detailed network captures for costly RPC/property drill-downs.
- Analyze packaged traces on a development machine with Python and the matching editor/TraceServices installation. Packaged recording does not require the analyzer to run on the player's machine.
- When native extraction succeeds, the analyzer skips the Unreal Insights CSV export (it re-analysed the whole trace for timer totals the native evidence already has, windowed and per thread) unless spike windows need their per-spike timing CSVs. This halves analysis time and keeps long captures under the runner's timeout.
- Metric records are stored only in `evidence.sqlite` (`metric_records`); `findings.json` keeps a count. Every capped export in the native evidence lists rows seen, rows kept and whether it was truncated (`export_limits`, `hitch_windows`).
- Keep a baseline from a known good build and review coverage before trusting a regression result.
