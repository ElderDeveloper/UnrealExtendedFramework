# Unreal Extended Atlassian

Native Jira and Confluence collaboration inside Unreal Engine 5.6 Editor. The unified **Backlot**
workspace provides Docs, Issues, Issue Detail, Sprint Board, Pins, Inbox, and annotated viewport
Capture without embedding a browser.

- **Engine:** Unreal Engine 5.6
- **Atlassian:** Cloud (`https://<site>.atlassian.net`) only
- **UI:** native Slate; the HTML parity reference is test input, never shipped UI
- **Scope:** editor targets only

## Backlot workflow

Open **Window → Atlassian → Jira Issues** or **Confluence Docs**. Both commands are compatibility
deep links into one Backlot tab; use its left navigation rail to switch between:

| Route | What it provides |
|---|---|
| **Docs** | Confluence page tree/search, eight document block presentations, structured editing, page operations, comments, and linked Jira work |
| **Issues** | Jira filters and views, inline status transitions, preview rail, create, edit, delete/Undo, activity, and comments |
| **Issue Detail** | Full description, editable properties, capture threads, activity, comments, and destructive operations |
| **Board** | Active-sprint columns, card CRUD, estimates, WIP state, transitions, ranking, and team load |
| **Pins** | Shared Unreal asset/level/Blueprint/page pins and collaborative threads |
| **Inbox** | Synthesized per-user Backlot notifications with read, dismiss, archive, and destination actions |
| **Capture** | Editor/PIE screenshot, PIN/BOX/BLUR annotations, Jira issue creation, attachment, context, and log upload |

The right rail is contextual. **Narrow Dock** hides the contextual sidebar and right rail and keeps
Backlot at a 560-pixel dock width beside the real Unreal viewport.

## Setup

1. Enable the plugin and restart the editor.
2. Open **Project Settings → Extended Framework → Extended Atlassian**.
3. Enter the **Atlassian Site URL**, for example `https://yourcompany.atlassian.net`.
4. Enter your Atlassian account e-mail and API token.
5. Click **Save Credentials**, then **Test Connection**.
6. Click **Refresh Lists from Atlassian**, then select the Jira project, board/sprint, and
   Confluence space from the discovered options.
7. For shared Pins, threaded presentation metadata, and Inbox cursors, set
   **Backlot Metadata Page ID** to a dedicated Confluence page that collaborators can edit.

The site/project/space/board configuration is project-owned and may be shared through
`Config/DefaultGame.ini`. Account e-mail and token are transient settings and are never written to
project configuration.

### Credentials

Per-user credentials are stored at:

```text
%LOCALAPPDATA%\UnrealExtendedAtlassian\credentials.ini
```

On Windows the token is protected with DPAPI and bound to the current Windows user. Copying that
file to another user or machine does not make the token usable. Treat the API token like a password:
software running as the same user can ask Windows to decrypt it.

Use **Clear Stored Credentials** to delete the local credential file. No token, e-mail, or site
credential is stored in the repository, fixture, screenshot baseline, or shared Backlot metadata.

## Data ownership and permissions

| Data | Authority |
|---|---|
| Issues, workflow, assignees, priorities, epics/parents, estimates, sprint, ranking | Jira / Jira Software |
| Issue descriptions, comments, and activity | Jira |
| Pages, hierarchy, versions, bodies, and comments | Confluence |
| Page ordering/review metadata, Pins, thread metadata, shared event cursors | Versioned Confluence content properties on the configured metadata page |
| Inbox read/archive state, route, rail, and compact preferences | Per-user Local App Data |
| Working Markdown | Existing project `Saved/Documents` working-copy store |
| API token and account e-mail | Per-user DPAPI credential store |

Backlot discovers capabilities and operation permissions from Atlassian. A missing permission
disables only the affected action and explains why; it does not hide cached readable content.
Conflicting versioned shared-metadata writes are reloaded and reconciled rather than silently
overwriting another collaborator.

## Capture

Press **Ctrl+Shift+B** on Windows/Linux or **Cmd+Shift+B** on macOS. The temporary compatibility
chord **Ctrl+Alt+B** is also registered for one migration release.

Capture prefers the PIE viewport, then the active editor viewport. The screenshot is taken before
the composer appears. It can include:

- level, world mode, camera transform, and selected actors;
- Unreal version, platform, RHI, GPU, and source-control revision;
- configured editor-log tail;
- full-resolution PIN, BOX, and BLUR annotations.

**Create & keep working** creates the Jira issue, uploads the annotated PNG and configured context,
routes Backlot to Issues, and selects the new issue. If issue creation fails, entered text and
annotations remain. If the issue succeeds but an attachment fails, the issue remains valid and the
result names the failed attachment.

## Interaction and recovery guarantees

- Network, validation, permission, rate-limit, conflict, and server errors preserve typed drafts.
- Optimistic mutations roll back ordering, counts, selection, and values if the remote commit fails.
- Destructive operations require confirmation. Operations that advertise Undo use a delayed commit;
  closing the editor safely resolves or cancels the pending operation according to its policy.
- Cached data remains visible during refresh/offline periods and is marked stale.
- Loading/error states do not replace the entire workspace or clear selections.
- Routine polling and Inbox refreshes remain non-interrupting during PIE/build work.
- Atlassian rate-limit `Retry-After` is honored; transient retries use the configured retry limit.

## Configuration reference

Everything is under **Project Settings → Extended Framework → Extended Atlassian**.

| Section | Important settings |
|---|---|
| **Connection / Credentials** | Site URL, per-user e-mail/token, save/test/clear actions |
| **Discovery** | Board, sprint, editable-field, estimation, and rank capability results |
| **Jira** | Project, bug project, default issue type/priority, JQL presets |
| **Board** | Board ID, active/explicit sprint, presentation-column mappings, WIP limit |
| **Confluence** | Primary space, visible spaces, personal-space inclusion, shared metadata page ID |
| **Polling** | Optional polling; minimum interval is 60 seconds |
| **Bug Report** | Screenshot, log tail, selection, camera, and source-control context |
| **Network** | Request timeout and retry count |

## Limits and expected degradation

- Atlassian Cloud only; Jira/Confluence Server and Data Center are unsupported.
- No inbound webhooks are hosted by the editor. Updates use refresh/polling and versioned cursors.
- Confluence/Jira rich bodies are normalized through the plugin document conversion layer. Unknown
  macros remain safe placeholders so round trips do not silently discard their source.
- The Issues query result and document caches have bounded limits. Backlot reports a truncated result
  rather than implying the list is complete.
- Priority is omitted when no priority is selected, supporting projects where the field is absent.
- Jira labels replace spaces with hyphens because Jira rejects labels containing spaces.
- Pins and shared thread features require an editable Backlot metadata page.
- Existing browser-opening actions remain available for Atlassian content that cannot be represented
  losslessly in the parity surface.

## Troubleshooting

| Symptom | Check |
|---|---|
| Unconfigured/connect screen | Save credentials, test the connection, and verify the HTTPS site URL |
| Empty project/space/board lists | Run **Refresh Lists from Atlassian** and inspect `LogExtendedAtlassian` |
| Board unavailable | Select a Jira Software board and active/explicit sprint; review Discovery status |
| Pins are read-only | Set a metadata page ID and grant the current account edit permission |
| `401` / token expired | Create a new API token, save credentials, and test again |
| `403` / disabled action | Confirm Jira/Confluence operation permissions for the selected object |
| `409` conflict | Refresh; Backlot preserves the draft and retries only after current state is known |
| `429` rate limit | Wait for the displayed retry window; increase polling interval or disable polling |
| Offline/server error | Keep working with cached content, then use Retry after connectivity returns |
| Missing captured image | Ensure an editor or PIE viewport is available and screenshot capture is enabled |
| Unexpected document formatting | Open the source page in Atlassian and inspect unsupported macros/placeholders |

The Output Log category is `LogExtendedAtlassian`. It reports connection, capability, retry,
permission, conversion, and operation results without logging credentials.

## Development and validation

Source layout:

| Module | Responsibility |
|---|---|
| `UnrealExtendedAtlassian` | Runtime-safe transport, settings, credentials, Jira/Confluence APIs, models, conversion, metadata codecs |
| `UnrealExtendedAtlassianEditor` | Backlot controller/providers, native Slate UI, editor target services, capture, and automation |

The deterministic fixture exactly mirrors `HTML/Backlot for UE5.dc.html` and lets interaction and
visual tests run without Atlassian or network access. The HTML and `support.js` are reference/test
inputs only.

Run the headless functional suite:

```powershell
UnrealEditor-Cmd.exe <Project>.uproject -unattended -nop4 -nosplash -nullrhi -NoSound `
  -ExecCmds="Automation RunTests ExtendedAtlassian.Parity;Quit" `
  -TestExit="Automation Test Queue Empty" -log
```

Run the real-render visual capture suite without `-nullrhi`:

```powershell
UnrealEditor-Cmd.exe <Project>.uproject -unattended -nop4 -nosplash -RenderOffscreen -NoSound `
  -ExecCmds="Automation RunTests ExtendedAtlassian.Visual.GoldenCapture;Quit" `
  -TestExit="Automation Test Queue Empty" -log
```

Then compare the lossless reference and Slate captures:

```powershell
python Tools/Parity/compare_backlot_visuals.py
```

See `HTML_PARITY_IMPLEMENTATION_PLAN.md` for the parity ledger and
`Tests/Parity/ReferenceBaselines/ReferenceEnvironment.json` for the pinned golden-capture
environment.
