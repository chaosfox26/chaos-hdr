# Chaos HDR: keep this project together

This project's canonical folder is `RTX HDR experimental` in the CachyOS
workspace. The user explicitly requires that it never be scattered again.

Put all project files, dependency environments, downloaded SDKs, research
references, temporary work, test fixtures, captures, results and backups inside
this folder. Use `output/scratch/` for temporary work. Do not create project
checkouts or scratch files in `/tmp`, Downloads or sibling projects.

Keep source, settings, required runtime dependencies, upstream licenses and
useful validation records. Clean redundant generated data deliberately; never
follow Wine-prefix symlinks into the user's files during cleanup. OS-required
service registrations, desktop entries and IPC sockets reference this project
from their normal OS locations; do not duplicate project files there.

Paths contain spaces. Quote arguments and use project-relative preload library
paths with the process working directory set to the project folder. Do not
launch FFXIV for maintenance verification.

# Installation experience

The public release must install with one short, copy-and-paste terminal command.
The installer must handle downloads, supported-system prerequisite setup, native
builds, service startup and the Alt+Z shortcut without extra terminal commands,
manual file placement, executable browsing or per-game launch flags. Keep all
project data consolidated as above and preserve existing profiles on reinstall.
Use a real, verified release URL when publishing the command; do not present an
unhosted or placeholder URL as working. Clearly identify supported systems and
any necessary system authentication. A fresh supported-system end-to-end install
must be validated before calling the public one-line workflow ready.

# Release cleanliness and privacy

Keep README.md as the public front page with working links to the documentation
under docs/. Clearly credit development help from OpenAI Codex and Claude Code.
Credit direct dependencies, reference material and related projects separately;
do not imply endorsement, copied code, first/unique status or replacement of
other community work. Preserve required upstream attribution and licenses.

Keep the active release KDE-only and minimal. Explanatory first-party code
comments belong in Chaos HDR development.md. Preserve required third-party
licenses, copyright notices, shebangs and compiler/tooling directives. Document
required components and the reason for each one; do not claim comment removal
improves runtime performance. Keep historical routes in project-local archives,
not in the current build or public source payload.

Normal runtime must have no first-party telemetry/uploads, account inspection,
game-process injection, screenshot/frame persistence or diagnostic input/window
inventories. Only transient GPU/window data required for HDR and local explicit
app approvals/picture settings are allowed. Development readback and rich
diagnostics must be compiled out of production. Keep configuration/status files
owner-private. Installation dependency downloads are distinct from runtime
telemetry; do not make unverified claims about proprietary vendor internals.

Public packaging is the default, uses a reviewed file allowlist, and excludes
personal settings, captures, logs, private validation, archives, compiled
binaries and installed dependencies. Never publish the entire project folder.
A clean source-history and license/provenance review is required before release.

Provide one-command uninstall alongside install. Remove owned services, KWin
integration, shortcut and menu/link registrations. Preserve profiles/project
files by default; require explicit --purge to delete them. Test removal on
isolated fixtures; never purge this development project for a verification run.
Keep the existing direct-buffer GPU path and event-driven idle/watchdog design.
