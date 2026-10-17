# Contributing and release preparation

[Back to Chaos HDR](../README.md)

Chaos HDR is being prepared for a public community source release. The original-source license and KWin/NGX linkage review remain unresolved; source visibility alone is not a license grant. Consult [third-party notices](../THIRD_PARTY_NOTICES.rst) before redistribution.

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

Development checks require explicit development compilation:

```sh
.venv/bin/python prepare_desktop.py --developer-checks
.venv/bin/python desktop_probe.py --gpu-input --controller --selection --watchdog --occlusion --frame-trace
```

The fixture uses an isolated compositor and copied settings. Build-mode changes use the same local build tree: do not switch a live production installation into development mode casually. Stop its controller first if deliberately doing that. Restore a production build and installation afterward. Never launch a real game merely for maintenance checks.

## Public packaging

Run `.venv/bin/python package_installer.py` for the non-personal source installer. Only explicitly allowlisted files are included; update that list when adding public documentation or source. `--personal` is private backup packaging, not the public release. Never publish the entire development folder.

Check payload integrity/extraction, preserved profiles, uninstall behavior on fixtures, local links, privacy and notices. Do not test purge on the development project. Use a public alias in commit metadata and keep personal identifiers out of source/history.

## Community reports and changes

Reports should describe the supported environment and reproducible behavior without private data. Separate observed results from hypotheses. Credit borrowed code precisely and preserve its required notices. A link to related work is not a claim of code reuse, endorsement or partnership.

Before publication: settle licensing/provenance, verify the real release URL and document fresh-system test results. Do not invent a download command, first-in-the-world claim, universal compatibility claim or performance guarantee.
