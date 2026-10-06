# Pirates-Unbound
# 🏴‍☠️ Sid Meier's Pirates! Modernization Project

A modern PC compatibility and enhancement project for **Sid Meier's Pirates! (2004)**, focused on improving the game for modern displays while preserving the look, gameplay and presentation of the original.

The project currently focuses on **proper widescreen support**, including rebuilding parts of the game's presentation that were originally designed specifically around a 4:3 display.

Rather than simply stretching the original image, the goal is to make Pirates! look and behave as though it had proper widescreen support from the beginning.

---

## ⚓ Project Goals

The main goal is simple:

> **Modernize the technical presentation of Sid Meier's Pirates! without changing what makes the original game look and feel like Pirates!.**

This means avoiding crude image stretching, distorted UI elements and changes to gameplay wherever possible.

The project is being designed around resolution-independent fixes so that improvements are not restricted to one particular display resolution.

---

# 🖥️ Widescreen Support

The project adds proper widescreen handling to areas of the game that were originally designed for **4:3 displays**.

Target resolutions include:

- 1920×1080
- 2560×1440
- 3840×2160 (4K)
- Other 16:9 resolutions
- Additional aspect ratios where possible

The intention is to calculate layouts dynamically rather than maintain separate hacks for every resolution.

---

## 🏘️ Reworked Town & Location Screens

One of the more difficult parts of adding widescreen support to Pirates! is the game's town and location screens.

These screens combine illustrated scenery with interactive menus and were composed specifically for a 4:3 viewport.

Simply stretching them to 16:9 causes:

- Distorted buildings
- Stretched mountains
- Incorrect scenery proportions
- Poorly positioned artwork
- Unnatural empty areas

The project instead aims to **recompose these scenes for widescreen**.

For supported screens:

- Menu text remains in its intended position
- Existing mouse interaction remains aligned with the menu
- Town and scenery artwork can be repositioned independently
- Town artwork is never horizontally stretched
- Mountains retain their original proportions
- Background and atmospheric effects can expand into the additional widescreen space

For example, town artwork that originally occupied the right side of a 4:3 composition can be moved toward the new right edge of a 16:9 display while the menu remains unchanged.

Conceptually:

### Original 4:3

```text
+------------------------------+
| MENU             SKY         |
| MENU                         |
| MENU               TOWN      |
|                TOWN          |
|          WATER               |
+------------------------------+
```

### Widescreen

```text
+------------------------------------------+
| MENU                         SKY         |
| MENU                                     |
| MENU                              TOWN   |
|                                  TOWN    |
|          WATER                           |
+------------------------------------------+
```

The intention is to use the **original game artwork**, not generate replacement scenery.

---

# 🌊 Background Extension

Extra widescreen space cannot always be filled by simply revealing more of the original scene.

The project is therefore investigating techniques for extending safe environmental elements such as:

- Ocean
- Sky
- Clouds
- Haze
- Atmospheric gradients
- Existing blue menu/background effects

Recognizable artwork such as buildings, docks, trees and mountains should not simply be stretched to fill the additional space.

Where possible, background and foreground layers are handled independently.

---

# 🎨 Preserve the Original Artwork

A core rule of the project is:

> **Move or extend artwork where appropriate — don't distort it.**

The following should retain their original proportions:

- Towns
- Buildings
- Mountains
- Ships
- Characters
- Docks
- Vegetation
- UI icons
- Fonts

Where a scene needs additional horizontal space, the preferred approach is to extend environmental/background elements and reposition existing scenery.

---

# 🖱️ Original Menu & Mouse Behaviour

Widescreen modifications should not unnecessarily interfere with the game's menu logic.

Where possible:

- Menu text keeps its original layout
- Selection markers remain aligned
- Mouse hitboxes remain aligned with visible options
- Text is not horizontally stretched
- Fonts retain their original proportions

The artwork behind a menu can therefore be recomposed without rebuilding the menu interaction system.

---

# 📐 Resolution-Independent Design

The widescreen system is designed around the current render resolution rather than a collection of hard-coded offsets.

For example, the equivalent 4:3 width of a display can be calculated from its height:

```cpp
float fourThreeWidth = screenHeight * (4.0f / 3.0f);
float extraWidth = screenWidth - fourThreeWidth;
```

This allows the same system to adapt naturally to different resolutions.

Examples:

| Resolution | 4:3 Equivalent | Additional Width |
|---|---:|---:|
| 1920×1080 | 1440×1080 | 480 px |
| 2560×1440 | 1920×1440 | 640 px |
| 3840×2160 | 2880×2160 | 960 px |

Rather than stretching scenery across that additional width, individual scene elements can be repositioned or extended appropriately.

---

# 🧭 Current Development

The project is currently under active development.

Current work includes:

- Widescreen rendering
- Resolution-independent positioning
- 4:3 → 16:9 scene conversion
- Town/location screen recomposition
- Background extension
- UI preservation
- 1080p support
- 1440p support
- 4K support

Additional screens will be investigated individually as development continues.

---

# ⚡ High Frame Rate Investigation

High-refresh-rate support is also being investigated.

Sid Meier's Pirates! was developed long before modern 120 Hz, 144 Hz, 165 Hz and 240 Hz displays became common.

Before simply removing any frame-rate restrictions, the project is investigating how the game handles:

- Animation timing
- Gameplay simulation
- Character movement
- Ship movement
- Combat
- Input
- Camera movement
- Particle effects
- Ocean effects
- Cutscenes
- Audio synchronization

The objective is to determine which systems are:

- Frame-rate dependent
- Delta-time based
- Fixed-timestep based
- Explicitly frame-limited

### Why not just unlock the FPS?

Older games sometimes update gameplay according to rendered frames.

A system written like:

```cpp
position += velocity;
```

may run twice as quickly at 60 FPS if it was originally designed around 30 FPS.

A time-independent implementation would instead behave more like:

```cpp
position += velocity * deltaTime;
```

For this reason, the project will not blindly remove the game's frame-rate limit.

The timing system will be investigated first so that higher rendering rates can eventually be supported **without changing gameplay speed or behaviour**.

---

# 🔬 High-FPS Testing

Testing is planned at:

- 30 FPS
- 60 FPS
- 120 FPS
- 144 FPS
- Higher refresh rates where practical

Particular attention will be given to:

- Sword fighting
- Sailing
- Naval combat
- Dancing
- Character animations
- Camera movement
- Input timing
- Particle effects
- Cutscenes

Any future high-FPS implementation should preserve the timing and gameplay behaviour of the original game.

---

# 🛠️ Development Philosophy

This project follows a few important principles.

### Preserve the original game

This is an enhancement project, not a remake.

### Avoid hard-coded resolution fixes

Where possible, calculations should adapt automatically to the current resolution and aspect ratio.

### Don't stretch artwork

Repositioning original artwork is preferable to distorting it.

### Don't change gameplay accidentally

Visual improvements should not affect simulation speed, difficulty or input timing.

### Investigate before patching

Older games often contain assumptions that aren't immediately obvious.

Rendering, animation and simulation systems should be understood before making invasive changes.

---

# 🧪 Experimental Project

This project is currently experimental.

Expect:

- Bugs
- Incomplete screens
- Rendering issues
- Compatibility problems
- Behaviour that changes between releases

Back up your game files before testing development builds.

---

# 📋 Planned / Investigated Features

- [x] Widescreen project foundation
- [x] Resolution-independent widescreen calculations
- [ ] Complete 1920×1080 support
- [ ] Complete 2560×1440 support
- [ ] Complete 3840×2160 support
- [ ] Reworked town/location screens
- [ ] Dynamic scenery positioning
- [ ] Extended menu/background effects
- [ ] Ultrawide investigation
- [ ] Frame limiter investigation
- [ ] Animation timing investigation
- [ ] 60+ FPS support
- [ ] High-refresh-rate testing
- [ ] Additional compatibility fixes

The roadmap will change as more of the game's rendering and timing systems are understood.

---

# 🐛 Reporting Issues

When reporting a problem, please include:

- Game version
- Mod version
- Resolution
- Aspect ratio
- Windows version
- GPU
- Screenshot or video if applicable
- Description of where the issue occurs

For graphical problems, please mention the exact screen/location where the issue appears.

---

# 🤝 Contributions

Testing, reverse-engineering information and code contributions are welcome.

If you discover information about the game's:

- Rendering pipeline
- Internal resolutions
- UI system
- Animation system
- Frame limiter
- Timing system
- DirectX behaviour

please consider opening an issue or pull request.

---

# ⚠️ Disclaimer

This is an unofficial fan-made project.

It is not affiliated with, endorsed by, or associated with **Firaxis Games**, **2K**, or the original developers/publishers of Sid Meier's Pirates!.

Sid Meier's Pirates! and all associated names, artwork and assets belong to their respective owners.

This project is intended to provide compatibility and visual improvements for legitimately owned copies of the game.

No original game assets should be distributed with this project unless their redistribution is explicitly permitted.

---

## 🏴‍☠️ Set Sail Again

Sid Meier's Pirates! still holds up remarkably well years after its release.

The aim of this project is to make returning to the Caribbean on a modern PC feel a little less like running a game from 2004 — without losing the character of the original.
