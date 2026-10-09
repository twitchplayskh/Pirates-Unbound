# Publish v0.1.0-beta.2

## Update an existing beta.1 repository

1. Extract `Pirates-Unbound-v0.1.0-beta.2-Changed-Files.zip` into the existing repository root. It contains only added/changed complete files compared with the preserved beta.1 Source.zip, plus update metadata under `release/`. Check `release/DELETED_FILES.txt` and remove any listed paths. Check dotfiles if using GitHub's web uploader.
2. Review and commit the update. Read `docs/CODE_NOTES-beta.2.md` for implementation scope and validation. Do not upload the private workspace, evidence, saves, settings, dumps or toolchain.
3. Create tag **v0.1.0-beta.2**, title **Pirates! Unbound v0.1.0-beta.2**, and mark the release **Pre-release**.
4. Paste `RELEASE_NOTES.md` into the release description.
5. Attach **Pirates-Unbound-v0.1.0-beta.2-Windows.zip** and **SHA256SUMS.txt**. The Changed-Files.zip is an incremental repository update, not the user installation package; attach it only if you also want to offer that delta. GitHub generates complete source archives from the tag.

The delta requires the matching beta.1 tree. Its baseline archive SHA-256 and exact changed-file hashes are recorded in `release/CHANGED_FILES.json`. Do not apply it blindly to another source revision. No repository or release has been published automatically.

## Rebuild

Run `tools/build.ps1 -LayoutOnly`, then `tools/package.ps1`. Pass `-OutputDirectory` to package into a separate directory and retain earlier artifacts. The packager requires matching version/build hashes and a layout-only marker. High-FPS research remains excluded; the wider-map option is a runtime opt-in, off by default.
