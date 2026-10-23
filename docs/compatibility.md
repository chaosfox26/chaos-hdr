# Compatibility and API mapping

[Back to Chaos HDR](../README.md)

## Distribution and desktop testing

**The tested desktop is an existing CachyOS installation running KDE Plasma
Wayland. Other distributions and desktop environments, including GNOME, have
not been tested for installation or runtime use.**

The current effect is built for KDE's KWin compositor. GNOME and other desktops
are not confirmed compatible. Although prerequisite setup targets CachyOS/Arch,
other Arch-based installations have not been validated. Portable source checks
on a CI runner do not establish support for that runner's desktop or distribution.
Fresh-system installation and startup after a new login also remain unverified.

## Why the API does not select the hook

Chaos HDR processes the window buffer delivered to KWin. It does not add a separate DirectX/OpenGL/Vulkan hook for each application. This table documents typical routes; it is not a runtime translation layer or a completed certification matrix.

| Application API | Typical Linux rendering route | Chaos HDR boundary |
|---|---|---|
| DirectX 9 | Wine/Proton → DXVK → Vulkan | Submitted KWin window buffer |
| DirectX 10 | Wine/Proton → DXVK → Vulkan | Same |
| DirectX 11 | Wine/Proton → DXVK → Vulkan | Same |
| DirectX 12 | Proton → vkd3d-proton → Vulkan | Same |
| Native Vulkan | Vulkan presentation | Same |
| Native OpenGL | OpenGL presentation | Same |
| OpenCL compute | Requires a separate presentation path | Final displayed window, if compatible |
| VR headset output | VR-runtime/headset presentation | Not established |

Sources for translation routes: [DXVK](https://github.com/doitsujin/dxvk) and [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton). Other Wine configurations can use different routes.

## Current fast-path requirements

- A selectable application window with a suitable GPU-backed DMA-BUF.
- Single-plane RGB input accepted by the import path and driver.
- Matching buffer/draw dimensions, normal transform and an unmodified source box.
- An sRGB SDR color description and no unsupported child-surface composition.
- A fully opaque surface, either without an alpha channel or declaring its full content opaque. Alpha-capable formats alone do not disqualify an opaque application.
- A compatible NVIDIA/KWin external-memory and synchronization path.

Buffer import can still fail even when basic attributes match. The app reports the fast path as unavailable and leaves that window unchanged. It does not silently use the slower diagnostic capture route. PQ HDR content bypasses SDR conversion; other unsupported color descriptions are also left alone.

## Evidence and limits

The project has local live-use feedback for FFXIV and AION 2 and isolated synthetic-window regression checks. This is not a controlled test of every graphics API, driver, game, mod or distribution. Reports should distinguish user observation, automated fixture results and matched measurements.

Approval uses the window identity KWin supplies, not a verified executable path. Applications sharing an identity may share approval behavior. Only one selected window is filtered at a time. Native HDR, scaled buffers, multiple surfaces and VR need careful separate validation.

No game injection or file modification is performed. That architectural fact is not publisher approval or a guarantee against anti-cheat restrictions. Do not describe the effect as invisible or universally ban-safe.

Useful compatibility reports state GPU/driver/KWin versions, rendering route if known, display scaling, buffer eligibility, and what was observed. Do not post account identifiers or unredacted logs. See [privacy](privacy.md) and [performance testing](performance.md).

The 0.4.0 candidate uses bounded retries for processing failures and reconsiders
changed buffer descriptors. It does not repeatedly initialize a failing engine
on every frame. A producer fence that is not ready is bypassed for that frame.
Driver failure during resource retirement remains a limitation of running inside
the compositor; required GPU lifetime synchronization is retained. Hard device
loss must be tested on a disposable graphics session before claiming recovery.
