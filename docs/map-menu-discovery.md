# Widescreen map decoration investigation

Investigation date: 2026-10-09. The original investigation below was read-only.
After the user's approval, an experimental implementation was installed and
tested; see [map-decoration-trial.md](map-decoration-trial.md). Original assets
and prepared release archives remain unchanged. FPS research remains paused.

## Finding

Reusing the full-screen menu background mechanism is possible, but it is only
one part of this problem. The map is a layered composition. Its blue backing
and the blue artwork surrounding the torn parchment edge are separate draws.
Expanding the backing does not expand the foreground cutout.

This conclusion uses original archive bytes, the previously captured executable
image, existing native draw logs and current source. No new live capture was
performed. The three references described in the request did not arrive as new
image attachments in this turn; no generated comparison was used as evidence.

## Original textures

| Layer | Stock asset | Evidence |
| --- | --- | --- |
| Blue full-screen backing in captured menu and Caribbean-map frames | `pauseMenuBG.dds`, Pak3, 1024×1024 DXT3 | Top-level compressed-pixel FNV fingerprint `93250c33d7594773` matches both existing captures exactly. Visible authored area is 1024×768. |
| Large torn map cutout | `mapEdgeMaskLarge.dds`, Pak2, 1024×512 DXT3 | Fingerprint `07687d5743b2ca34` matches map frame 15660, draw 37. Transparent upper area, blue RGB artwork below the ragged edge; pronounced raised right section. |
| Alternate torn map cutout | `mapEdgeMask.dds`, Pak6, 1024×256 DXT5 | Fingerprint `cd3a352b015adea1`; loaded alongside the large mask by the native map builder. Transparent upper area, lower ragged blue border. Its exact assignment to the governor screen still needs a labeled live capture. |
| Compass surround | `mapPanel.dds`, Pak3, 256×256 DXT5 | Fingerprint `cd4d3b45c39891a3` matches draw 38. Separate from the mask and chart. |
| Chart | Generated 1024×1024 X8R8G8B8 texture | A 1024×646 crop of the stock zoom-level BMP, separate from decoration. |

Other original menu images (`menuBG`, `menuBG1`, `e3MainMenu`) are different
assets with different fingerprints. Similar subject matter does not establish
texture identity. The captured shared menu backing is proven; the precise
startup main-menu draw has not been independently labeled in this investigation.
Do not interpret the generic menu capture as proof that every main-menu layer
uses the identical resource.

The mask contains **color artwork as well as alpha**, rather than just an alpha
stencil. Under the native-position-aligned crop hypothesis, its fully opaque
pixels differ from `pauseMenuBG` by a mean of about 31 per RGB channel. None of
those sampled pixels match exactly. This comparison does not establish the
original authoring process or an arbitrary best-fit alignment; it does rule out
assuming a seamless pixel-identical replacement at that alignment.

## Native geometry and existing widescreen behavior

The same previously captured 3840×2160 map frame records:

| Draw | Geometry and state |
| --- | --- |
| Backing, draw 2 | Four vertices, x ±320, z ±240, y −0.44; identity WORLD; 3840×2160 target and viewport. |
| Chart, draw 3 | Four vertices, x ±320, z −200..240; identity WORLD; same target/viewport. |
| Large mask, draw 37 | Local x ±512, z ±256; WORLD scale 0.625, translation (0, −0.41, −80). Effective x ±320, z −240..80. |
| Compass surround, draw 38 | Separate quad with WORLD scale 0.5, translation (237, −0.42, −122). |

The interface projection recorded before the current draw wrapper changes it
is (1.5, 2.66667, 2.5, 1), with VIEW distance 640. A full-screen 640×480 quad
qualifies for `UiDrawState(full=true)`: the wrapper temporarily sets projection
_11 to 2 and disables scissoring, then restores both states. At 16:9 this widens
that draw horizontally by 4/3. It does **not** preserve the backdrop's horizontal
artwork proportions. This is an existing patch technique, not a newly found
native widescreen renderer.

The large mask does not meet the full-screen-quad classifier. It remains in the
centered UI projection. Even when wide-chart mode permits its scissor beyond
the safe area, the original mesh still reaches only x ±320. That explains why
opening the scissor alone cannot extend this layer. Both chart and mask draws
already use the full output viewport; changing the viewport globally would
also affect the controls and is unsuitable.

The logs do not record UVs, the final post-wrapper projection/scissor, sampler
state, alpha test or depth state comprehensively. They therefore cannot prove
the precise GPU cause of the earlier failed decorative-strip experiments. Those
experiments remain rejected; this investigation does not reinstate them.

Native code evidence:

- Common menu construction loads `pauseMenuBG.dds` at 0x4B1AD1, through loader
  0x4CDDF0; nearby calls construct separate status/navigation controls.
- Native map construction at 0x4461C0 loads the large mask at 0x44636C and the
  alternate mask at 0x4463CD, using the texture/scene object constructor 0x4BDCA0.
- Native chart generator submits its independent crop quad at 0x445D2B through
  screen-quad constructor 0x4AF080. That constructor derives UVs from source
  offsets, source dimensions and texture dimensions.

## Proposed implementation

Keep the existing chart, text, icons, compass and input mapping. Add a narrowly
qualified decoration path shared by both map modes, identifying the **actual
mask texture and draw geometry**, not merely a screen name or texture size.
Use copied, owned vertices and original GPU textures; never alter original
scene objects or asset files.

The preferred first candidate is a horizontally sliced mask mesh. Preserve
the central artwork and control-adjacent contour at their native scale; allocate
extra width to selected low-detail side sections using the stock texture's UVs.
Avoid stretching the entire mask, especially its compass watermark and raised
right-hand shape. A seamless extension is not yet proven: this finite artwork
may require a small amount of localized stretching, or visible repetition.
Choose slice boundaries only after examining actual UVs and both screen variants.
Do not silently tile coastline or clamp a single edge texel across a wide strip.

Reuse the menu's **per-draw isolation and state restoration**, rather than its
unconditional horizontal stretch. For proportion-preserving backing coverage,
evaluate uniform scale-to-cover with UV cropping. This preserves artwork
proportions but crops some vertical content; it does not reveal additional
texture detail. Verify that the result remains coherent with the mask's blue
pixels. Retain the native backing if changing it introduces a color seam.

If those stock-RGB layers cannot join cleanly, an alternative to evaluate is
sampling the original mask's alpha and the original blue backing separately in
a decoration-only shader. This is rendering from original textures, not new
artwork, but changes the baked mask colors and needs explicit visual comparison.
It is a fallback proposal, not an established necessary substitution.

Neither candidate should touch any icon vertex, WORLD transform, UI projection
outside its own draw, event handler or clickable rectangle. Unknown assets,
unexpected states and unsupported screen variants must use the existing native
draw. No new timing, gameplay or FPS work is involved.

## Smallest next experiment

Obtain a labeled, read-only capture of the startup menu, Caribbean navigation
map and governor location map. For each backing and mask draw record texture
fingerprint, vertex/index data including UVs, WORLD/VIEW/PROJECTION before and
after wrappers, viewport, scissor, shaders, blending, alpha/depth and sampler
states. Confirm which mask the governor actually selects and its draw order.

Then evaluate the sliced mesh in an isolated test build before installing it.
Check seams and alpha edges at 4:3, 16:9 and 21:9, all chart zooms and both
control variants. Verify central controls and clickable coordinates numerically.
This report establishes the layered rendering mechanism; it does not certify
a finished border fix or authorize changing the working installation now.

Reproducible local evidence is under `evidence/map-menu-discovery/`; run
`tools/investigate-map-menu.py` to regenerate texture fingerprints, local stock
texture previews, numerical comparisons and code references. Extracted textures
are investigation evidence only and must not be included in public releases.
