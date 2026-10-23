<p align="center">
  <a href="../README.md"><img src="images/chaos-hdr-title.svg" alt="Chaos HDR — home" width="435" height="90"></a>
</p>

<p align="center">
  <a href="../README.md">Home</a> ·
  <a href="installation.md">Install</a> ·
  <a href="user-guide.md">User guide</a> ·
  <a href="../README.md#documentation">All docs</a> ·
  <a href="https://chaosfox26.github.io/chaos-hdr/">Website</a>
</p>

<p align="center"><img src="images/neon-divider.svg" alt="" width="900" height="18"></p>

# Architecture and dependencies

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

The session controller schedules a short-lived Python release checker using a
single-shot daily timer. A private persisted attempt timestamp and operation
lock enforce the 24-hour automatic limit across restarts and failures; manual
checks are explicit. Network requests and setup run outside KWin. The effect
reads only local update status and links to the native Updates & repair window.
The shortcut/launcher can open that window even when the effect is unavailable.
User-requested setup runs in a separate systemd user unit so restarting Chaos
HDR's own services does not terminate the installer. No desktop restart occurs.

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
| KWin output metadata | Supplies the selected screen’s current HDR state and effective peak directly to the effect |
| Fontconfig and installed fonts | Supplies missing characters for profile names and application titles |
| kscreen-doctor | Installation/display checks and isolated development fixtures |
| Python 3.12 and uv | Dependency setup and short-lived lifecycle helpers; no Python inference loop |
| systemd user services | Login startup and process crash recovery |
| libnotify / notify-send | Local update-available and setup-result notifications |

The default route does not require a second Gamescope compositor, SDL or a Python Torch conversion worker. The existing development environment is reused. See [credits](credits.md), [third-party notices](../THIRD_PARTY_NOTICES.rst) and the [detailed development reference](../Chaos%20HDR%20development.md).

## Settings and deployment in 0.4.0

The effect derives one effective peak from the selected window's current KWin
output, preferring the configured override to the display's advertised peak.
The UI, NVIDIA settings and output color description use that value. Native
output-change notifications refresh it; stale profile connector names do not
choose the rendering display. HDR is bypassed on an SDR output or outside the
supported peak range, while sharpening can remain available.

GPU recording receives a small in-memory picture-settings value. It performs no
profile-file checks. Slider changes apply immediately in memory; persistence is
debounced outside painting. A local write-ahead record lets a restart finish an
interrupted write of the current profile and named-profile collection.

Each completed build has an immutable directory under `output/builds/` whose
identity includes native code, shaders, artwork, fonts, ImGui, build definitions
and the KWin version. The `build-desktop` and `build` paths select the active
artifacts. Verified release sources build in staging before source activation;
a recovery journal preserves prior sources and artifact links. The installed
release marker changes after setup succeeds. A failed or interrupted activation
can restore the previous build, and the updater reconciles stale busy states
when it owns the operation lock.

The active sidebar allocates a texture covering its controls, popups and cursor,
with damage bounded to that part of its output. Direct DMA-BUF processing does
not allocate the diagnostic capture texture. Unsuitable alpha surfaces are left
unchanged. The preceding HDR job is checked without a CPU wait before reusing
its command resources. The submitted producer fence is imported into the
existing Vulkan semaphore wait; a pending producer is not rejected by a separate
CPU poll. If the preceding HDR job is unfinished, the last valid HDR output is
redrawn with its original color interpretation. OpenGL releases that output only
before the next conversion, after its final read. Before the first valid result,
the ordinary window path remains available. This retains one conversion job and
one output image, with no additional frame queue, copy or processing pass.

KWin retains presentation and VRR policy control. The effect does not override
refresh timing, force VRR or add a frame pacing timer. Development presentation
feedback records KWin's actual presentation mode; a configured Automatic policy
alone does not prove that adaptive presentation is active.

GPU cleanup polls the last submitted Vulkan fence and a GL fence inserted only
at retirement. The source buffer and its release point remain retained until
both have completed. Only one context can retire at a time; rebuilding waits
for its completion. A pending active submission gets a one-second watchdog,
and retirement has a separate one-second deadline. Unresolved resources are
retained until compositor exit, and GPU processing stays disabled for that
compositor session, including after effect reload. Normal managed unload first
asks the effect to retire; unexpected unload retains any unresolved resources.
This removes explicit blocking teardown waits without adding per-frame fences,
copies or queues. It does not isolate KWin from a driver hang on the shared GPU.

Build identity includes first-party/native sources, bundled assets and fonts, ImGui, SDK download metadata, KWin binary identity, relevant package/compiler versions and build flags. Staging leaves the active manifest and plugin untouched. Both packaged setup and the in-app updater share the same activation transaction; a failed load restores the prior source/manifest/links before restarting the previous integration.
