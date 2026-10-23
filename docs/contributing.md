# Contributing and release preparation

[Back to Chaos HDR](../README.md)

Chaos HDR's original code and documentation are licensed under [GNU GPL version 2 only](../LICENSE) (`GPL-2.0-only`). Contributions must be compatible with that license, with provenance and any upstream notices preserved. Third-party components keep their own licenses. The KWin/NGX combined-binary compatibility question remains unresolved; consult [third-party notices](../THIRD_PARTY_NOTICES.rst) before redistribution. This repository's license does not grant rights to NVIDIA's proprietary components.

## Project layout and invariants

- Keep source, dependencies, builds, scratch work and validation in the canonical RTX HDR experimental folder.
- Use `output/scratch` for project scratch files and reuse the existing `.venv`.
- Preserve user settings and required third-party notices.
- Keep the direct-buffer path, synchronization and buffer lifetime correct.
- Avoid new frame copies, GPU passes, buffering or idle polling without a justified design and measurements.
- Keep explanatory first-party code comments in the [development reference](../Chaos%20HDR%20development.md); retain upstream notices and tooling directives.
- No first-party telemetry, account scanning or production frame/input recording.

## Build and checks

Normal source setup uses `bash bootstrap.sh`; it obtains dependencies, builds production targets and registers the integration. An already-provisioned tree can rebuild production with `.venv/bin/python prepare_desktop.py`. Reinstall registrations with `.venv/bin/python install.py` when needed.

Run `.venv/bin/python -m unittest test_desktop_version.py test_updates.py` to check installed,
running and recorded KWin version handling without loading an effect or changing
the live desktop. The fixture also checks that the controller's attachment command
returns an actionable error instead of a Python traceback.

Updater fixtures cover release ordering/channels, persisted daily throttling,
offline/rate-limit errors, digest verification, preserved settings, local source
changes and non-fast-forward refusal. They never install into the real project.
`release.json` records the prepared `vMAJOR.MINOR.PATCH` version and experimental
release flag; keep it aligned with CMake versions and prepared release notes.
This metadata does not establish that a tag or release has been published.

Development checks require explicit development compilation:

```sh
.venv/bin/python prepare_desktop.py --developer-checks
.venv/bin/python desktop_probe.py --gpu-input --controller --selection --watchdog --occlusion --frame-trace
```

The fixture uses an isolated compositor and copied settings. Build-mode changes use the same local build tree: do not switch a live production installation into development mode casually. Stop its controller first if deliberately doing that. Restore a production build and installation afterward. Never launch a real game merely for maintenance checks.

## Public packaging

Run `.venv/bin/python package_installer.py` for the non-personal source installer. Only explicitly allowlisted files are included; update that list when adding public documentation or source. `--personal` is private backup packaging, not the public release. Never publish the entire development folder.

Prepare release assets in a clean source staging copy inside `output/scratch`,
containing the reviewed public allowlist and required licenses. Reuse the existing
project Python environment to run that copy's packager. It writes to the copy's
`output/installers`, leaving earlier installer assets untouched. Do not upload a
mixed directory containing old launchers, personal backups or unrelated files.

Pass `--public --installer-url` with the exact versioned HTTPS GitHub release URL
for `Chaos-HDR-CachyOS.run`. For a local 0.2.0 candidate, the intended path is
`https://github.com/chaosfox26/chaos-hdr/releases/download/v0.2.0/Chaos-HDR-CachyOS.run`.
This is packaging metadata for a future asset, not a working download URL today.
Confirm its repository and tag match the intended release. Packaging generates
`install.sh` containing that URL and the current installer's SHA-256, alongside
the source archive, `.run` and dedicated `.sha256` file.

Collect only those four public files in a new version-specific asset directory,
such as `output/installers/v0.2.0`. From that directory, generate and verify the
aggregate checksums after all four files are final:

```sh
sha256sum Chaos-HDR-CachyOS.run Chaos-HDR-CachyOS-source.tar.gz Chaos-HDR-CachyOS.sha256 install.sh > SHA256SUMS
sha256sum -c SHA256SUMS
sha256sum -c Chaos-HDR-CachyOS.sha256
bash Chaos-HDR-CachyOS.run --check
```

The packager does not generate `SHA256SUMS`; never reuse an old aggregate file.
Verify the launcher's embedded digest, the dedicated checksum and aggregate entry
all identify the same `.run` bytes. Recheck archive hashes, extraction, preserved
settings and safe failure paths. Any source or documentation change requires
regenerating the package, launcher and all checksums together.

Tagging, uploading and publishing require separate authorization. Once authorized,
publish only the verified public asset set in the same release. Keep 0.1.0 assets
unchanged. Verify the hosted downloads and GitHub asset digest, then test the
actual published one-command workflow against the fresh-system acceptance criteria
before replacing the front-page install command or describing it as ready. The
launcher handles the download automatically; recipients never need to make the
source installer executable. A checksum binds the payload to the launcher;
it is not an independent signature if the publication account is compromised.

Check payload integrity/extraction, preserved profiles, uninstall behavior on fixtures, local links, privacy and notices. Do not test purge on the development project. Use a public alias in commit metadata and keep personal identifiers out of source/history.

## Community reports and changes

Reports should describe the supported environment and reproducible behavior without private data. Separate observed results from hypotheses. Credit borrowed code precisely and preserve its required notices. A link to related work is not a claim of code reuse, endorsement or partnership.

Before publication: settle licensing/provenance and record available validation
results and pending acceptance tests. After separately authorized hosting, verify
the real release URL and record the actual fresh-system test results. Do not
invent a working download command, first-in-the-world claim, universal
compatibility claim or performance guarantee.

The public one-command installation must meet the [release acceptance criteria](installation.md#release-acceptance) before being described as ready. Dependency downloads, the complete production build, startup and Alt+Z setup belong to that command; asking recipients to finish setup manually does not meet the requirement.
