# Unreal Extended Atlassian

Jira and Confluence inside the Unreal Editor. Part of the Extended Framework plugin family.

- **Engine:** UE 5.6
- **Atlassian:** Cloud only (`https://<site>.atlassian.net`). Data Center / Server is not supported.
- **Scope:** editor only — the plugin is gated to editor targets and ships nothing.

## What it does

**Report a bug without leaving the editor.** `Ctrl+Alt+B` captures the viewport and the surrounding
editor state, then files a Jira issue with a screenshot and log tail attached. The capture happens
before the dialog appears, so the screenshot shows what you were looking at.

Captured automatically: level name, world context (Editor / PIE / Simulate), camera transform — the
*player* camera during play — selected actors, engine version, platform, RHI, GPU, and the git branch
and short SHA of the project repository.

**Browse Jira issues.** A docked tab driven by JQL, with saved presets. Change status, post comments,
jump to the browser.

**Read Confluence docs.** A docked tab with a space/page tree and CQL search, so design docs sit
beside the level instead of in another window.

## Setup

1. Enable the plugin and restart the editor.
2. **Project Settings → Extended Framework → Extended Atlassian.**
3. Set **Atlassian Site URL** to `https://yourcompany.atlassian.net`. Shared with the team through
   `Config/DefaultGame.ini`.
4. Under **Credentials**: enter your account e-mail, click **Get an API Token**, paste the token in,
   then **1. Save Credentials** → **2. Test Connection**.
5. A successful test loads your projects, issue types, priorities and Confluence spaces. The Jira and
   Confluence fields below are **dropdowns** — pick from the lists rather than typing keys.

Only the site URL and your credentials are ever typed. Everything else comes from Atlassian. Use
**3. Refresh Lists from Atlassian** if projects or spaces change later.

Each person does step 4 once on their own machine. The e-mail and token fields are `Transient` — they
are never written to project config, only to your per-user credential file.

> The dropdowns are empty until the first successful connection, and every field always offers its
> currently configured value — so an offline editor or a project you lose access to never silently
> erases a setting. Watch the **Output Log** (`LogExtendedAtlassian`) if a button seems not to
> respond; every action reports what it decided.

### Where your token is stored

`%LOCALAPPDATA%\UnrealExtendedAtlassian\credentials.ini` — deliberately outside the repository, so no
`.gitignore` mistake can commit it. On Windows the token is wrapped with DPAPI and bound to your user
account; copying the file to another machine yields nothing usable.

This is obfuscation at rest, not a secret manager. Anything running as you can unwrap it, and an
Atlassian API token grants full access to your account. Treat it like a password.

## Using it

| | |
|---|---|
| **Report a bug** | `Ctrl+Alt+B`, or *Window → Atlassian → Report Bug* |
| **Jira issues** | *Window → Atlassian → Jira Issues* |
| **Confluence docs** | *Window → Atlassian → Confluence Docs* |

**Issue browser.** Pick a JQL preset or type your own and press Enter. Columns all sort. Selecting an
issue loads its description, comments and available transitions. Changing status applies immediately
and rolls back with a notification if Jira rejects it. Results cap at 200 per query; the status line
says so when the cap truncated the set.

**Bug report.** Everything is optional except the summary. If the issue is created but an attachment
upload fails, the issue still stands and the notification names which attachment did not make it. If
the *create* fails, the dialog stays open so nothing you typed is lost.

**Confluence.** Spaces expand lazily. Page bodies are cached for the session — use **Reload Page** to
refetch. Search accepts plain text (wrapped into CQL and scoped to your configured spaces) or raw CQL.

## Known limits

- **Rich text is flattened.** Jira descriptions and Confluence pages are converted to plain text with
  Markdown-style markers. Prose, headings, lists and code read well. Complex macros, embedded Jira
  tables and images degrade to text or placeholders. Every page and issue has an **Open in Browser**
  button for exactly this reason.
- **Writing is plain too.** Comments and bug descriptions are sent as paragraphs, hard breaks and code
  blocks. Anything richer should be edited in Jira.
- **No live notifications.** The editor has no public endpoint, so there are no webhooks. Polling is
  available in settings but **off by default** — Atlassian's rate limits are cost-based, and a team
  polling in parallel will get throttled.
- **Priority** is only sent when you explicitly pick one; projects without a priority field reject the
  create otherwise.
- **Labels** have spaces replaced with hyphens, because Jira rejects labels containing spaces.

## Configuration reference

Everything is under *Project Settings → Extended Framework → Extended Atlassian*.

| Section | Notes |
|---|---|
| Connection | Site URL. |
| Jira | Project key, separate bug project (optional), default issue type and priority, JQL presets. |
| Confluence | Space keys to show. Empty means every space you can read. |
| Polling | Off by default. Minimum 60s when enabled. |
| Bug Report | Which context to capture, and how much log tail to attach. |
| Network | Request timeout and retry count. Retries apply to rate limits and transient server errors only. |

## Reusing it in another project

Nothing in the plugin is specific to the project it was written in. Copy the `UnrealExtendedAtlassian`
folder into another project's `Plugins`, add it to the `.uproject` with
`"TargetAllowList": ["Editor"]`, and configure the site and project key in Project Settings.

## Development

Source layout follows the Extended Framework convention: a flat `Source/<Module>/` with
`ExtendedAtlassian<Thing>.h/.cpp` naming.

| Module | Type | Responsibility |
|---|---|---|
| `UnrealExtendedAtlassian` | Runtime | REST client, credential store, ADF and HTML conversion, Jira and Confluence APIs. No `UnrealEd` dependency. |
| `UnrealExtendedAtlassianEditor` | Editor | Slate tabs, menus, bug report dialog, screenshot and context capture. |

The runtime module avoids `UnrealEd` on purpose, so an in-game bug reporter for playtest builds can
reuse the transport later without restructuring.

### Tests

```bash
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests ExtendedAtlassian" -TestExit="Automation Test Queue Empty" -unattended -nullrhi -nosplash
```

Covers the four pieces that fail silently or misleadingly: ADF round-trip, Confluence HTML
conversion, multipart byte framing, and credential encrypt/decrypt. The credential test writes to a
temporary file and never touches the real per-user store.

See `IMPLEMENTATION_PLAN.md` for the phase-by-phase status and the open risks.
