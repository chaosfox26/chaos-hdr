# Troubleshooting

[Back to Chaos HDR](../README.md)

## The image does not change

- Open Alt+Z, select the intended application and explicitly allow it.
- Check Enable Chaos HDR and the panel’s selected-target message.
- An incompatible buffer stays unchanged; read the fast-path message.
- Already-HDR content should not receive another SDR-to-HDR conversion.
- Window scaling, transforms, child surfaces or color descriptions can make the current direct path ineligible. See [compatibility](compatibility.md).

## Startup or KWin update problems

The effect must match both installed and running KWin. If they differ, log out/in, then rerun setup. Do not force-load an incompatible plugin.

The existing service identifiers remain stable after renaming:

```sh
systemctl --user status truehdr-desktop.service truehdr-overlay-controls.service
```

These are user graphical-session services, not root services. The native controller reports errors locally. If logs are needed for a report, inspect and redact them before sharing; automatic uploading is not part of the app.

## Controls and screenshots

Alt+Z can conflict with another shortcut registration. The installer registers the existing shortcut helper. The panel takes keyboard/mouse input while open. Screenshot shortcuts may therefore be blocked or conflict with game input; close the panel for normal screenshot-tool use. This remains a known issue, not a completed handoff feature.

## Flicker, brightness or overlapping windows

Describe whether the issue affects only the selected app, windows above it, or the idle desktop. Record whether the panel is open. Repeated game pixels replacing a menu indicate an occlusion/draw problem, not automatically a brightness setting. Avoid changing saturation to hide a color-pipeline fault.

Check KDE HDR and configured peak brightness. The current engine accepts 400–2,000 nits. Do not assume a screenshot displayed on an SDR viewer reproduces the HDR monitor’s appearance.

## Stutter or performance changes

Keep the workload comparable and distinguish game loading/shader compilation from effect execution and presentation waits. A loaded DLSS library does not prove it is executing every frame. See [measurement guidance](performance.md).

## Removal

Run `~/.local/bin/chaos-hdr-uninstall`. If the registration is missing, use `bash uninstall.sh` inside the project. Stop/unload failures retain required files; retry after resolving the reported problem or logging out. `--purge` permanently deletes the entire project and settings. See [removal details](installation.md).
