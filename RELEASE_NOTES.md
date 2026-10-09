# Pirates! Unbound v0.1.0-beta.2

Graphics and widescreen update for the inspected Steam 1.0.2.0 executable of Sid Meier's Pirates! (2004), on Windows.

## Changes

- **Texture filtering…** in the launcher: Native, Bilinear or Trilinear, plus Native / 2× / 4× / 8× / 16× anisotropic filtering. Try Trilinear + 16× for sharper world textures viewed at an angle. The GPU's supported maximum applies automatically. UI and captured backgrounds retain native filtering. New/legacy preferences default to Native; changes apply on the next launch.
- **Experimental wider Caribbean and governor maps**, off by default: show additional original coastline at native chart scale while retaining markers, compass, toolbar and clickable positions. Blue cutout decoration extends across the display using original artwork and reflected side strips.
- Remove the black overlay bars from stock real-time new-game recruitment scenes when widescreen world rendering and centered UI are enabled. Subtitle proportions stay intact. Prerecorded movies retain their encoded framing.

## Install or update

Download **Pirates-Unbound-v0.1.0-beta.2-Windows.zip**. Quit the game and launcher, then copy its **PiratesWide** folder beside **Pirates!.exe**. Existing installs: merge/replace the included program files, preserving **settings.xml** and **backups**. The archive contains no user settings, saves, executable from the game or game artwork.

In Steam → Pirates → Properties → General → Launch Options, use the quoted full path to **PiratesWideLauncher.exe** followed by `%command%`. Existing launcher integration can stay as it is. Press Play in Steam, choose settings and Play in the launcher. README.md contains installation, recovery and removal instructions.

For the GitHub repository update, extract **Pirates-Unbound-v0.1.0-beta.2-Changed-Files.zip** over the existing beta.1 source tree. It contains complete added/changed files at repository-relative paths, plus the deletion list and SHA-256 manifest under `release/`. It is an incremental source update, not an installation package.

## Validation and limitations

The layout-only release build passes its native geometry, map, state-restoration, hardware MSAA/filtering and launcher tests. Live 4K borderless checks covered sailing with effective 16× anisotropy and both map variants with native control clicks and chart-crop matching. Filtering was tested on one GPU; it adds CPU/GPU work and its performance cost has not been benchmarked. It does not increase source texture resolution.

The optional maps remain experimental: reflected decoration repeats the navigation cutout's raised right shape; ultrawide, other GPUs, device-reset stress and all map controls have not been fully qualified. Unsupported cases fall back to the previous rendering path. Wider maps require centered UI and filled backgrounds.

**High-FPS/interpolation research remains disabled. Original gameplay timing is preserved.** This release includes no general performance fix. Other game editions and wrapper/injector combinations remain untested. All towns, activities and long sessions have not been exhaustively validated.

Verify downloads with **SHA256SUMS.txt**. Report visual issues with scene/town, resolution, launcher options and reproduction steps. This is an independent community project, not an official Firaxis/2K patch.
