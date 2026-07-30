#!/usr/bin/env python3
"""Contract tests for the frozen Backlot reference extractor."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
PLUGIN_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

import extract_backlot_reference as extractor  # noqa: E402
import generate_backlot_typography as typography  # noqa: E402


class BacklotReferenceExtractorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.html = PLUGIN_ROOT / "HTML" / "Backlot for UE5.dc.html"
        cls.support = PLUGIN_ROOT / "HTML" / "support.js"
        cls.manifest_path = PLUGIN_ROOT / "Tests" / "Parity" / "BacklotReferenceManifest.json"
        cls.fixture_path = PLUGIN_ROOT / "Tests" / "Parity" / "BacklotFixture.json"
        cls.extracted = extractor.build_manifest(cls.html, cls.support)

    def test_frozen_reference_is_valid(self) -> None:
        self.assertEqual([], extractor.validate_manifest(self.extracted))

    def test_generated_manifest_is_current(self) -> None:
        checked_in = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(self.extracted, checked_in)

    def test_contract_ids_are_unique(self) -> None:
        rows = (
            self.extracted["regions"]
            + self.extracted["eventBindings"]
            + self.extracted["operationDefinitions"]
        )
        ids = [row["contractId"] for row in rows]
        self.assertEqual(len(ids), len(set(ids)))

    def test_all_events_have_a_source_region(self) -> None:
        unclassified = [
            row["contractId"]
            for row in self.extracted["eventBindings"]
            if row["region"] == "unclassified"
        ]
        self.assertEqual([], unclassified)

    def test_completion_gate_accepts_passing_hash_bound_report(self) -> None:
        errors = extractor.validate_manifest(self.extracted, require_complete=True)
        self.assertEqual([], errors)

    def test_every_event_row_has_traceability_fields(self) -> None:
        required = {
            "contractId",
            "event",
            "line",
            "source",
            "region",
            "fixtureState",
            "slateWidget",
            "handler",
            "dataOwner",
            "expectedModelMutation",
            "expectedServiceCall",
            "goldenImage",
            "testId",
            "testResult",
            "status",
        }
        for row in self.extracted["eventBindings"]:
            self.assertTrue(required.issubset(row), row["contractId"])

    def test_all_reference_rows_are_deterministically_mapped(self) -> None:
        required_values = {
            "regions": (
                "fixtureState",
                "slateWidget",
                "handler",
                "dataOwner",
                "expectedModelMutation",
                "expectedServiceCall",
                "goldenImage",
                "testId",
            ),
            "eventBindings": (
                "fixtureState",
                "slateWidget",
                "handler",
                "dataOwner",
                "expectedModelMutation",
                "expectedServiceCall",
                "goldenImage",
                "testId",
            ),
            "operationDefinitions": (
                "fixtureState",
                "controllerOperation",
                "expectedModelMutation",
                "expectedServiceCall",
                "goldenImage",
                "testId",
            ),
        }
        for collection, fields in required_values.items():
            for row in self.extracted[collection]:
                self.assertNotEqual("unmapped", row["status"], row["contractId"])
                for field in fields:
                    self.assertTrue(row[field], f"{row['contractId']} missing {field}")

    def test_click_binding_traceability_count_is_frozen(self) -> None:
        click_rows = [
            row for row in self.extracted["eventBindings"] if row["event"] == "onClick"
        ]
        self.assertEqual(123, len(click_rows))
        self.assertEqual(123, len({row["contractId"] for row in click_rows}))

    def test_fixture_inventory_and_initial_state(self) -> None:
        fixture = json.loads(self.fixture_path.read_text(encoding="utf-8"))
        constants = fixture["constants"]
        state = fixture["initialState"]
        self.assertEqual(
            "BE30EFC1E6338FC411FD905D7B13FE9C07432E142054A1516919504D4AAA67D9",
            fixture["source"]["fixturePayloadSha256"],
        )
        self.assertEqual(12, len(state["pages"]))
        self.assertEqual(17, len(state["docTree"]))
        self.assertEqual(9, sum(len(rows) for rows in state["comments"].values()))
        self.assertEqual(4, len(state["activityLog"]["NFB-1042"]))
        self.assertEqual(12, len(state["issues"]))
        self.assertEqual(6, len(state["pinCards"]))
        self.assertEqual(9, sum(len(card["threads"]) for card in state["pinCards"]))
        self.assertEqual(8, len(state["inbox"]))
        self.assertEqual(4, sum(1 for row in state["inbox"] if row["unread"]))
        self.assertEqual(5, len(constants["people"]))
        self.assertEqual(5, len(constants["epics"]))
        self.assertEqual(5, len(constants["views"]))
        self.assertEqual(5, len(constants["statuses"]))
        self.assertEqual(8, len(constants["blockTypes"]))
        self.assertEqual("docs", state["view"])
        self.assertEqual("wet", state["docSel"])
        self.assertEqual("NFB-1042", state["selected"])
        self.assertEqual("M_WetStone_Master", state["pinSel"])
        self.assertEqual(0, state["threadSel"])
        self.assertEqual("All", state["inboxTab"])
        self.assertEqual(0, state["inboxSel"])
        self.assertEqual(2, state["activePin"])
        self.assertTrue(state["railOpen"])
        self.assertFalse(state["compact"])

    def test_generated_slate_palette_contains_every_literal(self) -> None:
        palette_path = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealExtendedAtlassianEditor"
            / "Generated"
            / "ExtendedAtlassianPalette.inl"
        )
        palette = palette_path.read_text(encoding="utf-8")
        for color in self.extracted["visualTokens"]["hexColors"]:
            self.assertIn(color.lower(), palette)
        self.assertEqual(102, palette.count("Backlot.Literal."))

    def test_vendored_font_bundle_is_frozen(self) -> None:
        self.assertEqual(
            extractor.EXPECTED_FONT_HASHES,
            self.extracted["reference"]["fontBundle"]["files"],
        )

    def test_generated_typography_contains_every_font_contract(self) -> None:
        typography_path = (
            PLUGIN_ROOT
            / "Source"
            / "UnrealExtendedAtlassianEditor"
            / "Generated"
            / "ExtendedAtlassianTypography.inl"
        )
        typography_text = typography_path.read_text(encoding="utf-8")
        self.assertEqual(typography.generate(self.manifest_path), typography_text)
        self.assertEqual(79, typography_text.count("Backlot.HTML.Font."))
        self.assertNotRegex(typography_text, r"(?<![.0-9])\d+f\b")

    def test_cpp_float_literals_are_valid(self) -> None:
        self.assertEqual("1.0f", typography.cpp_float(1.0))
        self.assertEqual("10.0f", typography.cpp_float(10.0))
        self.assertEqual("7.5f", typography.cpp_float(7.5))


if __name__ == "__main__":
    unittest.main()
