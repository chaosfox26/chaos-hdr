<h1 align="center">
  <img src="docs/images/chaos-hdr-title.svg" alt="Chaos HDR" width="522" height="108">
</h1>

<p align="center">
  <strong>Low-latency NVIDIA HDR and sharpening for KDE Plasma Wayland.</strong><br>
  Neon controls. Your picture settings. No game injection.
</p>

<p align="center">
  <a href="https://github.com/chaosfox26/chaos-hdr/releases/latest/download/Chaos-HDR-CachyOS.run"><img src="docs/images/download-installer.svg" alt="Download latest installer" width="230" height="48"></a>
  <a href="docs/user-guide.md"><img src="docs/images/user-guide.svg" alt="Read the user guide" width="154" height="48"></a>
</p>

<p align="center">
  <a href="#start-here">One-command install</a> ·
  <a href="#current-interface">Interface</a> ·
  <a href="#tested-gpu-processing-time">Performance</a> ·
  <a href="https://github.com/chaosfox26/chaos-hdr/releases/latest">Release notes</a> ·
  <a href="https://chaosfox26.github.io/chaos-hdr/">Website</a>
</p>

<p align="center"><img src="docs/images/neon-divider.svg" alt="" width="900" height="18"></p>

## Start here

**Tested desktop: an existing CachyOS installation with KDE Plasma Wayland.**
Other distributions and desktop environments, including **GNOME**, have not
been tested for installation or runtime use. The current effect requires KDE/KWin;
compatibility with other desktops is not established.

**Install the latest release** by pasting this command into a terminal:

```sh
curl -fsSL https://github.com/chaosfox26/chaos-hdr/releases/latest/download/install.sh | bash
```

- **Installer target:** CachyOS/Arch, KDE Plasma Wayland, compatible NVIDIA RTX GPU/driver, HDR enabled in KDE. Other Arch-based systems and fresh-machine installation remain unverified.
- **Open controls:** Alt+Z → choose a window → Allow application → choose HDR and sharpening.
- **Uninstall:** `~/.local/bin/chaos-hdr-uninstall`
- **Delete everything as well:** `~/.local/bin/chaos-hdr-uninstall --purge` — permanently deletes the complete project and saved settings.

The same command always follows the latest regular GitHub release. Its launcher pins the matching installer and verifies its checksum before setup. Setup handles dependencies, the native build and login startup on the current CachyOS/Arch KDE Wayland target. Users do not need to save an installer manually, change executable permissions or run extra setup commands. See [installation and removal](docs/installation.md) for requirements and details.

<p align="center"><img src="docs/images/neon-divider.svg" alt="" width="900" height="18"></p>

## Current interface

<p align="center">
<img src="docs/images/chaos-hdr-sidebar.png" alt="Chaos HDR sidebar with neon title, lightning background, independent HDR and sharpening switches, sliders and a hover tooltip" width="420">
</p>

The current sidebar, captured against black in an isolated test desktop using
a synthetic application. Alt+Z slides it in and out; the live lightning and
title effects are animated.

## What it does

- NVIDIA TrueHDR conversion with debanding, plus adjustable sharpening.
- Live Alt+Z controls, automatic KDE display-peak detection and named picture profiles.
- Explicit application approval without executable browsing; one selected window is processed at a time.
- Direct submitted GPU-buffer processing inside KWin, outside the application process.
- Event-driven idle operation and a watchdog only while a target has HDR enabled.
- Daily GitHub release checks, update notifications and user-initiated update/repair controls.
- No first-party telemetry, account inspection or normal frame recording.

<p align="center"><img src="docs/images/neon-divider.svg" alt="" width="900" height="18"></p>

## Tested GPU processing time

With **HDR and sharpening enabled**, the tested implementation measured the
following median GPU times. These local synthetic-scene tests recorded about
300 completed frames per mode on October 23, 2026 using 0.4.0. This table retains
that historical baseline; it is not a fresh measurement of every 0.4.3 mode.

| Resolution | Processing, sidebar closed | Processing, sidebar open | Overlay drawing, sidebar open |
|---|---:|---:|---:|
| 1440p / QHD ("2K") — 2560 × 1440 | **0.269 ms** | 0.275 ms | 0.018 ms |
| 4K / UHD — 3840 × 2160 | **0.490 ms** | 0.490 ms | 0.022 ms |

Processing includes GPU input preparation, sharpening and NVIDIA TrueHDR.
Overlay drawing is measured separately; the closed sidebar has no overlay
draw. These are GPU processing measurements, not total input-to-display latency
or a guarantee for every GPU and game. See [performance and testing](docs/performance.md)
for measurement scope and the [0.4.0 results](docs/releases/0.4.0.md) for the
baseline comparison and recovery tradeoffs.

The **0.4.3** follow-up measured **0.269 ms** median GPU processing in the live
1440p game comparison, and **0.445 ms** in a separate 4K moving-fixture check
with Vulkan validation enabled, both with the sidebar closed. Settings and test
conditions differ between those runs and the historical table. The matched
baseline comparisons showed no material regression. See the [0.4.3 results](docs/releases/0.4.3.md).

## 0.4.3 — stable HDR frame handling

A full-height, translucent Alt+Z sidebar with neon lettering, animated
lightning, smooth transitions and independent HDR and sharpening controls.
The exposed desktop stays usable, and KDE screenshots can include the open
sidebar. Optional reduced motion and accessible native controls are under **More**.

Fixes brightness flicker caused by drawing the original SDR image during a
temporary GPU readiness gap. Producer completion uses the existing GPU fence
without a duplicate CPU poll. An unfinished HDR job retains the last valid HDR
image until processing can resume. No additional frame queue, image copy or
HDR pass is introduced; a repeated image can still delay visible new content.

**[Download the latest packaged installer](https://github.com/chaosfox26/chaos-hdr/releases/latest/download/Chaos-HDR-CachyOS.run)** · [0.4.3 release notes](docs/releases/0.4.3.md)

Preserves the 0.4.2 font-rendering correction, existing picture controls and
GPU recovery safeguards. KWin retains VRR control; live presentation feedback
confirmed Adaptive Sync with HDR on and off in the tested setup.
Fresh-system installation and new-login acceptance remain unverified.

<p align="center"><img src="docs/images/neon-divider.svg" alt="" width="900" height="18"></p>

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

## About Chaos HDR

Experimental NVIDIA TrueHDR and sharpening for selected SDR application windows on KDE Wayland.

**Created by Chaos Fox with the help of OpenAI Codex.**

Chaos HDR is my implementation of the workflow I wanted: launch an application normally, open Alt+Z, explicitly allow its window, and adjust HDR without changing its files. It builds on NVIDIA’s engine and the Linux graphics community’s work. Other projects explore related goals using different methods; this project does not claim to be the first or to replace them.

**License:** Chaos HDR's original code and documentation are licensed under [GNU GPL version 2 only](LICENSE) (`GPL-2.0-only`), copyright Chaos Fox. Third-party components retain their own licenses; this grant does not relicense NVIDIA's engine, SDK or other dependencies.

**Experimental source release:** fresh-system installation validation is still pending. The KWin/NGX combined-binary licensing question remains unresolved; the source installer downloads dependencies upstream and builds locally. This release does not claim permission to redistribute the combined binary. See [third-party notices](THIRD_PARTY_NOTICES.rst). No universal compatibility or performance guarantee is implied.

## Community context

Linux HDR, compositor filtering and native Linux NVIDIA TrueHDR integrations already have prior work. Please explore [LinRTX-OBS](https://github.com/NoID1290/LinRTX-OBS), [KWin Effect Shaders](https://github.com/kevinlekiller/kwin-effect-shaders), [KWin Effect Upscale](https://github.com/JensKSP/kwin-effect-upscale), [Gamescope](https://github.com/ValveSoftware/gamescope) and [CachyOS-Auto-HDR](https://github.com/Aurion8/CachyOS-Auto-HDR). The [credits and methods comparison](docs/credits.md) explains their different roles and links further work.

These links acknowledge and help readers discover other work. They do not imply endorsement, partnership, code reuse or performance equivalence. NVIDIA TrueHDR remains NVIDIA’s separately licensed engine; Chaos HDR is an independent integration.

## Current boundaries

The fast path requires compatible buffers and leaves unsupported windows unchanged. VR output and universal anti-cheat approval are not established. The panel uses regional input handling; KDE screenshot selection takes priority over its controls. See [compatibility](docs/compatibility.md) and [troubleshooting](docs/troubleshooting.md).
