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

Once you have a release installer, run this from its directory:

```sh
bash Chaos-HDR-CachyOS.run
```

It extracts reviewed source, reuses or creates the project-local dependency environment, obtains pinned NVIDIA components, builds the native effect and shortcut/controller helpers, and registers login startup. Existing picture profiles and app approvals are preserved. There are no game-folder copies or per-game launch flags.

A public hosted download command is pending a real release URL. Do not substitute an invented URL. Integrity checking is available with `bash Chaos-HDR-CachyOS.run --check`.

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

After a KWin update, log out/in if the running version differs, then rerun setup to build against the matching version. Do not force-load a mismatched effect. Existing internal service/IPC names are retained for compatibility. See [troubleshooting](troubleshooting.md).
