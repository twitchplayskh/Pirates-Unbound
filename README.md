# 🏴‍☠️ Pirates! Unbound

### A modern widescreen, high-resolution and compatibility overhaul for Sid Meier's Pirates! (2004).

**Pirates! Unbound** is a modernization project for the PC version of **Sid Meier's Pirates! (2004)**, focused on making the game work and look better on modern displays while preserving the style, gameplay and presentation of the original.

The project currently provides proper **widescreen and high-resolution support**, including reworked parts of the game's presentation that were originally designed specifically around a 4:3 display.

Rather than simply stretching the original image to fill a modern screen, Pirates! Unbound modifies the way scenes are positioned and presented so that the game feels much closer to having native widescreen support.

---

# ⚓ Project Goals

The main goal is simple:

> **Modernize Sid Meier's Pirates! for modern PCs without losing what makes the original game look and feel like Pirates!.**

Pirates! Unbound aims to:

- Preserve the original artwork
- Preserve the original gameplay
- Support modern resolutions
- Properly adapt 4:3 scenes to widescreen
- Avoid stretched or distorted artwork
- Preserve existing menu and mouse behaviour
- Remove old technical limitations where safely possible
- Investigate high-refresh-rate support
- Improve compatibility with modern PCs

Whenever possible, fixes are designed to be **resolution-independent** rather than hard-coded for individual resolutions.

---

# 🖥️ Widescreen Support

**Status: ✅ Implemented**

Pirates! Unbound adds proper widescreen handling to areas of the game originally designed around **4:3 displays**.

Currently supported target resolutions include:

- ✅ **1920×1080**
- ✅ **2560×1440**
- ✅ **3840×2160 (4K)**
- ✅ **Other compatible 16:9 resolutions through dynamic scaling**

The widescreen system calculates positioning based on the current resolution instead of relying on separate modifications for 1080p, 1440p and 4K.

This allows the same system to scale automatically between different display resolutions.

---

# 🏘️ Reworked Town & Location Screens

**Status: ✅ Implemented**

One of the biggest challenges when adding widescreen support to Pirates! is the game's town and location screens.

These screens combine illustrated scenery with interactive menus and were originally composed specifically for a **4:3 viewport**.

Simply stretching the original presentation to 16:9 causes:

- Distorted buildings
- Stretched mountains
- Incorrect scenery proportions
- Poorly positioned artwork
- Unnatural empty areas
- Misaligned presentation

Pirates! Unbound instead **recomposes these scenes for widescreen**.

The system allows scenery to be positioned independently from the menu interface.

This means:

- ✅ Menu text remains in its intended position
- ✅ Existing mouse interaction remains aligned
- ✅ Menu hitboxes remain unchanged
- ✅ Town artwork can be repositioned independently
- ✅ Towns are not horizontally stretched
- ✅ Mountains retain their original proportions
- ✅ Background effects can expand into widescreen space
- ✅ The original artwork is preserved

Instead of stretching the entire scene, town and landscape artwork can be moved toward the new widescreen boundary.

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

### Pirates! Unbound Widescreen

```text
+------------------------------------------+
| MENU                         SKY         |
| MENU                                     |
| MENU                              TOWN   |
|                                  TOWN    |
|          WATER                           |
+------------------------------------------+
```

The result makes better use of the additional horizontal space while retaining the proportions and appearance of the original artwork.

---

# 🌊 Extended Background Effects

**Status: ✅ Implemented**

The additional horizontal space created by widescreen displays also requires parts of the game's backgrounds and atmospheric effects to cover a larger area.

Pirates! Unbound extends suitable environmental and interface elements into this additional space.

These can include:

- Ocean
- Sky
- Clouds
- Haze
- Atmospheric gradients
- Blue menu/background effects

Recognizable artwork such as buildings and mountains is kept at its intended aspect ratio rather than being stretched to fill the screen.

Where possible, background and foreground elements are handled independently.

---

# 🏙️ Dynamic Scenery Positioning

**Status: ✅ Implemented**

Town and location artwork can be repositioned independently from the game's interface.

For example, artwork originally positioned near the right side of a 4:3 screen can now follow the right edge of a widescreen display.

The amount of movement is calculated dynamically.

This means that the scenery naturally adjusts between:

**1080p → 1440p → 4K**

without requiring separate positioning values for each resolution.

Most importantly:

> **The scenery moves. It does not stretch.**

This preserves the proportions of:

- Buildings
- Mountains
- Docks
- Ships
- Trees
- Vegetation
- Other recognizable scenery

---

# 🎨 Preserving the Original Artwork

A core rule of Pirates! Unbound is:

> **Move or extend artwork where appropriate — don't distort it.**

The following retain their original proportions:

- Towns
- Buildings
- Mountains
- Ships
- Characters
- Docks
- Vegetation
- UI icons
- Fonts

Where a scene requires additional horizontal space, the preferred approach is to reposition existing scenery and extend safe environmental/background elements.

Pirates! Unbound is not intended to replace the game's original artistic direction.

Instead, the goal is to make that artwork work naturally on modern displays.

---

# 🖱️ Original Menu & Mouse Behaviour

The widescreen modifications are designed to avoid unnecessarily interfering with the game's menu system.

Where possible:

- ✅ Menu text retains its original layout
- ✅ Selection markers remain correctly aligned
- ✅ Mouse hitboxes remain aligned with visible options
- ✅ Fonts retain their original proportions
- ✅ Menu text is not horizontally stretched
- ✅ Background scenery can move independently from the interface

This means the visual composition behind a menu can be adapted for widescreen without rebuilding the underlying menu interaction system.

---

# 📐 Resolution-Independent Design

Pirates! Unbound does not rely on a collection of individual resolution hacks.

The widescreen system calculates the available display area dynamically.

For example, the equivalent 4:3 width of a display can be calculated from its height:

```cpp
float fourThreeWidth = screenHeight * (4.0f / 3.0f);
float extraWidth = screenWidth - fourThreeWidth;
```

This allows the game to determine how much additional horizontal space is available.

| Resolution | Original 4:3 Area | Additional Width |
|---|---:|---:|
| 1920×1080 | 1440×1080 | 480 px |
| 2560×1440 | 1920×1440 | 640 px |
| 3840×2160 | 2880×2160 | 960 px |

Instead of stretching scenery across this additional width, Pirates! Unbound can reposition individual scene elements appropriately.

This allows the same system to work naturally at multiple resolutions.

---

# 🖥️ 1080p Support

**Status: ✅ Implemented**

1920×1080 is supported by the widescreen system.

At 1080p:

```text
Display:
1920×1080

Equivalent 4:3 area:
1440×1080

Additional widescreen width:
480 pixels
```

The additional horizontal space is handled dynamically by Pirates! Unbound.

---

# 🖥️ 1440p Support

**Status: ✅ Implemented**

2560×1440 is supported.

At 1440p:

```text
Display:
2560×1440

Equivalent 4:3 area:
1920×1440

Additional widescreen width:
640 pixels
```

Scene positioning scales automatically rather than relying on a separate 1440p-specific layout.

---

# 🖥️ 4K Support

**Status: ✅ Implemented**

3840×2160 is supported.

At 4K:

```text
Display:
3840×2160

Equivalent 4:3 area:
2880×2160

Additional widescreen width:
960 pixels
```

Town/location scenery and widescreen effects adjust automatically to the larger display area while preserving the proportions of the original artwork.

---

# ⚡ High Frame Rate Investigation

**Status: 🔬 Under Investigation**

High-refresh-rate support is another area being investigated for Pirates! Unbound.

Sid Meier's Pirates! was developed long before modern:

- 60 Hz
- 120 Hz
- 144 Hz
- 165 Hz
- 240 Hz

displays became commonplace.

Simply removing an old game's frame-rate restriction can cause unexpected problems if gameplay or animation logic is tied to the number of rendered frames.

For this reason, Pirates! Unbound is investigating the game's timing systems before attempting to provide high-FPS support.

---

# ⏱️ Animation & Timing Investigation

The project is investigating how Pirates! handles:

- Animation timing
- Gameplay simulation
- Character movement
- Ship movement
- Naval combat
- Sword fighting
- Dancing
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

---

# ⚠️ Why Not Just Unlock the FPS?

Older games sometimes update gameplay according to rendered frames.

For example:

```cpp
position += velocity;
```

If this was originally expected to execute 30 times per second, executing it 60 or 120 times per second could dramatically increase movement speed.

A time-independent system would instead behave more like:

```cpp
position += velocity * deltaTime;
```

For this reason, Pirates! Unbound will not simply remove frame-rate restrictions without first understanding the game's timing behaviour.

The goal of any future high-FPS implementation is:

> **Higher visual frame rates without changing the speed or behaviour of the original game.**

---

# 🔬 High-FPS Testing

Testing and reverse engineering will investigate behaviour at:

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
- Audio synchronization

If gameplay simulation is tied to frame rate, rendering and simulation may need to be separated rather than simply removing the frame limiter.

---

# 🛠️ Development Philosophy

Pirates! Unbound follows several important principles.

### Preserve the original game

This is an enhancement and compatibility project, not a remake.

### Support modern displays properly

Modern resolutions should receive properly adjusted layouts rather than simply stretching a 4:3 image.

### Avoid hard-coded resolution fixes

Where possible, calculations adapt automatically to the current resolution and aspect ratio.

### Don't stretch recognizable artwork

Repositioning original artwork is preferable to distorting it.

### Preserve the UI

Widescreen scenery modifications should not unnecessarily change menus or mouse interaction.

### Don't accidentally change gameplay

Visual improvements should not alter simulation speed, difficulty or input timing.

### Investigate before patching

Older games often contain assumptions that aren't immediately obvious.

Rendering, animation and simulation systems should be understood before invasive changes are made.

---

# 📋 Project Status & Roadmap

## Widescreen

- [x] Widescreen project foundation
- [x] Resolution-independent widescreen calculations
- [x] 1920×1080 support
- [x] 2560×1440 support
- [x] 3840×2160 (4K) support
- [x] Other compatible 16:9 resolutions through dynamic scaling
- [x] Reworked town/location screens
- [x] Dynamic scenery positioning
- [x] Extended menu/background effects
- [x] Original menu positioning preserved
- [x] Mouse/menu interaction preserved
- [ ] Ultrawide support and testing

## Performance & Compatibility

- [ ] Frame limiter investigation
- [ ] Animation timing investigation
- [ ] Gameplay timing investigation
- [ ] 60+ FPS support
- [ ] 120+ FPS testing
- [ ] High-refresh-rate support
- [ ] Additional modern-PC compatibility fixes

The core widescreen system is functional at **1080p, 1440p and 4K**, including the reworked town/location presentation.

Individual scenes and unusual aspect ratios may still require additional testing and refinement as development continues.

---

# 🧪 Current Development Status

Pirates! Unbound is under active development.

The core widescreen implementation is working, but the project should still be considered experimental while additional parts of the game are tested.

You may encounter:

- Individual scenes that still require adjustment
- Unusual aspect-ratio issues
- Rendering edge cases
- Compatibility issues
- Features that change between releases

Please report any screens that do not display correctly.

---

# 🐛 Reporting Issues

When reporting a problem, please include:

- Pirates! Unbound version
- Game version
- Resolution
- Aspect ratio
- Windows version
- GPU
- Screenshot or video if applicable
- Description of where the issue occurs

For graphical problems, please include the exact town, menu, minigame or location where the issue appears.

Screenshots are particularly useful for widescreen layout problems.

---

# 🤝 Contributions

Testing, reverse-engineering information and code contributions are welcome.

Useful information includes discoveries about the game's:

- Rendering pipeline
- Internal resolution
- Aspect-ratio handling
- UI system
- Town/location rendering
- Animation system
- Frame limiter
- Timing system
- DirectX behaviour

If you discover something useful, consider opening an issue or pull request.

---

# ⚠️ Disclaimer

**Pirates! Unbound is an unofficial fan-made project.**

It is not affiliated with, endorsed by, or associated with Firaxis Games, 2K, Sid Meier, or the original developers and publishers of Sid Meier's Pirates!.

Sid Meier's Pirates! and all associated names, artwork, trademarks and assets belong to their respective owners.

This project is intended to provide compatibility and visual improvements for legitimately owned copies of the game.

Original game assets should not be distributed with this project unless their redistribution is explicitly permitted.

---

# 🏴‍☠️ Pirates! Unbound

Sid Meier's Pirates! remains a unique game more than two decades after its original release.

**Pirates! Unbound** aims to break the game free from the display and technical limitations of its era while keeping the artwork, gameplay and character of the original intact.

### Wider seas. Modern displays. Pirates! Unbound.
