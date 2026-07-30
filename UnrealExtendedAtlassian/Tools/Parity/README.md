# Backlot parity tooling

`extract_backlot_reference.py` freezes the HTML prototype into a machine-readable contract. It
verifies the approved source hashes and structural inventory before writing
`Tests/Parity/BacklotReferenceManifest.json`.

Run from the plugin root:

```powershell
python Tools/Parity/extract_backlot_reference.py
node Tools/Parity/extract_backlot_fixture.mjs
python Tools/Parity/generate_backlot_palette.py
python Tools/Parity/generate_backlot_typography.py
python Tools/Parity/extract_backlot_reference.py --verify-only
python Tools/Parity/extract_backlot_reference.py --verify-only --require-complete
python -m unittest Tools/Parity/test_extract_backlot_reference.py
python Tools/Parity/compare_backlot_visuals.py
python Tools/Parity/check_backlot_style_literals.py --require-surfaces
python Tools/Parity/validate_backlot_qualification.py
python Tools/Parity/prepare_backlot_scratch_qualification.py --help
```

The generated manifest intentionally begins with unmapped Slate widget and test fields. Those fields
are completed as each implementation phase lands; the final parity gate rejects remaining unmapped
rows. `--require-complete` is the release gate and intentionally fails until every contract row is
implemented and its test result is `passed`.

`compare_backlot_visuals.py` reads immutable browser reference PNGs and native
Slate captures, then writes candidate, enhanced-diff, heatmap, and JSON metric
artifacts under `Saved/Automation/ExtendedAtlassian/Visual/Comparison`. It
never writes to `Tests/Parity/ReferenceBaselines`.

`check_backlot_style_literals.py` is the source gate for focused surface widgets:
new surface code must consume named Backlot colors, typography, and metrics.
Intentional paint primitives remain centralized in `SBacklotStylePrimitives`.

`validate_backlot_qualification.py` validates and deterministically expands
`Tests/Parity/BacklotQualificationMatrix.json`. It is plan-only by default and
does not compile, launch Unreal, capture images, or mutate Atlassian. Use
`--require-passed` only for the final release gate after the deferred build,
automation, visual, performance, scratch-site, and approval passes.

`prepare_backlot_scratch_qualification.py` validates disposable Jira and
Confluence resource IDs and emits a secret-free descriptor. It is plan-only
unless `--write` is supplied, never contacts Atlassian, and refuses resource
names without the `BACKLOT SCRATCH` marker.
