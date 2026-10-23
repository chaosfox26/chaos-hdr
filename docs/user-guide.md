<p align="center">
  <a href="../README.md"><img src="images/chaos-hdr-title.svg" alt="Chaos HDR — home" width="435" height="90"></a>
</p>

<p align="center">
  <a href="../README.md">Home</a> ·
  <a href="installation.md">Install</a> ·
  <strong>User guide</strong> ·
  <a href="../README.md#documentation">All docs</a> ·
  <a href="https://chaosfox26.github.io/chaos-hdr/">Website</a>
</p>

<p align="center"><img src="images/neon-divider.svg" alt="" width="900" height="18"></p>

# Using Chaos HDR

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
4. Click Allow application, then enable HDR, Sharpening, or both.
5. Adjust the image, then close the panel with its top-right arrow, Escape or Alt+Z.

Approval is explicit. Merely opening controls or highlighting an unapproved window leaves it unchanged. Approved window identities are remembered locally; reopening an app can select it automatically. Activating another approved window can switch the selected target. Only one window is processed at a time. Remove application revokes its remembered approval.

## Picture controls

| Control | Current range or behavior |
|---|---|
| HDR | Enables HDR conversion independently of sharpening |
| Sharpening | Enables sharpening independently of HDR |
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

The sidebar slides in from the left and spans the full output height, including the taskbar area beneath it. Its translucent background keeps the application visible beneath the animated lightning. Hover over a control for its explanation.

Clicks outside the sidebar reach the desktop, including exposed parts of the taskbar. A slider drag stays with its control until you release it, even if the pointer leaves the sidebar. Clicking another application returns typing to it.

KDE screenshot shortcuts remain available while the sidebar is open, including Print Screen and Meta+Shift+S where configured. Spectacle can capture the sidebar and the application together. During region selection, Chaos HDR yields input and stops drawing over the selection tool; the captured image already contains the sidebar. It resumes afterward. The final game draw respects KWin clipping. Screenshot color depends on the capture tool and viewer; an SDR screenshot may not reproduce an HDR display's appearance.

An unsupported window remains unfiltered rather than switching silently to a slower capture path. Check the panel message and [compatibility requirements](compatibility.md).

## Accessibility and text editing

Open **More → Reduce motion** for a static wordmark/lightning background and
instant opening and closing. The default retains the animated Chaos HDR design.

**More → Accessible controls** opens a native Qt controls window with labelled
fields, keyboard navigation and the platform accessibility interface. It also
appears as an action on the application launcher. It includes application
approval, profiles, HDR and sharpening. It follows the desktop's text settings;
full screen-reader validation is still part of release acceptance.

Profile names support Unicode. The overlay uses installed fallback fonts for
characters absent from the brand fonts. Complex text shaping is handled by the
native controls window. In the overlay name field, Ctrl+A/C/X/V and normal
navigation keys work. Clipboard access happens only when you request copy,
cut or paste; clipboard contents are not written to a project file.

## Display changes and recovery

Move the selected application between screens and its effective brightness
follows the screen receiving the window. An unavailable or SDR output bypasses
HDR without switching off a separately enabled sharpening setting. Your saved
picture profile does not overwrite KDE's display calibration.

Unsupported translucent windows stay unchanged. A temporary processing failure
gets a small number of automatic retries. **Retry processing** can request a new
attempt; removing and allowing the application again is also available.
Disconnecting the screen hosting the sidebar closes it and releases its input
and cursor ownership. Open it again with Alt+Z on an available screen.
