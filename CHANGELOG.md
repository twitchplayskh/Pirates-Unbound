# Changelog

## v0.1.0-beta.2 — Filtering and optional wider maps

- Add a Texture filtering launcher dialog with Native, Bilinear and Trilinear choices and independent Native / 2× / 4× / 8× / 16× anisotropic filtering.
- Limit anisotropy to GPU support. Apply qualifying world-texture overrides only around submitted draws and restore native sampler states afterward. Preserve interface and render-target sampling.
- Add an opt-in wider Caribbean/governor map option using original installed chart images, preserving native chart scale, markers and controls. Unsupported assets or chart crops fall back to native rendering.
- Extend the original blue parchment-cutout decoration with reflected side strips. Omit baked edge stripes with a small UV inset; the navigation cutout's raised right edge repeats, so the option remains experimental and off by default.
- Remove stock overlay bars from real-time new-game recruitment scenes while preserving subtitle proportions. Encoded movie framing is unchanged.
- Add filtering hardware tests, map fixtures and implementation/validation notes. Package metadata reads VERSION; separate output directories preserve previous release artifacts.

Native filtering remains the default for new and existing preferences. The release contains no high-FPS/interpolation behavior or general performance fix. Original gameplay timing is preserved.

## v0.1.0-beta.1 — First public beta

- Steam launcher with resolution, borderless fullscreen, widescreen options and MSAA.
- Centered 4:3 interface with matching mouse coordinates.
- Wider world camera, dialogue fades and corrected tavern compositing.
- Recognition of 13 stock town backgrounds and accompanying clouds/flags.
- Fix detached flags using the national `flag_eng`, `flag_fre`, `flag_dut`, `flag_spa` and pirate variants missing from the earlier texture catalog.
- Keyboard remapping editor, backup and config recovery.
- Separate clean source export and installable Windows archive, with SHA-256 checksums and retained licenses.

High-FPS experiments are disabled. Wider cartographic map rendering remains deferred; decorative parchment margins retain the original map layout. Compatibility and extended-play validation remain incomplete.
