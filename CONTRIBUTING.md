# Contributing

Build and test the layout release before submitting changes. Keep UI drawing and mouse coordinates aligned. Preserve original gameplay timing; high-FPS research is unfinished and is excluded from the public binary.

For visual bugs, report the town, scene, resolution, aspect ratio, launcher options, game version and steps to reproduce. Include a screenshot if possible. For crashes, include the error text and whether Alt-Tab or a scene transition occurred. Remove private paths from logs before sharing them.

Do not upload game binaries, memory dumps, extracted game assets, user settings or saves to this repository. Tests should use owned fixtures. Keep third-party copyright notices intact.

Changes to rendering need a live check of the affected scene, controls and outgoing transition, plus the build's coordinate, restoration, MSAA and launcher checks. State what was tested and what remains unverified.
