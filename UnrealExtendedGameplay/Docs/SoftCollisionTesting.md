# Soft Collision testing

How to verify Soft Collision in the editor. The plan's Phase 0 checks, the network test matrix and the performance budgets all need a running engine; this is the procedure, and the tables at the end are where the results go.

## Setup

1. Enable the plugin and compile.
2. Add `UEGSoftCollisionComponent` to the player Character and to the AI Character Blueprints. Nothing else is required; a profile asset is optional (the class defaults apply).
3. Make sure push-model replication is on: `net.IsPushModelEnabled 1` (in `DefaultEngine.ini` under `[SystemSettings]`, or on the command line).
4. Useful console commands on every machine:

| Command | What it does |
| --- | --- |
| `eg.SoftCollision.Debug 1` | Draws issued pushes: green predicted, yellow server or local, red bumps |
| `eg.SoftCollision.Debug 2` | Also draws every registered shape, cyan for pawns this machine scans |
| `eg.SoftCollision.Debug 3` | Also logs rejected requests on both sides |
| `eg.SoftCollision.ShowGrid 1` | Draws the occupied grid cells |
| `stat EGSoftCollision` | Timings and counters: scanners, sleeping pawns, neighbour tests, pushes, requests sent, accepted, rejected, late, unanswered |
| `p.NetShowCorrections 1` | Engine: draws every movement correction |
| `eg.SoftCollision.ForceServerOnly 1` | Turns prediction off (set it on server and clients alike), as a baseline |
| `eg.SoftCollision.SafetyLevelOverride 0/1/2` | Server: forces a safety level for every pawn; -1 restores the profiles |
| `eg.SoftCollision.Debug.RejectEvery N` | Server: rejects every Nth request, to exercise the client's reject path |
| `eg.SoftCollision.Parallel 0` | Runs the scan inline, to compare against the worker-thread path |

Network emulation, from the editor's PIE network settings or the console: `Net PktLag=100`, `Net PktLagVariance=20`, `Net PktLoss=5`.

## Automation tests

Session Frontend, Automation tab, filter `UnrealExtendedGameplay.SoftCollision`, or:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests UnrealExtendedGameplay.SoftCollision; Quit" -unattended -nopause -nullrhi
```

| Test | Covers |
| --- | --- |
| `SoftCollision.Contacts` | Cylinder, box and rotated box contacts, height gate, coinciding centres |
| `SoftCollision.Strength` | Momentum share, response curves, idle pair, runner and idle pawn, filters, cap, weak pushes, decay |
| `SoftCollision.Quantize` | Round trips, stability after a round trip, sequence wrap, token bucket, angles |
| `SoftCollision.Grid` | Cells, neighbourhoods, move, remove, reset |
| `SoftCollision.Prediction` | Request batching and redundancy, acknowledgement across the wrap, rejection, timeouts, server duplicates |
| `SoftCollision.Modifiers` | Modifier stack: scales multiply, switches add up, removal |
| `SoftCollision.World` | Spawned Characters in a test world: overlapping pawns separate, a pawn that cannot be pushed stays put |

## Phase 0: engine behaviour

Each check names the assumption from the plan's Engine facts table and what to record. Use a listen server with one client, or a dedicated server with two clients, a flat map and two player Characters.

### 1. The server honours the client's start time

Server `eg.SoftCollision.SafetyLevelOverride 0`. No packet loss, `Net PktLag=100`. On the client, walk slowly into the other player and stand overlapping for a few seconds, then run into them.

- Expected: `Late Requests` stays at 0 and `p.NetShowCorrections` shows no corrections caused by pushes.
- If corrections appear with no late requests, the server is not applying the source from the client's start time the way the plan assumes. Record it, then repeat with level 1 and level 2.

### 2. Late requests

Same as 1 with `Net PktLoss=10`.

- Record late requests per minute and corrections per minute, once with the profile's Late Policy on Trim Elapsed and once on Reject.
- Keep the policy with fewer corrections as the default.

### 3. Move combining while pushed

Unreal Insights with the networking trace (`-trace=default,net`), or `stat net` on the client.

- Count `ServerMovePacked` calls per second while standing still, while walking, and while being pushed.
- Expected: higher while pushed, because moves are not combined while a root motion source is active. Record the numbers; they are the cost per pushing player.

### 4. Correction pairing with the custom source

Server `eg.SoftCollision.SafetyLevelOverride 0`, `Net PktLoss=10`. Push a player continuously for 30 seconds.

- Expected: after each correction the push continues smoothly. A push that restarts or doubles after a correction means `Matches` or `UpdateStateFrom` does not pair the sources; record it.

### 5. Rejected and unanswered pushes

Server `eg.SoftCollision.Debug.RejectEvery 3`.

- Expected: every third push is removed on the client, followed by at most one correction.
- If the rejected push comes back after the correction (the replay restored it from the saved moves), record it: the reject path then needs to also clear the source from the saved moves.

### 6. Request order

Debug level 3 on the server.

- Late requests with zero packet loss mean the request arrived after its move; record how often.
- Repeat checks 1 and 6 with Iris (`-UseIrisReplication=1`, or `net.Iris.UseIrisReplication 1` with Iris enabled in the project).

### 7. Simulated proxy cost

Network Insights, one player pushing continuously: record the bits per second spent on the Character's `RepRootMotion` on the other client. This is the per-pushing-player cost on every other client.

### 8. Move-data transport

In a project movement component set up as in `Prediction/EGSoftCollisionMoveData.h`, repeat checks 1, 2 and 5.

- Expected: `Requests Sent` stays at 0 (pushes ride in moves), `Late Requests` stays at 0 even with packet loss, and pushes cause no corrections at safety level 0.
- Combine it with turn in place's saved move and check that turning still works while being pushed.

### 9. Integration with the rest of the plugin

- Turn in place: stand idle in a crowd and turn the camera; turning must not pause while being pushed.
- Root Motion Move and Set Warp Target: lunge through a crowd; the lunge must reach its target (pushes are suppressed during gameplay root motion).
- `PushActorTimeDilation(0.2)` on an AI being pushed: the push lasts five times longer in world time.
- `Extended.Status.Overlay 1`: one Soft Collision line per world.

### 10. Timestamp resets

Keep a player pushing for at least 5 minutes, past the CharacterMovementComponent's timestamp reset.

- Expected: no rejected requests with reason Stale and no burst of corrections around the reset.

## Network test matrix

Run each scenario with lag 0, 100 and 250 ms and packet loss 0 and 5 percent, with Iris off and on. Compare against the same scenario with the component removed (the engine's blocking collision).

| Scenario | Setup |
| --- | --- |
| Walk into an idle player | Client A walks at walk speed into client B standing still |
| Head-on | A and B run at each other |
| Crowd | 30 AI wander in a 10 m circle around a client |
| Choke point | 4 clients and 10 AI push through a 2 m doorway |
| Listen host against client | The host's pawn and a client's pawn push each other |

Record per scenario: corrections per minute (soft and blocking), rejects, late requests, unanswered pushes, bandwidth per client.

## Performance

A dedicated server with 4 clients and AI Characters that wander randomly in a dense area. Capture with Unreal Insights (`-trace=default,task`) and `stat EGSoftCollision`.

| Pawns | Snapshot ms | Wait ms | Apply ms | Frame updates ms | Scan task ms (workers) | CMC total ms |
| --- | --- | --- | --- | --- | --- | --- |
| 100 | | | | | | |
| 500 | | | | | | |
| 1000 | | | | | | |

Budgets from the plan: under 0.5 ms game thread on the server at 1000 pawns for snapshot, wait, apply and frame updates together; under 0.1 ms on a client.

## Results

| Check | Date | Engine | Result | Notes |
| --- | --- | --- | --- | --- |
| 1. Start time | | | | |
| 2. Late requests | | | | |
| 3. Move combining | | | | |
| 4. Correction pairing | | | | |
| 5. Rejected pushes | | | | |
| 6. Request order | | | | |
| 7. Simulated proxy cost | | | | |
| 8. Move-data transport | | | | |
| 9. Plugin integration | | | | |
| 10. Timestamp resets | | | | |
