# Chaos HDR

Experimental NVIDIA TrueHDR and sharpening for selected SDR application windows on KDE Wayland.

**Created by Chaos Fox with the help of OpenAI Codex and Claude Code.**

Chaos HDR is my implementation of the workflow I wanted: launch an application normally, open Alt+Z, explicitly allow its window, and adjust HDR without changing its files. It builds on NVIDIA’s engine and the Linux graphics community’s work. Other projects explore related goals using different methods; this project does not claim to be the first or to replace them.

**License:** Chaos HDR's original code and documentation are licensed under [GNU GPL version 2 only](LICENSE) (`GPL-2.0-only`), copyright Chaos Fox. Third-party components retain their own licenses; this grant does not relicense NVIDIA's engine, SDK or other dependencies.

**Experimental release preparation:** a public download URL and fresh-system installation validation are still pending. The KWin/NGX combined-binary licensing question remains unresolved; the source installer downloads dependencies upstream and builds locally. This release does not claim permission to redistribute the combined binary. See [third-party notices](THIRD_PARTY_NOTICES.rst). No universal compatibility or performance guarantee is implied.

## Start here

- **Install a downloaded release:** `bash Chaos-HDR-CachyOS.run`
- **Open controls:** Alt+Z → choose a window → Allow application → Enable Chaos HDR.
- **Uninstall:** `~/.local/bin/chaos-hdr-uninstall`
- **Delete everything as well:** `~/.local/bin/chaos-hdr-uninstall --purge` — permanently deletes the complete project and saved settings.

The installer handles dependencies, the native build and login startup on the current CachyOS/Arch KDE Wayland target. A short download-and-install command will be added when its real release URL is verified. See [installation and removal](docs/installation.md) for requirements and details.

## What it does

- NVIDIA TrueHDR conversion with debanding, plus adjustable sharpening.
- Live Alt+Z controls, automatic KDE display-peak detection and named picture profiles.
- Explicit application approval without executable browsing; one selected window is processed at a time.
- Direct submitted GPU-buffer processing inside KWin, outside the application process.
- Event-driven idle operation and a watchdog only while a target has HDR enabled.
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
