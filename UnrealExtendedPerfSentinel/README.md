# Extended Perf Sentinel

Capture an Unreal Insights trace together with runtime sidecars, then analyze it into reports and an indexed evidence database. Agents start with `agent_overview.json`, inspect coverage, and resolve metric/evidence records through the native `UnrealExtendedPerfSentinelEditor.PerfSentinelAgentToolset`. Editor tools live in the PerfSentinel menu; packaged Development builds also expose console commands.

The native investigation skill is discoverable through Unreal's `AgentSkillToolset`. It teaches the overview → coverage → metrics → evidence → comparison workflow, including missing-data and clock rules. See [Agent performance workflow](Docs/AgentPerformanceWorkflow.md) for the contract, tool API, instrumentation, limits, and implementation checklist.

## Capture a useful play session

1. Use separate server/client processes for multiplayer investigations. Each process records its own capture; one client's capture cannot describe every server connection.
2. Select LightweightBaseline for routine timing and gameplay-network evidence without expensive object inventories, Standard for routine collection with configured inventories, Multiplayer for a focused network investigation, or ComprehensiveGameplay to include loading, UI, animation, and task evidence too. Increase Network Trace Verbosity only when finer content detail is needed.
3. Start capture, play the scenario, and mark important moments. Stop before analyzing.

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
| Frame performance | Frame distributions, frame budgets, hitches, CPU/render/RHI/GPU timing where available, scopes around affected frames |
| Steady CPU/GPU work | Whole-session named-scope aggregates; nested inclusive totals must not be added together as elapsed frame time |
| Memory and lifetime | Runtime physical/virtual memory, GC observations, object/class counts; allocation/tag/callstack detail requires its providers |
| Loading and scheduling | Available load/file/task/platform providers, with requested-but-missing coverage reported |
| Connection health | Sampled native gameplay NetDriver/connection counters: bytes and packets per interval, RTT where measured, loss indicators, reliable outstanding bunches, channel counts, bandwidth readiness, effective driver settings |
| Network payload | Packet counts/sizes/statuses, throughput bins, named actor/property/RPC trace scopes, inclusive/exclusive content bits, and activity near hitches |
| Subsystem gauges | Render/RHI texture and buffer memory, process memory, async-loading and streaming counts; optional bounded physics, animation, AI, and audio workload counts |
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
- Keep a baseline from a known good build and review coverage before trusting a regression result.
