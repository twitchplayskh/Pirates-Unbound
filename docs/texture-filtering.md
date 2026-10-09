# Texture filtering

Included in v0.1.0-beta.2. Previous beta.1 archives remain preserved locally.

The inspected installation already has `TrilinearFiltering = 1` in Config.ini. A live sailing capture near St. Martin recorded native MIN/MAG/MIP filters `2,2,2` (linear) and MAXANISOTROPY `1`. These observed draws use trilinear filtering without anisotropic minification. This does not prove every game pass uses the same settings.

The launcher offers Native, Bilinear and Trilinear filtering, independently of Native / 2× / 4× / 8× / 16× anisotropy. New and legacy preferences default to Native. Use settings transfers selections to the main launcher; Save settings or Play persists them. Disabling the patch disables both overrides. The installed test configuration uses Trilinear + 16×.

The original Direct3D 9 renderer supports these filters through sampler states; no texture assets or rendering wrapper are required. Anisotropy is limited to the reported GPU maximum and requires anisotropic minification support. See Microsoft's [Direct3D 9 anisotropic filtering documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/anisotropic-texture-filtering).

## Scope and restoration

Before each submitted world draw, a scoped helper examines the 16 pixel texture samplers. Only ordinary color-format, mipmapped 2D textures with native non-point minification and enabled mip filtering qualify. Interface draws, render targets, depth textures, single-level textures, cube/volume textures and other formats retain native sampling. This is conservative coverage, not a promise that every effect texture is untouched: a world effect using an eligible color texture can qualify.

The helper saves and restores MINFILTER, MAGFILTER, MIPFILTER and MAXANISOTROPY around the draw. Address modes, UVs, textures and game state-cache bookkeeping remain unchanged. A failed sampler-state change restores that stage immediately. Bilinear selects linear minification/magnification with point mip selection; trilinear selects linear mip blending if supported. Anisotropy replaces minification and preserves the selected or native mip filtering.

Higher anisotropy can sharpen textures viewed at an angle, but it does not increase source texture resolution or replace MSAA. The override adds sampler inspection/state-change CPU work and GPU filtering cost. Performance has not been benchmarked. Native settings bypass the helper's sampling work.

## Validation

- Full layout-only build and existing native/launcher checks passed.
- Hardware D3D9 tests exercise all 15 filtering/anisotropy combinations, stages 0 and 3, supported-capability clamping, sampler restoration, unchanged address modes and UI/point/no-mip/single-level/render-target exclusions. This GPU reports maximum anisotropy 16.
- Launcher XML/INI round trips, invalid values and patch-disabled behavior passed. Dialog checked at this display's DPI, with both labels and help text visible.
- Launched through Steam at 3840×2160 with the existing 8× MSAA setting, loaded the paused sailing save and captured eight eligible world draws. Native `2,2,2,1` became effective `3,2,2,16` with cap 16. Menu and sailing UI displayed normally. This is a smoke test and sampler-state proof, not a controlled image-quality comparison or exhaustive gameplay validation.
- Test saves and game Config.ini/KeyMap.ini restored from pre-test backups; other launcher preferences retained. Existing release ZIPs preserved.

Local diagnostic evidence is under `evidence/texture-filtering`; saves, logs and installed preferences are not public package assets.
