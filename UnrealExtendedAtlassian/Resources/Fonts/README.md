# Font bundle

## IBM Plex

The Backlot editor UI vendors the following IBM Plex OpenType fonts so its rendering does not
depend on an operating-system font installation or a network connection:

- IBM Plex Sans Regular
- IBM Plex Sans Medium
- IBM Plex Sans SemiBold
- IBM Plex Mono Regular
- IBM Plex Mono Medium

Source: IBM Plex release `v6.4.0`, `OpenType.zip`

Upstream: <https://github.com/IBM/plex>

Release: <https://github.com/IBM/plex/releases/tag/v6.4.0>

License: SIL Open Font License 1.1. The complete license text is stored in `OFL.txt`.

## Noto Emoji

IBM Plex carries no emoji or pictographic symbols, and Confluence pages use them inline, so every
one rendered as a missing-glyph box. `NotoEmoji-Regular.ttf` is appended to the composite fallback
typeface ahead of the engine's CJK `DroidSansFallback.ttf`, so symbols resolve from the font that is
about symbols and CJK still falls through.

- Noto Emoji, Version 3.002

This is the **monochrome** family, deliberately not `NotoColorEmoji`: Slate has no COLR/CBDT colour
glyph support, and the colour builds are 3–10 MB. Glyphs render in the surrounding text colour, so
they read as typography rather than as Confluence's colour emoji images.

Upstream ships it as a variable font with a single `wght` axis (300–700, default 400). FreeType loads
the default instance, so it renders as Regular with no instancing step and no engine-side support for
variable fonts required. The file is vendored byte-for-byte as published.

Source: `google/fonts`, `ofl/notoemoji/NotoEmoji[wght].ttf`, vendored as `NotoEmoji-Regular.ttf`

Upstream: <https://github.com/google/fonts/tree/main/ofl/notoemoji>

License: SIL Open Font License 1.1, copyright Google LLC. The complete license text is stored in
`OFL-NotoEmoji.txt`, kept separate from IBM Plex's `OFL.txt` because the copyright holders differ.

## Frozen SHA-256 hashes

| File | SHA-256 |
| --- | --- |
| `IBMPlexSans-Regular.otf` | `6B17A35A31DED2E81B3ED19E5EB532D22B9A0B5A76833B0D757A5C71AB5E0F6C` |
| `IBMPlexSans-Medium.otf` | `27D25E3E08EA63DF7CD1FA535C9C45AA04BA11CA75B8031B2FFB83F247601AD1` |
| `IBMPlexSans-SemiBold.otf` | `1AFF1F99F0F415632E71A4B9D43804D093E85B8954489A973F0CF1E2E24B9B04` |
| `IBMPlexMono-Regular.otf` | `84D88458FD307636A2BD1A7A9A5432B394157A4766FC4EAC7895A68B63D38E83` |
| `IBMPlexMono-Medium.otf` | `4C3E6855BE7F36DD5BE412F4FA28DC16764BDC1BEE1337836035E090CA90D540` |
| `OFL.txt` | `91C25C350D3CAC39DA2736D74F7BA37EF648F5237A4E330A240615BC8D8C4360` |
| `NotoEmoji-Regular.ttf` | `DE6C18832938AFC99CAF132B39D6A30A19BAC7F2E812E28DB2535B4608D27551` |
| `OFL-NotoEmoji.txt` | `500BB1CCF43DF7BBB522112F9133A52B16E1C35E809632F5D8609B179152DE5B` |
