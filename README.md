# Pirates! Unbound

A community widescreen project for **Sid Meier's Pirates! (2004)**, with a Steam launcher, borderless fullscreen, MSAA and a keyboard remapping editor.

**Current beta: v0.1.0-beta.2.** The release preserves the original gameplay timing. High-FPS rendering and interpolation remain research and are disabled in the public build.

## Features

- Wider 3D world view with the original vertical field of view.
- Centered 4:3 text and controls, with matching mouse coordinates.
- Windowed or borderless display, presets and custom resolution up to 3840×2160.
- Full-width dialogue fades, corrected scene-copy layers and town scenery composition.
- Recognition of all 13 stock town backgrounds, clouds and national town flag variants.
- Decorative parchment map margins, plus opt-in wider original Caribbean/governor charts with native controls.
- Off / 2× / 4× / 8× MSAA, with supported-level fallback.
- Native / Bilinear / Trilinear world texture filtering and Native / 2× / 4× / 8× / 16× anisotropy, limited to GPU support.
- Keyboard remapping per gameplay mode and optional WASD presets.
- Automatic display-config recovery and backups before launch.

## Compatibility

Windows, the inspected Steam **1.0.2.0** executable only. Its SHA-256 is:

```text
5342209C16EA847FA6AC9B90F25A20B46012AEF24D1B5F8A7C7FACDFE174E2E7
```

The launcher rejects other executables. Other editions and combinations with dgVoodoo, ReShade or other injectors are untested. This is an independent community project, not an official game patch.

## Install

1. Download `Pirates-Unbound-v0.1.0-beta.2-Windows.zip` from this repository's Releases section.
2. Quit the game and launcher. Extract the ZIP and copy its **PiratesWide** folder into the game folder, beside `Pirates!.exe`.
3. In Steam → Pirates → Properties → General → Launch Options, enter the launcher path followed by `%command%`. For example:

```text
"C:\SteamLibrary\steamapps\common\Sid Meier's Pirates!\PiratesWide\PiratesWideLauncher.exe" %command%
```

Replace the example path with your actual game location. Press **Play in Steam**, select display settings, and press **Play** in the launcher. Existing installs: replace the included program files, preserving `settings.xml` and `backups`.

Borderless uses the desktop display mode. Select your monitor's native resolution for the best result; lower matching-aspect resolutions scale to the monitor. For a normal window, select a size that fits the desktop. Change resolution between sessions through the launcher.

**Controls / hotkeys** edits the game's KeyMap.ini while the game is closed. Save controls and restart the game to apply them. Duplicate keys within a mode are rejected. Some hardcoded shortcuts and on-screen button hints remain unchanged.

**Texture filtering…** opens filtering controls. Select **Trilinear + 16×** for sharper world textures at oblique angles, then **Use settings** and **Save settings** or **Play**. Native preserves the game's selection. Changes apply on the next launch; UI and captured backgrounds retain native filtering. See [filtering coverage and validation](docs/texture-filtering.md).

## Backups and removal

The launcher backs up Config.ini and existing saves in `PiratesWide/backups`, then restores Config.ini byte for byte after normal exit. New saves are retained. If the launcher is interrupted, quit the game and reopen the launcher to recover the pending config; **Restore display** also retries recovery. Control edits have separate backups.

To remove Steam integration, quit the game, recover any pending config through the launcher, then clear Steam's Launch Options. You can keep the backups and remove the PiratesWide program folder. The patch does not replace Pirates!.exe or edit the game's archives.

## Known limitations

This is a beta with limited hardware and gameplay coverage. All stock town artwork is recognized, but every city, interior and activity has not been visited. Modded art needs separate recognition. World-object picking, long sessions, combat, dancing, sneaking and repeated device-loss transitions need more validation.

Maps use decorative margins by default. The optional experimental wider Caribbean/governor maps show more installed chart imagery at native scale, preserving markers and controls. Their blue cutout decoration uses reflected side strips; the navigation cutout's raised shape repeats. Unsupported inputs fall back. See [the map trial notes](docs/experimental-wide-maps.md).

MSAA smooths polygon edges; texture and shader aliasing can remain. 4× MSAA adds roughly 253 MiB of color/depth storage at 4K. Unsupported levels fall back. In-game resolution changes and arbitrary device recreation are not supported; restart instead.

The release includes no general performance fix. The experimental sailing checkbox is disabled. Original gameplay and animation timing remain unchanged.

## Build

Install an **i686 / 32-bit MinGW-w64 GCC** toolchain at `tools/toolchain/mingw32` (or pass `-Toolchain` to the build script). The local tested compiler is GCC 16.1.0. The WinForms launcher uses the Windows .NET Framework 4 compiler.

```powershell
./tools/build.ps1 -LayoutOnly
./tools/package.ps1
```

The build runs coordinate/frustum, transient restoration, D3D overlay, hardware MSAA/filtering and launcher/control checks. It writes `build/layout-only.build`, which the package requires. The package creates separate source and Windows archives under `dist`, using an allowlist so local research, toolchains, dumps, saves and settings are not included.

The rendering runtime is C++ in `src`, the launcher is C# in `launcher`, and MinHook is vendored. Diagnostic research code is excluded by release compilation flags. See [the roadmap](docs/ROADMAP.md), [contributing](CONTRIBUTING.md), [release notes](CHANGELOG.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

## License

Original project code: [MIT](LICENSE). MinHook/HDE retain their original BSD-style licenses. Game code, artwork and saves are not covered by the project license and are not distributed.
