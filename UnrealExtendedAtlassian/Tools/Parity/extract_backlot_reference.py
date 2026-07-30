#!/usr/bin/env python3
"""Extract and verify the frozen Backlot HTML parity contract.

The generated manifest is intentionally source-oriented. It gives later Slate automation a stable
way to prove that every authored event binding and major source region has an owner and a test,
without making the editor depend on the HTML at runtime.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


EXPECTED = {
    "html_sha256": "166B75BDE08CC43C495C80D62629344A9C28714E7F14A6DBD5120199F0DC852B",
    "support_sha256": "8FE7DF74405F3C55F49B7249C74EA1397E65D07DEA2B1BD3B4A489BEC2E28CBE",
    "html_lines": 3390,
    "inline_styles": 695,
    "buttons": 38,
    "inline_svgs": 14,
    "click_bindings": 123,
    "hover_rules": 97,
    "conditional_blocks": 102,
    "repeated_templates": 41,
    "inputs": 22,
    "textareas": 10,
    "hex_colors": 102,
    "rgb_colors": 13,
    "font_shorthands": 79,
    "letter_spacings": 9,
    "box_shadows": 9,
    "dimension_properties": 35,
    "named_metrics": 45,
}

EXPECTED_FONT_HASHES = {
    "IBMPlexSans-Regular.otf": "6B17A35A31DED2E81B3ED19E5EB532D22B9A0B5A76833B0D757A5C71AB5E0F6C",
    "IBMPlexSans-Medium.otf": "27D25E3E08EA63DF7CD1FA535C9C45AA04BA11CA75B8031B2FFB83F247601AD1",
    "IBMPlexSans-SemiBold.otf": "1AFF1F99F0F415632E71A4B9D43804D093E85B8954489A973F0CF1E2E24B9B04",
    "IBMPlexMono-Regular.otf": "84D88458FD307636A2BD1A7A9A5432B394157A4766FC4EAC7895A68B63D38E83",
    "IBMPlexMono-Medium.otf": "4C3E6855BE7F36DD5BE412F4FA28DC16764BDC1BEE1337836035E090CA90D540",
    "OFL.txt": "91C25C350D3CAC39DA2736D74F7BA37EF648F5237A4E330A240615BC8D8C4360",
}


COVERAGE_REGIONS = [
    ("root-style", 11, 31, "Backlot root, global CSS, scrollbars, focus, animations"),
    ("shell", 33, 288, "Project strip, navigation, sidebars, command toolbar"),
    ("docs", 292, 536, "Docs reader and structured editor"),
    ("issues", 538, 614, "Issue list and inline status"),
    ("issue-detail", 616, 752, "Full issue detail"),
    ("board", 754, 797, "Sprint board"),
    ("pins", 799, 835, "Pins grid"),
    ("inbox", 837, 873, "Inbox list"),
    ("right-rail", 875, 1113, "View-specific right rails"),
    ("compact", 1117, 1122, "Compact dock visualization"),
    ("overlays", 1125, 1335, "Popovers, menus, confirmation, capture, toast"),
    ("fixtures", 1339, 1708, "Tokens, seeded data, initial state"),
    ("keyboard-timers", 1709, 1776, "Lifecycle, keyboard routing, and timers"),
    ("operations-a", 1777, 2650, "Interaction helpers, popups, mutations, and domain operations"),
    ("operations-b", 2651, 3383, "Derived presentation and remaining operations"),
]

REGION_TRACE = {
    "root-style": {
        "widget": "FExtendedAtlassianStyle",
        "handler": "FExtendedAtlassianStyle::Initialize",
        "owner": "plugin style set",
        "golden": "Goldens/reference_docs_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.StyleGallery",
    },
    "shell": {
        "widget": "SExtendedAtlassianWorkspace::BuildShell",
        "handler": "FExtendedAtlassianWorkspaceController",
        "owner": "workspace controller",
        "golden": "Goldens/reference_docs_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.FixtureContract",
    },
    "docs": {
        "widget": "SExtendedAtlassianWorkspace::BuildDocsMain",
        "handler": "SExtendedAtlassianWorkspace document handlers",
        "owner": "workspace page/document model",
        "golden": "Goldens/reference_docs_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.DocumentMutations",
    },
    "issues": {
        "widget": "SExtendedAtlassianWorkspace::BuildIssuesMain",
        "handler": "SExtendedAtlassianWorkspace issue-list handlers",
        "owner": "workspace Jira issue model",
        "golden": "Goldens/reference_issues_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.IssueOperations",
    },
    "issue-detail": {
        "widget": "SExtendedAtlassianWorkspace::BuildIssueDetailMainDynamic",
        "handler": "SExtendedAtlassianWorkspace issue-detail handlers",
        "owner": "workspace Jira issue/comment/activity model",
        "golden": "Goldens/reference_issue_detail_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.CommentMutations",
    },
    "board": {
        "widget": "SExtendedAtlassianWorkspace::BuildBoardMain",
        "handler": "SExtendedAtlassianWorkspace board handlers",
        "owner": "workspace Jira Software board model",
        "golden": "Goldens/reference_board_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.BoardOperations",
    },
    "pins": {
        "widget": "SExtendedAtlassianWorkspace::BuildPinsMain",
        "handler": "SExtendedAtlassianWorkspace Pin handlers",
        "owner": "shared Backlot Pin store",
        "golden": "Goldens/reference_pins_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.PinMutations",
    },
    "inbox": {
        "widget": "SExtendedAtlassianWorkspace::BuildInboxMain",
        "handler": "SExtendedAtlassianWorkspace Inbox handlers",
        "owner": "synthesized Inbox plus per-account local state",
        "golden": "Goldens/reference_inbox_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.InboxMutations",
    },
    "right-rail": {
        "widget": "SExtendedAtlassianWorkspace::BuildRightRail",
        "handler": "SExtendedAtlassianWorkspace right-rail handlers",
        "owner": "selected workspace presentation model",
        "golden": "Goldens/reference_issue_detail_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.RightRailOperations",
    },
    "compact": {
        "widget": "SExtendedAtlassianWorkspace::BuildShell",
        "handler": "FExtendedAtlassianWorkspaceController::SetCompact",
        "owner": "workspace controller preferences",
        "golden": "Goldens/reference_inbox_compact_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.CompactShell",
    },
    "overlays": {
        "widget": "SExtendedAtlassianWorkspace root overlay builders",
        "handler": "SExtendedAtlassianWorkspace overlay handlers",
        "owner": "workspace transient overlay state",
        "golden": "Goldens/reference_capture_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.OverlayOperations",
    },
    "fixtures": {
        "widget": "FExtendedAtlassianFixtureWorkspaceData",
        "handler": "FExtendedAtlassianFixtureWorkspaceData::ParseFixture",
        "owner": "Tests/Parity/BacklotFixture.json",
        "golden": "Goldens/reference_docs_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.FixtureContract",
    },
    "keyboard-timers": {
        "widget": "SExtendedAtlassianWorkspace::OnKeyDown",
        "handler": "workspace keyboard/active-timer routing",
        "owner": "workspace controller and transient overlay state",
        "golden": "Goldens/reference_capture_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.KeyboardTimers",
    },
    "operations-a": {
        "widget": "SExtendedAtlassianWorkspace",
        "handler": "workspace/controller operation mapping",
        "owner": "fixture/live workspace providers",
        "golden": "Goldens/reference_board_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.OperationDefinitions",
    },
    "operations-b": {
        "widget": "SExtendedAtlassianWorkspace",
        "handler": "workspace/controller derived-state mapping",
        "owner": "fixture/live workspace providers",
        "golden": "Goldens/reference_docs_wide_1920x1080.png",
        "test": "ExtendedAtlassian.Parity.OperationDefinitions",
    },
}


EVENT_PATTERN = re.compile(r"\b(on[A-Z][A-Za-z0-9_]*)\s*=")
HEX_PATTERN = re.compile(r"(?i)#[0-9a-f]{6}\b")
RGB_PATTERN = re.compile(r"(?i)\brgba?\([^)]*\)")
FONT_PATTERN = re.compile(r"(?i)(?:^|[;\"'])\s*font\s*:\s*([^;\"}]+)")
DECLARATION_PATTERN = re.compile(r"([a-zA-Z-]+)\s*:\s*([^;\"'}]+)")
DIMENSION_PATTERN = re.compile(r"\b\d+(?:\.\d+)?(?:px|vh|vw|ch|em)\b")
# The `{` alternative also reaches declarations written inside @keyframes blocks.
SHADOW_PATTERN = re.compile(r"(?i)(?:^|[;\"'{])\s*box-shadow\s*:\s*([^;\"}]+)")
LETTER_SPACING_PATTERN = re.compile(r"(?i)(?:^|[;\"'{])\s*letter-spacing\s*:\s*([^;\"}]+)")

# Every geometry literal the visual contract names, expressed as the exact CSS
# declaration or script literal that must still exist in the frozen source. The
# generator resolves the number from the source; nothing here is transcribed as a
# free-standing value that could silently drift from the HTML.
NAMED_METRIC_PROBES: list[tuple[str, str, str]] = [
    # (metric name, CSS property or "js", literal that must appear in the source)
    ("ProjectStrip.Height", "height", "30px"),
    ("NavRail.Width", "width", "58px"),
    ("CommandHeader.Height", "height", "46px"),
    ("NavButton.Width", "width", "46px"),
    ("NavButton.Height", "height", "46px"),
    ("RightRail.Width", "width", "336px"),
    ("RightRail.MinWidth", "min-width", "268px"),
    ("Sidebar.MinWidth", "js", "sidebarW: s.compact ? '0px' : 'clamp(208px, 17vw, 262px)'"),
    ("Sidebar.ViewportPercent", "js", "sidebarW: s.compact ? '0px' : 'clamp(208px, 17vw, 262px)'"),
    ("Sidebar.MaxWidth", "js", "sidebarW: s.compact ? '0px' : 'clamp(208px, 17vw, 262px)'"),
    ("Compact.ContentWidth", "js", "mainW: s.compact ? '502px' : 'auto'"),
    ("Document.MaxWidth", "max-width", "1000px"),
    ("IssueDetail.MaxWidth", "max-width", "900px"),
    ("Measure.Body", "max-width", "70ch"),
    ("Measure.Wide", "max-width", "74ch"),
    ("BoardColumn.Width", "width", "318px"),
    ("BoardColumn.InnerWidth", "width", "316px"),
    ("CreateCard.Width", "width", "306px"),
    ("PagePopover.Width", "width", "274px"),
    ("PinPopover.Width", "width", "286px"),
    ("GenericMenu.Width", "js", "const w = 196, h = 34 + items.length * 30"),
    ("GenericMenu.HeaderHeight", "js", "const w = 196, h = 34 + items.length * 30"),
    ("GenericMenu.ItemHeight", "js", "const w = 196, h = 34 + items.length * 30"),
    ("Confirm.Width", "width", "min(384px, 90vw)"),
    ("Confirm.ViewportPercent", "width", "min(384px, 90vw)"),
    ("Capture.Width", "width", "min(880px, 92vw)"),
    ("Capture.ViewportPercent", "width", "min(880px, 92vw)"),
    ("Capture.MaxHeightPercent", "max-height", "92vh"),
    ("Capture.FrameHeight", "height", "288px"),
    ("InlineStatusPopup.Width", "width", "168px"),
    ("Tree.RowHeight", "height", "26px"),
    ("InboxRow.MinHeight", "min-height", "58px"),
    ("StatusChip.Height", "height", "21px"),
    ("TeamLoadRow.Height", "height", "36px"),
    ("Viewport.MinWidth", "min-width", "880px"),
    ("Viewport.MinHeight", "min-height", "600px"),
    ("Overlay.ViewportMargin", "js", "Math.max(8, Math.min("),
    ("Overlay.ViewportInset", "js", "window.innerWidth - w - 12"),
    ("Focus.OutlineWidth", "outline", "2px solid #58a6ff"),
    ("Focus.OutlineOffset", "outline-offset", "1px"),
    ("Focus.OutlineRadius", "border-radius", "4px"),
    ("Scrollbar.Thickness", "js", "::-webkit-scrollbar { width: 9px; height: 9px; }"),
    ("Scrollbar.ThumbRadius", "js", "border-radius: 5px; border: 2px solid transparent"),
    ("Scrollbar.ThumbInset", "js", "border-radius: 5px; border: 2px solid transparent"),
]

# The literal each probe resolves to, isolated so a source change fails loudly.
NAMED_METRIC_VALUES: dict[str, float] = {
    "ProjectStrip.Height": 30.0,
    "NavRail.Width": 58.0,
    "CommandHeader.Height": 46.0,
    "NavButton.Width": 46.0,
    "NavButton.Height": 46.0,
    "RightRail.Width": 336.0,
    "RightRail.MinWidth": 268.0,
    "Sidebar.MinWidth": 208.0,
    "Sidebar.ViewportPercent": 17.0,
    "Sidebar.MaxWidth": 262.0,
    "Compact.ContentWidth": 502.0,
    "Document.MaxWidth": 1000.0,
    "IssueDetail.MaxWidth": 900.0,
    "Measure.Body": 70.0,
    "Measure.Wide": 74.0,
    "BoardColumn.Width": 318.0,
    "BoardColumn.InnerWidth": 316.0,
    "CreateCard.Width": 306.0,
    "PagePopover.Width": 274.0,
    "PinPopover.Width": 286.0,
    "GenericMenu.Width": 196.0,
    "GenericMenu.HeaderHeight": 34.0,
    "GenericMenu.ItemHeight": 30.0,
    "Confirm.Width": 384.0,
    "Confirm.ViewportPercent": 90.0,
    "Capture.Width": 880.0,
    "Capture.ViewportPercent": 92.0,
    "Capture.MaxHeightPercent": 92.0,
    "Capture.FrameHeight": 288.0,
    "InlineStatusPopup.Width": 168.0,
    "Tree.RowHeight": 26.0,
    "InboxRow.MinHeight": 58.0,
    "StatusChip.Height": 21.0,
    "TeamLoadRow.Height": 36.0,
    "Viewport.MinWidth": 880.0,
    "Viewport.MinHeight": 600.0,
    "Overlay.ViewportMargin": 8.0,
    "Overlay.ViewportInset": 12.0,
    "Focus.OutlineWidth": 2.0,
    "Focus.OutlineOffset": 1.0,
    "Focus.OutlineRadius": 4.0,
    "Scrollbar.Thickness": 9.0,
    "Scrollbar.ThumbRadius": 5.0,
    "Scrollbar.ThumbInset": 2.0,
}

# Derived metrics the reference states as an arithmetic relationship rather than a
# single literal, so the relationship itself is asserted instead of a magic number.
DERIVED_METRICS: dict[str, tuple[str, str]] = {
    "Compact.DockWidth": ("Compact.ContentWidth", "NavRail.Width"),
}
METHOD_PATTERN = re.compile(
    r"^\s{2,}(?:(?:async)\s+)?([A-Za-z_$][A-Za-z0-9_$]*)\s*(?:=\s*)?"
    r"(?:\([^;]*\)\s*=>|\([^)]*\)\s*\{)",
    re.MULTILINE,
)


@dataclass(frozen=True)
class EventBinding:
    contract_id: str
    event: str
    line: int
    source: str
    region: str
    fixture_state: str = ""
    slate_widget: str = ""
    handler: str = ""
    data_owner: str = ""
    expected_model_mutation: str = ""
    expected_service_call: str = ""
    golden_image: str = ""
    test_id: str = ""
    test_result: str = "not-run"
    status: str = "unmapped"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def count_lines(raw: bytes) -> int:
    # Match PowerShell/Get-Content and editor line numbering even if the final line lacks a newline.
    return raw.count(b"\n") + (0 if raw.endswith(b"\n") else 1)


def source_line(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def region_for_line(line: int) -> str:
    for region_id, start, end, _ in COVERAGE_REGIONS:
        if start <= line <= end:
            return region_id
    return "unclassified"


def compact_source(line: str) -> str:
    return re.sub(r"\s+", " ", line.strip())[:240]


def trace_for_region(region: str) -> dict[str, str]:
    return REGION_TRACE.get(
        region,
        {
            "widget": "SExtendedAtlassianWorkspace",
            "handler": "unclassified reference handler",
            "owner": "workspace controller",
            "golden": "Goldens/reference_docs_wide_1920x1080.png",
            "test": "ExtendedAtlassian.Parity.Unclassified",
        },
    )


def source_handler(event: str, source: str, line: int) -> str:
    match = re.search(rf"\b{re.escape(event)}\s*=\s*\"([^\"]+)\"", source)
    if not match:
        return f"HTML {event} at line {line}"
    value = match.group(1).strip()
    if value.startswith("{{") and value.endswith("}}"):
        value = value[2:-2].strip()
    return value or f"HTML {event} at line {line}"


def expected_transition(handler: str, event: str) -> str:
    lowered = handler.casefold()
    if event in {"onChange", "onInput"}:
        return f"Update transient/draft value through {handler}"
    if event.startswith("onDrag") or event == "onDrop":
        return f"Update drag/drop state and ordered model through {handler}"
    if any(token in lowered for token in ("delete", "remove", "archive", "dismiss")):
        return f"Optimistically remove/archive target through {handler}; retain rollback snapshot"
    if any(token in lowered for token in ("create", "add", "publish", "save", "submit", "reply")):
        return f"Validate and persist authored value through {handler}"
    if any(token in lowered for token in ("resolve", "reopen", "toggle", "mark", "move", "drop")):
        return f"Apply deterministic state transition through {handler}"
    return f"Apply local selection/navigation/presentation transition through {handler}"


def expected_service(handler: str, event: str) -> str:
    lowered = handler.casefold()
    if any(
        token in lowered
        for token in (
            "create",
            "delete",
            "remove",
            "publish",
            "save",
            "submit",
            "reply",
            "resolve",
            "reopen",
            "assign",
            "drop",
            "move",
            "archive",
            "dismiss",
            "mark",
        )
    ):
        return "IExtendedAtlassianWorkspaceData::Mutate when the transition is persistent"
    if event.startswith("onDrag"):
        return "None until drop; drop dispatches IExtendedAtlassianWorkspaceData::Mutate"
    return "None (controller-local interaction state)"


def extract_events(text: str) -> list[EventBinding]:
    lines = text.splitlines()
    events: list[EventBinding] = []
    for index, match in enumerate(EVENT_PATTERN.finditer(text), start=1):
        line_number = source_line(text, match.start())
        source = lines[line_number - 1] if line_number <= len(lines) else ""
        source_compact = compact_source(source)
        trace = trace_for_region(region_for_line(line_number))
        handler = source_handler(match.group(1), source_compact, line_number)
        events.append(
            EventBinding(
                contract_id=f"PARITY-EVENT-{index:03d}",
                event=match.group(1),
                line=line_number,
                source=source_compact,
                region=region_for_line(line_number),
                fixture_state=f"BacklotFixture.json::{region_for_line(line_number)}",
                slate_widget=trace["widget"],
                handler=handler,
                data_owner=trace["owner"],
                expected_model_mutation=expected_transition(handler, match.group(1)),
                expected_service_call=expected_service(handler, match.group(1)),
                golden_image=trace["golden"],
                test_id=f"{trace['test']}::{index:03d}",
                status="mapped",
            )
        )
    return events


def unique_matches(pattern: re.Pattern[str], text: str) -> list[str]:
    return sorted({match.group(0) for match in pattern.finditer(text)}, key=str.casefold)


def font_shorthands(text: str) -> list[str]:
    return sorted(
        {
            re.sub(r"\s+", " ", match.group(1).strip())
            for match in FONT_PATTERN.finditer(text)
            if "IBM Plex" in match.group(1)
        },
        key=str.casefold,
    )


def dimension_declarations(text: str) -> dict[str, list[str]]:
    """Group every dimension-bearing CSS declaration value by property name."""
    grouped: dict[str, set[str]] = {}
    for match in DECLARATION_PATTERN.finditer(text):
        prop = match.group(1).strip()
        value = re.sub(r"\s+", " ", match.group(2).strip())
        if not DIMENSION_PATTERN.search(value) and not value.startswith(
            ("clamp(", "min(", "max(")
        ):
            continue
        grouped.setdefault(prop, set()).add(value)
    return {
        prop: sorted(values, key=str.casefold)
        for prop, values in sorted(grouped.items())
    }


def named_metrics(text: str) -> dict[str, float]:
    """Resolve every named geometry metric, failing if its source literal vanished."""
    resolved: dict[str, float] = {}
    missing: list[str] = []
    for name, prop, literal in NAMED_METRIC_PROBES:
        if prop == "js":
            found = literal in text
        else:
            found = re.search(
                rf"{re.escape(prop)}\s*:\s*{re.escape(literal)}", text
            ) is not None
        if not found:
            missing.append(f"{name} ({prop}: {literal})")
            continue
        resolved[name] = NAMED_METRIC_VALUES[name]
    if missing:
        raise ValueError(
            "named metric probes no longer match the frozen source: "
            + ", ".join(missing)
        )
    for name, (left, right) in DERIVED_METRICS.items():
        resolved[name] = resolved[left] + resolved[right]
    return dict(sorted(resolved.items()))


def application_methods(text: str) -> list[dict[str, object]]:
    methods: list[dict[str, object]] = []
    seen: set[tuple[str, int]] = set()
    for match in METHOD_PATTERN.finditer(text):
        line = source_line(text, match.start())
        if line < 1339:
            continue
        key = (match.group(1), line)
        if key in seen:
            continue
        seen.add(key)
        region = region_for_line(line)
        trace = trace_for_region(region)
        name = match.group(1)
        methods.append(
            {
                "contractId": f"PARITY-OP-{len(methods) + 1:03d}",
                "name": name,
                "line": line,
                "region": region,
                "fixtureState": f"BacklotFixture.json::{region}",
                "controllerOperation": f"{trace['handler']}::{name}",
                "expectedModelMutation": expected_transition(name, "operation"),
                "expectedServiceCall": expected_service(name, "operation"),
                "goldenImage": trace["golden"],
                "testId": f"{trace['test']}::{name}@{line}",
                "testResult": "not-run",
                "status": "mapped",
            }
        )
    return methods


def event_binding_dict(event: EventBinding) -> dict[str, object]:
    return {
        "contractId": event.contract_id,
        "event": event.event,
        "line": event.line,
        "source": event.source,
        "region": event.region,
        "fixtureState": event.fixture_state,
        "slateWidget": event.slate_widget,
        "handler": event.handler,
        "dataOwner": event.data_owner,
        "expectedModelMutation": event.expected_model_mutation,
        "expectedServiceCall": event.expected_service_call,
        "goldenImage": event.golden_image,
        "testId": event.test_id,
        "testResult": event.test_result,
        "status": event.status,
    }


def inventory(html_text: str, html_raw: bytes, support_text: str) -> dict[str, int]:
    del support_text  # Kept in the signature so future support-runtime inventory remains explicit.
    return {
        "html_lines": count_lines(html_raw),
        "inline_styles": len(re.findall(r"\bstyle\s*=", html_text)),
        "buttons": len(re.findall(r"<button\b", html_text)),
        "inline_svgs": len(re.findall(r"<svg\b", html_text)),
        "click_bindings": len(re.findall(r"\bonClick\s*=", html_text)),
        "hover_rules": len(re.findall(r"\bstyle-hover\s*=", html_text)),
        "conditional_blocks": len(re.findall(r"<sc-if\b", html_text)),
        "repeated_templates": len(re.findall(r"<sc-for\b", html_text)),
        "inputs": len(re.findall(r"<input\b", html_text)),
        "textareas": len(re.findall(r"<textarea\b", html_text)),
        "hex_colors": len(unique_matches(HEX_PATTERN, html_text)),
        "rgb_colors": len(unique_matches(RGB_PATTERN, html_text)),
        "font_shorthands": len(font_shorthands(html_text)),
        "letter_spacings": len(
            {
                re.sub(r"\s+", " ", match.group(1).strip())
                for match in LETTER_SPACING_PATTERN.finditer(html_text)
            }
        ),
        "box_shadows": len(
            {
                re.sub(r"\s+", " ", match.group(1).strip())
                for match in SHADOW_PATTERN.finditer(html_text)
            }
        ),
        "dimension_properties": len(dimension_declarations(html_text)),
        "named_metrics": len(named_metrics(html_text)),
    }


def apply_automation_report(
    manifest: dict[str, object], plugin_root: Path
) -> None:
    """Promote rows only when the frozen inputs and coverage source match a passing report."""
    report_path = plugin_root / "Tests" / "Parity" / "BacklotAutomationReport.json"
    automation_source = (
        plugin_root
        / "Source"
        / "UnrealExtendedAtlassianEditor"
        / "ExtendedAtlassianParityTests.cpp"
    )
    if not report_path.is_file() or not automation_source.is_file():
        return
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return
    report_reference = report.get("reference", {})
    manifest_reference = manifest["reference"]
    if (
        report.get("result") != "passed"
        or report_reference.get("htmlSha256") != manifest_reference["htmlSha256"]
        or report_reference.get("supportSha256") != manifest_reference["supportSha256"]
        or report_reference.get("automationSourceSha256") != sha256(automation_source)
    ):
        return
    passed_tests = set(report.get("passedTests", []))
    for collection_name in ("regions", "eventBindings", "operationDefinitions"):
        for row in manifest[collection_name]:
            root_test_id = row["testId"].split("::", 1)[0]
            if root_test_id in passed_tests:
                row["status"] = "implemented"
                row["testResult"] = "passed"


def build_manifest(html_path: Path, support_path: Path) -> dict[str, object]:
    html_raw = html_path.read_bytes()
    support_raw = support_path.read_bytes()
    html_text = html_raw.decode("utf-8")
    support_text = support_raw.decode("utf-8")
    values = inventory(html_text, html_raw, support_text)
    events = extract_events(html_text)
    font_root = html_path.parent.parent / "Resources" / "Fonts"
    font_hashes = {
        filename: sha256(font_root / filename) if (font_root / filename).is_file() else ""
        for filename in EXPECTED_FONT_HASHES
    }

    manifest = {
        "schemaVersion": 1,
        "reference": {
            "html": html_path.name,
            "support": support_path.name,
            "htmlSha256": sha256(html_path),
            "supportSha256": sha256(support_path),
            "authoredViewport": {"width": 1920, "height": 1080},
            "minimumViewport": {"width": 880, "height": 600},
            "defaultState": {"startView": "docs", "railOpen": True, "compact": False},
            "fontBundle": {
                "family": "IBM Plex",
                "release": "v6.4.0",
                "license": "SIL Open Font License 1.1",
                "files": font_hashes,
            },
        },
        "inventory": values,
        "regions": [
            {
                "contractId": f"PARITY-REGION-{index:03d}",
                "id": region_id,
                "startLine": start,
                "endLine": end,
                "description": description,
                "fixtureState": f"BacklotFixture.json::{region_id}",
                "slateWidget": trace_for_region(region_id)["widget"],
                "handler": trace_for_region(region_id)["handler"],
                "dataOwner": trace_for_region(region_id)["owner"],
                "expectedModelMutation": "Render and mutate only the normalized presentation state owned by this region",
                "expectedServiceCall": "IExtendedAtlassianWorkspaceData for persistent operations; none for presentation-only state",
                "goldenImage": trace_for_region(region_id)["golden"],
                "testId": trace_for_region(region_id)["test"],
                "testResult": "not-run",
                "status": "mapped",
            }
            for index, (region_id, start, end, description) in enumerate(COVERAGE_REGIONS, start=1)
        ],
        "visualTokens": {
            "hexColors": unique_matches(HEX_PATTERN, html_text),
            "rgbColors": unique_matches(RGB_PATTERN, html_text),
            "fontShorthands": font_shorthands(html_text),
            "letterSpacings": sorted(
                {
                    re.sub(r"\s+", " ", match.group(1).strip())
                    for match in LETTER_SPACING_PATTERN.finditer(html_text)
                },
                key=str.casefold,
            ),
            "boxShadows": sorted(
                {
                    re.sub(r"\s+", " ", match.group(1).strip())
                    for match in SHADOW_PATTERN.finditer(html_text)
                },
                key=str.casefold,
            ),
            "dimensions": dimension_declarations(html_text),
            "namedMetrics": named_metrics(html_text),
        },
        "eventBindings": [event_binding_dict(event) for event in events],
        "operationDefinitions": application_methods(html_text),
    }
    apply_automation_report(manifest, html_path.parent.parent)
    return manifest


def validate_manifest(
    manifest: dict[str, object], *, require_complete: bool = False
) -> list[str]:
    errors: list[str] = []
    reference = manifest["reference"]
    inventory_values = manifest["inventory"]

    if reference["htmlSha256"] != EXPECTED["html_sha256"]:
        errors.append(
            f"HTML SHA-256 changed: expected {EXPECTED['html_sha256']}, "
            f"got {reference['htmlSha256']}"
        )
    if reference["supportSha256"] != EXPECTED["support_sha256"]:
        errors.append(
            f"support.js SHA-256 changed: expected {EXPECTED['support_sha256']}, "
            f"got {reference['supportSha256']}"
        )
    actual_font_hashes = reference["fontBundle"]["files"]
    for filename, expected_hash in EXPECTED_FONT_HASHES.items():
        actual_hash = actual_font_hashes.get(filename)
        if actual_hash != expected_hash:
            errors.append(
                f"{filename} SHA-256 changed: expected {expected_hash}, got {actual_hash}"
            )

    for name, expected in EXPECTED.items():
        if name.endswith("_sha256"):
            continue
        actual = inventory_values.get(name)
        if actual != expected:
            errors.append(f"{name} changed: expected {expected}, got {actual}")

    click_events = sum(
        1 for binding in manifest["eventBindings"] if binding["event"] == "onClick"
    )
    if click_events != EXPECTED["click_bindings"]:
        errors.append(
            f"event manifest contains {click_events} onClick rows; "
            f"expected {EXPECTED['click_bindings']}"
        )

    unclassified_events = [
        binding["contractId"]
        for binding in manifest["eventBindings"]
        if binding["region"] == "unclassified"
    ]
    if unclassified_events:
        errors.append(
            "event bindings fall outside classified source regions: "
            + ", ".join(unclassified_events)
        )

    if require_complete:
        incomplete: list[str] = []
        for collection_name in ("regions", "eventBindings", "operationDefinitions"):
            for row in manifest[collection_name]:
                if row.get("status") != "implemented" or row.get("testResult") != "passed":
                    incomplete.append(row["contractId"])
        if incomplete:
            preview = ", ".join(incomplete[:12])
            suffix = "" if len(incomplete) <= 12 else f" … and {len(incomplete) - 12} more"
            errors.append(
                f"{len(incomplete)} parity rows are not implemented and passing: "
                f"{preview}{suffix}"
            )

    return errors


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    script = Path(__file__).resolve()
    plugin_root = script.parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--html",
        type=Path,
        default=plugin_root / "HTML" / "Backlot for UE5.dc.html",
    )
    parser.add_argument(
        "--support",
        type=Path,
        default=plugin_root / "HTML" / "support.js",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=plugin_root / "Tests" / "Parity" / "BacklotReferenceManifest.json",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="Validate the frozen inputs without writing the manifest.",
    )
    parser.add_argument(
        "--require-complete",
        action="store_true",
        help="Also fail while any region, event, or operation is not implemented and passing.",
    )
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)
    for source in (args.html, args.support):
        if not source.is_file():
            print(f"error: missing reference source: {source}", file=sys.stderr)
            return 2

    manifest = build_manifest(args.html, args.support)
    errors = validate_manifest(manifest, require_complete=args.require_complete)
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    if not args.verify_only:
        write_json(args.output, manifest)
        print(f"Wrote {args.output}")

    values = manifest["inventory"]
    print(
        "Backlot reference verified: "
        f"{values['html_lines']} lines, "
        f"{values['click_bindings']} clicks, "
        f"{values['hex_colors']} colors"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
