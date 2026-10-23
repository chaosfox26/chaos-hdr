# Privacy and local data

[Back to Chaos HDR](../README.md)

## Normal first-party runtime

- No telemetry client, upload endpoint or remote crash-report client.
- No account inspection, game-process injection or installed-game-library inventory scan.
- No normal screenshots, saved frames or persistent input/window inventories.
- The selected GPU image is processed transiently because HDR requires image access.
- Window captions/identities are used transiently for the application chooser.
- Explicit app approvals, selected picture profiles and the reduced-motion preference are saved locally.
- Copy, cut and paste use an on-demand native clipboard helper on explicit user input; clipboard text is not logged or saved by Chaos HDR.
- Health/status and settings files use owner-only permissions.

The release checker makes unauthenticated requests to this project's public
GitHub releases, at most once per 24 hours automatically and additionally when
you request a manual check or installation. These requests expose ordinary
network metadata to GitHub, but send no profiles, app/window identities, account
data or diagnostic uploads. Check timestamps, release availability and setup
logs stay under the project's private `output/updates/` folder. Installing an
update downloads its verified release asset; rebuilding may fetch prerequisites
through the existing installer. Neither action runs silently.

This is a description of the reviewed first-party code. NVIDIA’s runtime and driver are proprietary; their internals have not been independently audited or network-monitored.

## Installation and development

Installation downloads dependencies from system/package/NVIDIA publishers, exposing ordinary network metadata to those servers. This is separate from application telemetry.

Production builds disable frame readback helpers, synthetic input hooks and rich timing/window diagnostics. Explicit development builds can record synthetic fixture results. Development captures, traces and historical research remain project-local under ignored output folders and are excluded from public source packages.

## Publication and reports

Public packaging uses an explicit source allowlist and excludes personal profiles, approved identities, captures, logs, archives, builds and installed dependencies. The personal packaging option intentionally includes settings and must not be used as the public release asset.

Privacy checks include generic identifier/credential patterns and a separate private exclusion-list cross-check. The exclusion list is not published. Automated matching cannot prove the absence of every possible secret format.

Private names, email addresses and exclusion-list values must not appear in `.gitignore` or any tracked file. Ignore rules contain generic paths, not personal identifiers. Release checks examine the actual source, package and reachable Git history; ignoring a file is not redaction of previously committed content. Required upstream attribution remains intact.

The author explicitly permits their public GitHub handle in repository and
installation URLs. That limited exception does not allow other private
identifiers, account details or exclusion-list entries to be published.

Before posting a report, remove names, account identifiers, home paths, window titles, tokens and unrelated logs. Hardware/software versions and a concise description are usually enough. Review screenshots personally before sharing them. Do not submit passwords or API keys.
