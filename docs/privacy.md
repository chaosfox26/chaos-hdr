# Privacy and local data

[Back to Chaos HDR](../README.md)

## Normal first-party runtime

- No telemetry client, upload endpoint or remote crash-report client.
- No account inspection, game-process injection or installed-game-library inventory scan.
- No normal screenshots, saved frames or persistent input/window inventories.
- The selected GPU image is processed transiently because HDR requires image access.
- Window captions/identities are used transiently for the application chooser.
- Explicit app approvals and selected picture profiles are saved locally.
- Health/status and settings files use owner-only permissions.

This is a description of the reviewed first-party code. NVIDIA’s runtime and driver are proprietary; their internals have not been independently audited or network-monitored.

## Installation and development

Installation downloads dependencies from system/package/NVIDIA publishers, exposing ordinary network metadata to those servers. This is separate from application telemetry.

Production builds disable frame readback helpers, synthetic input hooks and rich timing/window diagnostics. Explicit development builds can record synthetic fixture results. Development captures, traces and historical research remain project-local under ignored output folders and are excluded from public source packages.

## Publication and reports

Public packaging uses an explicit source allowlist and excludes personal profiles, approved identities, captures, logs, archives, builds and installed dependencies. The personal packaging option intentionally includes settings and must not be used as the public release asset.

Privacy checks include generic identifier/credential patterns and a separate private exclusion-list cross-check. The exclusion list is not published. Automated matching cannot prove the absence of every possible secret format.

Before posting a report, remove names, account identifiers, home paths, window titles, tokens and unrelated logs. Hardware/software versions and a concise description are usually enough. Review screenshots personally before sharing them. Do not submit passwords or API keys.
