# Original-texture map decoration trial

2026-10-09. Implemented after approval of the discovery proposal. Available through the existing experimental wider Caribbean and governor maps launcher option.

The generic full-screen backing is insufficient: the foreground cutout contains both the parchment alpha edge and baked blue RGB. This implementation extends that foreground draw using its actual stock GPU texture, with an unchanged central geometry section and reflected side sections. It does not modify game archives, icon geometry, native UI projection or input handlers.

Verified texture identities:

| Screen | Stock texture fingerprint | Native WORLD scale / depth / vertical translation |
|---|---|---|
| Caribbean navigation | `07687d5743b2ca34` (`mapEdgeMaskLarge.dds`) | `0.625 / -0.53 / -80` |
| Governor nearest enemy city | `cd3a352b015adea1` (`mapEdgeMask.dds`) | `0.625 / -0.12 / -160` |

Both live draws use UV corners `(0,1)..(1,0)` and native U address mode 3 (CLAMP). The first reflected version revealed vertical seams. Clamping alone did not fix them: numerical inspection of the original texture columns found a dark terminal left column and a three-column stripe on the right. A draw-local four-pixel UV inset per side omits those stripes. This crops 8 of 1024 columns, changing horizontal sample scale by 0.78125%; text, controls and their coordinates are unaffected. The inset removed the dark joins in both live screens.

Only the identified texture, native quad layout, expected UI transform, full output viewport and supported aspect range qualify. The owned 12-vertex mesh uses the native texture and effective draw state. A complete D3D9 state block restores state afterward. Texture references are retained to prevent address-reuse misidentification and released on reset. Unexpected variants fall back to the existing draw.

Validation:

- Layout-only build and its native geometry, map decoding, UI, hardware MSAA and launcher checks passed.
- Live 3840×2160 borderless / 8× MSAA trial: both map variants covered the full width, with dark joins removed.
- Loaded the arrival save at St. Martin and opened the governor's nearest enemy city map to San Juan. Native title and return-button placement remained intact; clicking return worked.
- Opened the Caribbean chart through the native status toolbar. Both zoom-out clicks worked at the original compass coordinates, traversing zoom 6 → 4 → 2.
- At every final chart crop the existing oracle reported `matches=1 samples=661504`; the governor's zoom-4 crop passed too. This certifies stock chart texel matching, not a pixel-identical decoration after its intentional UV inset.
- Live D3D draw and state restoration returned success for both mask identities.

Remaining qualification: mirrored right-side artwork repeats the navigation cutout's raised shape. This is visible and keeps the option experimental. Ultrawide and non-4K live screenshots, all compass filters, reset stress, and independent GPU state/pixel comparison remain unqualified. The stock main-menu exact texture identity was not newly labeled during this trial; this implementation does not depend on treating it as an identical foreground asset.

Local evidence: `evidence/map-border-trial/first-final.log`, `clamp-governor.log`, `final.log`, `build.log`, and `before/manifest.json`. The source asset previews are local investigation material, excluded from distribution. Test saves and preferences were restored and SHA-256 checked after normal game exit; the new experimental runtime remains installed. Launcher and public release archives were preserved.

Installed runtime SHA-256: `84015749C937E0CC9C9BEF198AAB46481DB36117B43BA6C3046EAEE57CD9EA2E`.
