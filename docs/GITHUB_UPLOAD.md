# Publish the prepared project

The source folder and source ZIP are separate from the installable Windows ZIP. The Windows ZIP is the asset to attach to a GitHub release.

1. Create an empty repository named `Pirates-Unbound` under your account. Use the prepared source folder as its root; do not upload the private working directory or its Git history.
2. Add the contents of `dist/github/Pirates-Unbound` using GitHub Desktop or Git. The repository should have README.md, LICENSE, src, launcher, tests, vendor, tools, docs and .github at its root. GitHub's web uploader may omit dotfiles, so check `.gitignore` and `.github` explicitly.
3. Create the first release with tag **v0.1.0-beta.1**, title **Pirates! Unbound v0.1.0-beta.1**, and mark it **Pre-release**.
4. Paste `RELEASE_NOTES.md` into the release description.
5. Attach `Pirates-Unbound-v0.1.0-beta.1-Windows.zip` and `SHA256SUMS.txt`. Optionally attach the Source.zip; GitHub also generates source archives from the tag.

The preparation process publishes nothing and creates no remote repository. Local saves, settings, research captures, memory dumps and game assets are excluded. Do not upload the private workspace's evidence or backups.

To rebuild release assets from the public source, run `tools/build.ps1 -LayoutOnly`, then `tools/package.ps1`. The packager refuses experimental or mismatched build artifacts.
