# v0.1.0-beta.2 code notes

## Scope

This release is built with `PIRATES_LAYOUT_ONLY`. It adds graphics settings and optional map/cinematic composition changes while retaining original gameplay timing. It does not qualify or enable the unfinished owned-hull/high-FPS architecture. The source delta is measured against the saved beta.1 distribution, rather than the private working tree's unrelated Git history.

## Filtering

- `src/texture-filtering.h`: scoped Direct3D9 sampler overrides for eligible mipmapped color textures during world draws. Native settings bypass the work. GPU capability checks limit anisotropic minification and select supported mip filtering. Four sampler values are restored after each draw, including failure recovery; native game state-cache bookkeeping is not patched.
- `src/widescreen.cpp`: loads enumerated INI values and creates the scope around each of the four intercepted draw variants. Custom map/UI operations retain their separate state handling.
- `launcher/Filtering.cs`, `launcher/Launcher.cs`: modal options dialog, DPI scaling, preference persistence and validation, INI generation and patch-disabled behavior. XML fields default to zero for old settings files. Use settings updates the main form; Save/Play persists it. No existing display/control preference is reset.
- `tests/texture-filtering.cpp`: hardware D3D9 fixture exercises filter/AF combinations, stages, state restoration, address-mode preservation and conservative exclusion cases. Launcher self-tests cover persistence and validation. Actual sailing capture confirmed native linear min/mag/mip with maximum anisotropy 1, overridden to anisotropic minification with maximum 16 on the tested GPU.

The filter does not replace texture assets or increase their resolution. Eligible effect textures can also qualify. Per-draw inspection and filtering add overhead; no performance benchmark or cross-GPU qualification is claimed. Details: `docs/texture-filtering.md`.

## Maps and recruitment scenes

- `src/map-expansion.h`: decodes installed stock chart imagery, checks coherent native chart-crop samples and builds an owned wider texture at native scale. Native markers, controls and input mapping retain their positions. Missing/unsupported assets, mismatches or unsupported layouts fall back. No game assets are distributed.
- `src/map-decoration.h`: recognizes two verified original map mask textures and uses a central quad with reflected side strips. A four-pixel UV inset omits baked edge stripes. State restoration and retained texture references protect following draws and reset handling. The repeated raised edge remains a documented experimental visual limitation.
- `src/cinematic-bars.h`: narrow geometry/transform/color recognition for stock real-time recruitment overlay strips. Arbitrary black panels, subtitles and encoded movies are not removed.
- `tests/map-expansion.cpp`, `tests/coordinates.cpp`: chart decoding/crop, placement and geometry fixtures. Live 4K checks covered both maps and native return/zoom clicks. Coverage and remaining limitations are recorded in the map docs.
- `src/town-assets.h`: catalog differences, if present in the delta, accompany the installed-image map recognition work; no artwork files are part of this release.

## Research source and packaging

The retained owned-render observation/proof and callback/census headers are diagnostic research source already present in the beta.1 distribution, so they are omitted from this incremental update. They remain outside the release runtime's layout-only path. This release provides no evidence that interpolation, independent rendering or generation tracking is ready. No new gameplay modes or extra simulation frames are enabled.

`tools/build.ps1` compiles/runs the added map/filtering fixtures and includes the filtering dialog; metadata obtains the version from VERSION. `tools/package.ps1` accepts a separate output directory, checks artifact hashes and exports allowlisted source/program files. The versioned runtime banner identifies beta.2. Original MIT and third-party license files are preserved.

## Validation and delivery

The release is rebuilt with `tools/build.ps1 -LayoutOnly`; all required native geometry, map, transient/UI, hardware MSAA/filtering and launcher checks must pass before packaging. Live graphics tests were performed before the version/banner-only rebuild. The final program files are hash-checked when packaged. The previous beta.1 archives are retained.

The Changed-Files.zip includes only byte-different/new distribution source files at their repository-relative paths, plus a release manifest and deletion instructions. Applying the delta to beta.1 must reproduce the complete beta.2 source export byte for byte. The Windows ZIP includes program files, user documentation, licenses and hashes; it excludes saves, preferences, game assets and the toolchain.
