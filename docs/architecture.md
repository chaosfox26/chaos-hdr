# Architecture and dependencies

[Back to Chaos HDR](../README.md)

## Processing boundary

Chaos HDR is a native KWin effect, inside the compositor process and outside the application process. It handles a completed, selected SDR window buffer. It does not translate DirectX, load a library into a game, inspect game memory or install game files.

```text
Application rendering
  -> submitted SDR GPU buffer
  -> KWin window draw hook
  -> direct DMA-BUF import into Vulkan
  -> fused sharpening/input preparation
  -> NVIDIA TrueHDR with debanding
  -> shared 10-bit PQ HDR output
  -> KWin color-managed composition
  -> display
```

Importing shared GPU memory does not render the game again. Input and HDR output resources are still necessary, as are synchronization and buffer-lifetime protection. Normal production operation has no CPU frame readback or frame-file recording.

## KWin hook map

| Hook or notification | Responsibility |
|---|---|
| `Effect::drawWindow()` | Selects the target draw, checks eligibility, processes the submitted buffer and draws HDR output |
| `Effect::paintScreen()` | Draws the controls over normal screen composition |
| `Effect::prePaintScreen()` | Attaches presentation feedback when explicitly enabled in development |
| `windowAdded`, `windowActivated`, `windowDeleted` | Selects approved windows and handles their lifecycle |
| File watchers | Reacts to picture settings, overlay triggers and relevant configuration changes |
| D-Bus owner notifications | Reacts to KWin lifecycle changes |

The implementation is in [effect.cpp](../native/desktop/effect.cpp). The direct path is in [gpu.cpp](../native/desktop/gpu.cpp); the experimental NVIDIA binding is in [vulkan_hdr.cpp](../native/vulkan_hdr.cpp). GPU ownership, acquire/release synchronization and final clipping must remain correct. The native effect blocks direct scanout while required for the filtered active window or visible controls.

## Startup and health

Systemd user services start the native session controller and shortcut helper at KDE login. The HDR effect is not a root daemon. The controller waits for file/display/compositor events instead of periodic healthy-idle polling. While HDR is enabled for a selected window, a two-second watchdog monitors component health. The effect’s heartbeat does not require new game frames, so paused content is not treated as a hang. Closing the target or disabling HDR stops the watchdog. Recovery retries are bounded.

Stable internal service, build-target and IPC names retain earlier spellings for compatibility. The public product name is Chaos HDR.

## Required components

| Component | Why it is needed |
|---|---|
| KDE KWin / Plasma Wayland | Owns submitted windows, composition, input and HDR output |
| Matching KWin headers, C++23 compiler, CMake | Builds against the installed compositor ABI |
| NVIDIA RTX GPU/driver and VFX runtime | Runs NVIDIA TrueHDR inference |
| Pinned NGX headers/static loader | Binds the experimental Vulkan feature path |
| Vulkan loader/headers and glslang | Records GPU work and compiles the preparation shader |
| OpenGL/libepoxy, libdrm, Wayland server | Integrates KWin rendering, buffer attributes and synchronization |
| Qt 6 and KDE Frameworks | Event handling, configuration, local IPC and shortcuts |
| Dear ImGui | Renders in-compositor controls |
| kscreen-doctor | Reads effective KDE display brightness metadata |
| Python 3.12 and uv | Dependency setup and short-lived lifecycle helpers; no Python inference loop |
| systemd user services | Login startup and process crash recovery |

The default route does not require a second Gamescope compositor, SDL or a Python Torch conversion worker. The existing development environment is reused. See [credits](credits.md), [third-party notices](../THIRD_PARTY_NOTICES.rst) and the [detailed development reference](../Chaos%20HDR%20development.md).
