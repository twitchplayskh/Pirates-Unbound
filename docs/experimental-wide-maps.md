# Experimental wider maps

The v0.1.0-beta.2 launcher has an **Experimental: wider Caribbean and governor maps** checkbox. It is off by default and applies on the next launch. Enable the widescreen patch, centered UI and filled backgrounds first.

This displays additional coastline from the installed game's original Caribbean map images. It keeps the native chart scale, city labels, markers, compass, toolbar and mouse-coordinate mapping. It reads the installed FPK archives; no game images are included in the project or distributable.

The map's finite image boundaries use parchment instead of wrapping coastlines. Supported native zoom levels are 2, 4 and 6. Expansion is limited to aspect ratios above 4:3 and at most 8:3; unsupported configurations retain the existing map presentation.

The option remains experimental. The runtime extends the two verified stock blue cutout textures with reflected side sections. The central geometry, compass, toolbar and click coordinates keep their native placement. A four-pixel UV inset at each edge omits baked dark stripes; this crops 8 of the texture's 1024 columns, changing its horizontal sample scale by less than 1%. Texture addressing is clamped for this draw and restored afterward. The navigation cutout's raised right edge repeats in the added strip; this is a visible tradeoff of reusing the finite original artwork. It needs further review before making the option a release default.

The runtime checks the original chart crop against the stock image before substituting its owned wider texture. A failed comparison, unsupported image, missing asset or unexpected resource uses the original map path. No gameplay timing or high-FPS changes are involved.

Included as an opt-in experiment in v0.1.0-beta.2. The previous beta.1 archives are preserved locally.
