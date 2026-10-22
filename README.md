# Chaos HDR

## 0.2.0 release notes — KWin 6.8.1 compatibility

**Restores compatibility with KWin 6.8.1 after the 6.8.0-to-6.8.1 desktop update.**
The native effect can be rebuilt for the updated desktop, restoring effect
loading and the Alt+Z overlay while keeping the compatibility guard in place.

- **Updates & repair in Alt+Z:** access release updates and desktop repair from
  the menu. If the effect cannot load, Alt+Z and the application launcher open
  an independent recovery window.
- **Daily GitHub release checks:** automatic checks run at most once every
  24 hours, including after failed attempts or login restarts. Manual checks
  show the last check and any connection error.
- **Update notifications:** newer applicable releases display “Chaos HDR has an
  update.” Experimental installations include prereleases; stable installations
  exclude them.
- **User-controlled installation:** updates install only after confirmation and
  verify the release installer. Local source changes and commits are protected.
- **Rebuild for this desktop:** repair desktop compatibility without waiting for
  a new Chaos HDR release. Saved profiles and allowed applications are preserved;
  only Chaos HDR's services restart.
- **Clear startup errors:** mismatched versions and recovery steps appear in the
  user service log.

HDR conversion, debanding and sharpening algorithms are unchanged. Future KWin
API changes may still require source changes; a rebuild cannot guarantee every
future desktop version will work. If running and installed KWin differ, sign
out and back in before rebuilding.

**Publication status:** the 0.2.0 implementation is merged; release verification
and separate publication authorization are still required. The download command
below remains pinned to the published 0.1.0 release and does not install 0.2.0.
Local automated tests and a production rebuild on KWin 6.8.1 passed. Fresh-system
installation, startup after a new login, real-application HDR conversion and
installation of a newer hosted release remain unverified.
[Full 0.2.0 release notes](docs/releases/0.2.0.md).

## About Chaos HDR

Experimental NVIDIA TrueHDR and sharpening for selected SDR application windows on KDE Wayland.

**Created by Chaos Fox with the help of OpenAI Codex and Claude Code.**

Chaos HDR is my implementation of the workflow I wanted: launch an application normally, open Alt+Z, explicitly allow its window, and adjust HDR without changing its files. It builds on NVIDIA’s engine and the Linux graphics community’s work. Other projects explore related goals using different methods; this project does not claim to be the first or to replace them.

**License:** Chaos HDR's original code and documentation are licensed under [GNU GPL version 2 only](LICENSE) (`GPL-2.0-only`), copyright Chaos Fox. Third-party components retain their own licenses; this grant does not relicense NVIDIA's engine, SDK or other dependencies.

**Experimental source release:** fresh-system installation validation is still pending. The KWin/NGX combined-binary licensing question remains unresolved; the source installer downloads dependencies upstream and builds locally. This release does not claim permission to redistribute the combined binary. See [third-party notices](THIRD_PARTY_NOTICES.rst). No universal compatibility or performance guarantee is implied.

## Start here

- **Install:** paste this one command into a terminal and press Enter:

```sh
curl -fsSL https://github.com/chaosfox26/chaos-hdr/releases/download/v0.1.0/install.sh | bash
```

- **Supported target:** CachyOS/Arch, KDE Plasma Wayland, compatible NVIDIA RTX GPU/driver, HDR enabled in KDE. Fresh-machine installation is not yet verified.
- **Open controls:** Alt+Z → choose a window → Allow application → Enable Chaos HDR.
- **Uninstall:** `~/.local/bin/chaos-hdr-uninstall`
- **Delete everything as well:** `~/.local/bin/chaos-hdr-uninstall --purge` — permanently deletes the complete project and saved settings.

The public command downloads and runs setup directly from GitHub, then handles dependencies, the native build and login startup on the current CachyOS/Arch KDE Wayland target. Users do not need to save an installer manually, change executable permissions or run extra setup commands. See [installation and removal](docs/installation.md) for requirements and details.

## What it does

- NVIDIA TrueHDR conversion with debanding, plus adjustable sharpening.
- Live Alt+Z controls, automatic KDE display-peak detection and named picture profiles.
- Explicit application approval without executable browsing; one selected window is processed at a time.
- Direct submitted GPU-buffer processing inside KWin, outside the application process.
- Event-driven idle operation and a watchdog only while a target has HDR enabled.
- Daily GitHub release checks, update notifications and user-initiated update/repair controls.
- No first-party telemetry, account inspection or normal frame recording.

## Documentation

| Page | What you will find |
|---|---|
| [Installation and removal](docs/installation.md) | Requirements, one-command setup/removal, updates and file locations |
| [User guide](docs/user-guide.md) | Window approval, controls, profiles and everyday operation |
| [Architecture and dependencies](docs/architecture.md) | Exact KWin hooks, GPU data flow and why components are needed |
| [Compatibility and API mapping](docs/compatibility.md) | Graphics routes, fast-path requirements and testing limits |
| [Performance and testing](docs/performance.md) | What measurements mean and how to make useful comparisons |
| [Privacy](docs/privacy.md) | Transient data, saved configuration and production/development boundaries |
| [Troubleshooting](docs/troubleshooting.md) | Unsupported windows, KWin updates, services and known issues |
| [Contributing](docs/contributing.md) | Source layout, development checks and release practices |
| [Credits and related projects](docs/credits.md) | Dependencies, reference material and alternatives using different methods |
| [Development reference](Chaos%20HDR%20development.md) | Detailed component notes and explanations moved from source comments |
| [Third-party notices](THIRD_PARTY_NOTICES.rst) | Upstream terms and unresolved licensing boundaries |

## Community context

Linux HDR, compositor filtering and native Linux NVIDIA TrueHDR integrations already have prior work. Please explore [LinRTX-OBS](https://github.com/NoID1290/LinRTX-OBS), [KWin Effect Shaders](https://github.com/kevinlekiller/kwin-effect-shaders), [KWin Effect Upscale](https://github.com/JensKSP/kwin-effect-upscale), [Gamescope](https://github.com/ValveSoftware/gamescope) and [CachyOS-Auto-HDR](https://github.com/Aurion8/CachyOS-Auto-HDR). The [credits and methods comparison](docs/credits.md) explains their different roles and links further work.

These links acknowledge and help readers discover other work. They do not imply endorsement, partnership, code reuse or performance equivalence. NVIDIA TrueHDR remains NVIDIA’s separately licensed engine; Chaos HDR is an independent integration.

## Current boundaries

The fast path requires compatible buffers and leaves unsupported windows unchanged. VR output and universal anti-cheat approval are not established. The panel’s input grab can still interfere with screenshot shortcuts while it is open. See [compatibility](docs/compatibility.md) and [troubleshooting](docs/troubleshooting.md).
