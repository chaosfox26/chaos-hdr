# Troubleshooting

[Back to Chaos HDR](../README.md)

## The image does not change

- Open Alt+Z, select the intended application and explicitly allow it.
- Check Enable Chaos HDR and the panel’s selected-target message.
- An incompatible buffer stays unchanged; read the fast-path message.
- Already-HDR content should not receive another SDR-to-HDR conversion.
- Window scaling, transforms, child surfaces or color descriptions can make the current direct path ineligible. See [compatibility](compatibility.md).

## Startup or KWin update problems

If Alt+Z stops opening the overlay after a desktop update, the shortcut service
may still be running while the native effect refuses to load. An active systemd
service does not mean the effect loaded successfully. In 0.2.0, Alt+Z and the
application launcher open Updates & repair when the effect is unavailable. Use
Rebuild for this desktop there. Earlier installations need the command below.

The effect must match both installed and running KWin. For example, an effect
built for KWin 6.8.0 is deliberately blocked after updating to 6.8.1. Keep that
compatibility check: editing the build manifest or forcing the old plugin to
load can make the compositor unstable.

From the existing Chaos HDR project folder, rebuild and reinstall with:

```sh
bash bootstrap.sh
```

This reuses the dependency environment and preserves saved profiles and app
approvals. It rebuilds the native effect and restarts only Chaos HDR's services.
When installed and running KWin already match, a desktop restart is unnecessary.
If setup reports that running KWin differs from installed KWin, log out/in when
convenient, then rerun setup. Chaos HDR does not automatically rebuild or install
packages during normal startup.

The existing service identifiers remain stable after renaming:

```sh
systemctl --user status truehdr-desktop.service truehdr-overlay-controls.service
journalctl --user -b -u truehdr-desktop.service --no-pager
```

These are user graphical-session services, not root services. The native controller reports errors locally. If logs are needed for a report, inspect and redact them before sharing; automatic uploading is not part of the app.

The local `output/desktop/controller-status.json` must report `ready: true` after
successful attachment; `build-desktop/install-manifest.json` records the KWin
version used for the build. After setup, verify that Alt+Z opens the controls and
an explicitly approved compatible SDR window is processed. A successful rebuild
alone does not establish conversion for every application.

## Controls and screenshots

Alt+Z can conflict with another shortcut registration. The installer registers the existing shortcut helper. The sidebar should leave the exposed taskbar clickable and allow KDE screenshot shortcuts. Spectacle's region selector takes input priority even over the sidebar. If an older installation blocks these interactions, rebuild or update to the version containing regional input handling. For a capture problem, report the shortcut, capture tool and whether the problem occurs while selecting or only in the saved image.

## Flicker, brightness or overlapping windows

Describe whether the issue affects only the selected app, windows above it, or the idle desktop. Record whether the panel is open. Repeated game pixels replacing a menu indicate an occlusion/draw problem, not automatically a brightness setting. Avoid changing saturation to hide a color-pipeline fault.

Check KDE HDR and configured peak brightness. The current engine accepts 400–2,000 nits. Do not assume a screenshot displayed on an SDR viewer reproduces the HDR monitor’s appearance.

## Stutter or performance changes

Keep the workload comparable and distinguish game loading/shader compilation from effect execution and presentation waits. A loaded DLSS library does not prove it is executing every frame. See [measurement guidance](performance.md).

## GPU recovery requires signing out

This message means Chaos HDR could not confirm that GPU resources were safe to
release. It stops processing and retains those resources instead of repeatedly
rebuilding. The setting is shared across effect reloads in the current compositor
session; changing profiles or updating the effect will not clear it. Save your
work, then sign out of KDE and back in when convenient. Your picture settings
are preserved. Chaos HDR never signs you out or restarts the compositor itself.

Temporary completion delays can recover automatically. A driver hang can still
stall the shared GPU and KWin before the recovery timer runs. If the desktop is
unresponsive, this safeguard cannot guarantee that controls remain usable.

## Removal

Run `~/.local/bin/chaos-hdr-uninstall`. If the registration is missing, use `bash uninstall.sh` inside the project. Stop/unload failures retain required files; retry after resolving the reported problem or logging out. `--purge` permanently deletes the entire project and settings. See [removal details](installation.md).

## Interrupted updates

The 0.4.0 updater builds before activation and retains a private recovery journal.
On restart it reconciles an interrupted operation under the operation lock. If
restoring the previous build also fails, it retains that journal and backup;
resolve the reported desktop/build incompatibility and retry. Do not delete the
backup while recovery is pending. Installed version reporting uses the successful
activation marker rather than merely the source file's version.

## Motion, fonts or text navigation

Use More → Reduce motion to stop the animated background and title. Use
More → Accessible controls for native text input, complex scripts and the
platform accessibility interface. Missing characters in the overlay can require
an installed font covering that script. Clipboard helpers inherit the current
Wayland session; a timed-out paste leaves the editor unchanged.
