# Using Chaos HDR

[Back to Chaos HDR](../README.md)

## Updates and desktop repair

Open Alt+Z and choose **Updates & repair**. If the effect cannot load, Alt+Z and
the Chaos HDR application launcher open this recovery window directly.

The window shows your installed version, release channel, last check and any
connection/setup error. Automatic checks contact this project's GitHub releases
once every 24 hours at most; **Check for updates** requests an extra manual
check. A newer applicable release produces **Chaos HDR has an update** and an
**Install** button. Experimental installations include prereleases; stable
installations exclude them. No update installs without your confirmation.

After a KWin update, **Rebuild for this desktop** rebuilds the existing source
even if no new Chaos HDR release exists. It preserves saved profiles and allowed
applications and restarts only Chaos HDR. If the running desktop differs from
the installed desktop version, sign out and back in first. Future KWin changes
may still need a new Chaos HDR release.

Release installation preserves settings. A source checkout must be clean and
must fast-forward to the release without replacing local commits; otherwise the
window explains why installation stopped. Rebuild remains available for local
development work. Setup progress/errors are local under `output/updates/`.

## Choose an application

1. Start the application using its normal launcher.
2. Press Alt+Z or open Chaos HDR from the app menu.
3. Select a running application window.
4. Click Allow application and enable Chaos HDR.
5. Adjust the image, then close the panel with Done, Escape or Alt+Z.

Approval is explicit. Merely opening controls or highlighting an unapproved window leaves it unchanged. Approved window identities are remembered locally; reopening an app can select it automatically. Activating another approved window can switch the selected target. Only one window is processed at a time. Remove application revokes its remembered approval.

## Picture controls

| Control | Current range or behavior |
|---|---|
| Enable Chaos HDR | Turns processing on/off |
| Peak brightness | Automatic from KDE’s configured display; supported target 400–2,000 nits |
| Middle greys | 10–100 nits |
| Contrast | −100% to +100% |
| Saturation | −100% to +100% |
| Sharpen intensity | 0–100%; zero disables sharpening |
| Ignore Film Grain | 0–100%; reduces amplification of fine grain during sharpening |

Debanding is enabled by default to smooth visible gradient steps. Ignore Film Grain is not a general denoiser. Sharpening is our implementation, not a calibrated reproduction of every Windows NVIDIA filter value.

## Profiles

Changes save automatically. Use Save as to create a named profile and the Profile list to switch. Delete removes a custom profile and restores Global; Global itself is retained. Application approval and picture-profile selection are separate. The current implementation does not promise automatic per-application preset assignment.

Peak brightness and display selection are treated separately from reusable picture settings so presets do not overwrite display calibration.

## Other windows and screenshots

The final game draw respects KWin clipping, so menus and other windows above it are preserved. The controls themselves grab keyboard/mouse input while open. Screenshot-shortcut interaction with an open panel remains a known issue; close the panel for ordinary screenshot-tool use. We do not claim that every capture tool preserves HDR appearance.

An unsupported window remains unfiltered rather than switching silently to a slower capture path. Check the panel message and [compatibility requirements](compatibility.md).
