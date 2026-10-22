# Installation and removal

[Back to Chaos HDR](../README.md)

## Requirements

- CachyOS/Arch with KDE Plasma Wayland and systemd user-session support.
- A compatible NVIDIA RTX GPU and Linux driver exposing the required Vulkan and external-memory features.
- An HDR display with HDR enabled in KDE. The current TrueHDR binding accepts peak targets from 400 to 2,000 nits.
- Matching installed/running KWin versions and development headers.
- Internet access for dependencies and system authentication if build packages are missing.

The installer does not replace the GPU driver. Other distributions, desktop environments and fresh-system combinations are not certified by the existing-machine checks.

## Same implementation as the development installation

The public installer builds the same production direct-buffer KWin effect used
on the development CachyOS installation. It includes NVIDIA TrueHDR, debanding,
the full sharpening range, Alt+Z application approval and controls, named picture
profiles, event-driven startup and the active-target watchdog. It does not
substitute a shader approximation, separate preview, capture fallback or
game-injected version. NVIDIA's runtime and SDK downloads are pinned by the
installer; native components are built against the recipient's installed KWin.

Personal profiles, app approvals and development captures are not distributed.
Recipients start with clean defaults and can save their own settings. The same
implementation does not guarantee identical frame times or picture calibration
on different hardware, displays, drivers or KWin versions.

## One-command installation

The primary installation workflow is to copy one command from the GitHub front
page, paste it into a terminal and press Enter. It downloads the release and runs
setup automatically. There is no manual binary download, executable-permission
step or separate setup command. The GitHub-hosted entry point verifies the
downloaded source installer's SHA-256 before running it, and keeps temporary
downloads inside the consolidated project folder.

```sh
curl -fsSL https://github.com/chaosfox26/chaos-hdr/releases/download/v0.1.0/install.sh | bash
```

This experimental source release has not yet passed a fresh-machine installation
test. It expects a supported KDE Wayland session with working NVIDIA graphics and
HDR enabled. Missing build packages may require administrator authentication.
For an already downloaded source installer, the alternative local command is:

```sh
bash Chaos-HDR-CachyOS.run
```

It extracts reviewed source, reuses or creates the project-local dependency environment, obtains pinned NVIDIA components, builds the native effect and shortcut/controller helpers, and registers login startup. Existing picture profiles and app approvals are preserved. There are no game-folder copies or per-game launch flags.

The [GitHub release](https://github.com/chaosfox26/chaos-hdr/releases/tag/v0.1.0) also provides source and installer downloads for inspection. Integrity checking is available with `bash Chaos-HDR-CachyOS.run --check`.

## Release acceptance

The one-command installation workflow is ready only when the actual published
command has been tested on a fresh supported CachyOS/Arch KDE Wayland system.
That command must fetch the release and every required application dependency,
build the full production implementation, register and start its services, and
configure Alt+Z without manual file placement or additional setup commands.
Normal system authentication for prerequisite packages may still be required.

Record the tested system versions and verify the real TrueHDR runtime loads,
the direct-buffer path filters an explicitly approved compatible SDR window,
controls and saved profiles work, startup works after a new login, and the
documented one-command uninstall removes the integration while preserving
settings. Integrity and extraction checks on the development machine do not
substitute for this fresh-system test. Do not offer a reduced-feature runtime
as equivalent to the production implementation.

## First use

Launch an application normally. Press Alt+Z, choose its window, click Allow application and enable Chaos HDR. Opening the panel alone never approves an app. See the [user guide](user-guide.md).

## One-command uninstall

```sh
~/.local/bin/chaos-hdr-uninstall
```

Removes owned services, the loaded effect, shortcut, app-menu entry and uninstall registration. Saved profiles and project files remain. Shared system packages remain installed. If the command is unavailable, run `bash uninstall.sh` from the project folder.

To permanently delete the entire project, including settings, archives and validation records:

```sh
~/.local/bin/chaos-hdr-uninstall --purge
```

Purge is destructive. The uninstaller checks the destination and stops on component-stop/unload failures rather than deleting files required by running components.

## Files and updates

The canonical folder is `~/Documents/ChatGPT/CachyOS/RTX HDR experimental`. Source, `.venv`, `build`, `build-desktop`, settings and `output` stay inside it. OS-managed service, menu, shortcut and uninstall registrations use normal OS locations and reference this folder.

After a KWin update, run `bash bootstrap.sh` from the existing project folder to
rebuild and reinstall against the matching version. This preserves saved profiles
and app approvals. If running KWin differs from the installed version, log out/in
first. When they already match, no additional desktop restart is needed. A blocked
effect also prevents Alt+Z from showing its overlay even if both services are
running. Do not force-load a mismatched effect or edit its manifest to bypass the
check. Existing internal service/IPC names are retained for compatibility. See
[troubleshooting](troubleshooting.md#startup-or-kwin-update-problems).
