Chaos HDR
=========

Experimental NVIDIA TrueHDR and sharpening for selected compatible SDR windows
on KDE Wayland. Enable an application through Alt+Z; ordinary game launches work.
Our code runs in KWin, outside the game process, without installing game files.

Install
-------

The release requirement is one short terminal command that downloads and runs
the installer. A public download command will be documented only after a real
release URL and a fresh-install check exist. Nothing has been published yet.

For a downloaded local installer, run::

  bash Chaos-HDR-CachyOS.run

It handles dependencies, builds the native components and enables automatic
startup and Alt+Z. System authentication may be needed for missing build packages.
It targets CachyOS/Arch, KDE Wayland, a compatible NVIDIA RTX GPU and an HDR-enabled
monitor. It builds against the installed KWin version; it does not replace the
GPU driver. Internet access is needed to download dependencies from publishers.

All project files stay in one RTX HDR experimental folder. The existing default
is ~/Documents/ChatGPT/CachyOS/RTX HDR experimental. Profiles are preserved during
reinstallation. Rebuild after a KWin update; if the running and installed versions
differ, log out and back in first.

Use
---

Press Alt+Z, select a running window and explicitly choose Allow application.
Opening the controls alone does not approve anything. Approved identities and
picture profiles stay local. Unsupported buffers remain unfiltered; the normal
build does not silently fall back to a slower capture route. Native HDR content
must not be converted a second time.

The controls provide automatic display peak brightness, middle gray, contrast,
saturation, sharpening intensity and Ignore Film Grain. Ignore Film Grain reduces
sharpening of fine grain; it is not a general denoiser. The normal app menu opens
the same controls without scanning installed game libraries.

Uninstall
---------

After installation, one command removes the integration::

  ~/.local/bin/chaos-hdr-uninstall

This stops and disables the user services, unloads the KWin effect, unregisters
Alt+Z and removes the menu entry and uninstall link. It retains the project files
and saved profiles for reinstallation. Shared system packages are left installed.

To also permanently delete the entire project folder and all saved settings::

  ~/.local/bin/chaos-hdr-uninstall --purge

That includes development archives and private validation records. Use this only
when those files are no longer wanted. If the registered command is unavailable,
run bash uninstall.sh from the project folder. The uninstaller stops on service
or effect-unload failures instead of deleting files still needed by running HDR.

Privacy
-------

The first-party normal runtime has no telemetry client, upload endpoint, account
inspection or game-process injection. It uses the selected GPU image for HDR,
transient compositor window information for selection, and local picture settings
and explicit application approvals. Production frame readback/capture helpers and
rich input/window diagnostic persistence are disabled at build time. Normal status
contains only component health and enable state, written with owner-only access.

Installation contacts system/package/NVIDIA servers for downloads. This is separate
from runtime telemetry. NVIDIA's engine and driver are proprietary dependencies;
their internals have not been independently audited. No claim of proven universal
anti-cheat approval, invisibility or ban safety is made.

Development and release status
------------------------------

See Chaos HDR development.md for the component inventory, why each dependency is
needed, processing boundaries, privacy choices, test procedures and explanations
moved out of source comments. Required third-party license notices stay intact.
Historical rendering/launcher routes are archived inside output/archives and are
excluded from release packages and the active build.

The public source packager uses an explicit allowlist and excludes personal
profiles, diagnostics, captures, archives, binaries and installed dependencies.
Public packaging is the default; --personal explicitly includes saved profiles.
NVIDIA dependencies retain their own terms. Original-source licensing and the
KWin/proprietary-NGX linkage review remain prerequisites for publication; see
THIRD_PARTY_NOTICES.rst. This is a release candidate, not a fresh-OS-certified release.
