# Chaos HDR development

The [front page](README.md) links the [user guide](docs/user-guide.md),
[installation and removal](docs/installation.md), [architecture](docs/architecture.md),
[compatibility/API mapping](docs/compatibility.md), [performance methodology](docs/performance.md),
[privacy](docs/privacy.md), [troubleshooting](docs/troubleshooting.md),
[contributing](docs/contributing.md) and [credits/related methods](docs/credits.md).
This file retains the detailed implementation reference and historical explanations.

Chaos HDR is the application name. NVIDIA TrueHDR is the proprietary conversion
engine it uses. Stable internal service, shortcut-component, build-target and IPC
identifiers retain their existing spellings for installation compatibility; these
are implementation identifiers, not the public product name. The canonical
project folder remains RTX HDR experimental as required by the project rules.

Explanatory comments relocated from first-party source. Original source locations are references to the pre-cleanup files. Required upstream license notices, executable shebangs, compiler directives and runtime help text remain with their code. Third-party source is kept intact.

## Runtime boundaries

The native KWin direct-buffer path, NVIDIA TrueHDR/debanding, sharpening, synchronization, picture profiles, input handling and conditional watchdog behavior are unchanged by comment relocation. The default build selects production targets; developer fixtures are explicit. Historical routes are not part of the default build.

## KWin update recovery

The native effect must match the recorded build version, installed KWin and
running KWin. The attachment guard fails closed before loading any plugin when
these differ. Desktop updates therefore require an explicit local rebuild with
`bash bootstrap.sh`; normal login startup does not compile code, install packages
or bypass compatibility checks.

The session controller invokes `desktop.py --attach-only` so expected attachment
failures reach stderr as a concise message. Errors include the mismatched
versions and recovery step. The controller saves the end of long errors, where
the actionable exception normally appears, and logs changed failures to the user
journal. Retries remain bounded and healthy idle remains event driven.

When the effect answers its health check, Alt+Z's helper writes `overlay.toggle`
and the effect draws the controls. If the effect is unavailable, the helper opens
the native Updates & repair window instead. The application launcher provides
the same recovery entry. Its release-check, install and rebuild actions start
separate systemd user units, independent of the effect and its controller.

`updates.py` uses unauthenticated GitHub releases and existing semantic tags.
`release.json` records the installed tag and whether experimental releases apply.
The daily controller timer performs no healthy-idle polling; a private persisted
attempt timestamp and lock prevent repeated automatic network requests after
restarts or offline errors. Explicit manual checks may run sooner. The checker
notifies once per newly available tag and writes only local update status.
Installation verifies the expected repository asset URL and GitHub SHA-256
digest. Source checkouts must be clean, use the official origin and fast-forward
without dropping local commits; packaged installations retain a source backup.
Rebuild reuses existing setup and is available without a newer release.

Version-guard and updater tests use isolated manifests, profiles and mocked
provider/setup operations. They do not load an effect, install a real release or
modify the live desktop. Production capture restrictions remain unchanged.

## GitHub download entry point

`github-install.sh.in` is a release template, not a ready-to-run hosted command.
`package_installer.py --installer-url` binds it to the exact versioned GitHub
release asset and the generated source installer's SHA-256. The resulting
`output/installers/install.sh` is published with that asset. The front-page
one-liner downloads and runs this launcher; no manual download or executable
permission change is required. Setup still downloads external dependencies from
their upstream providers rather than including them in the release.

The launcher restricts downloads and redirects to HTTPS, checks the complete
installer hash before execution, keeps staging under the canonical project's
`output/scratch`, and cleans it on exit. Download errors and hash mismatches stop
setup. `--extract-only` enables isolated verification without installing services.
The checksum protects against mismatched or corrupted payloads; it is not a
separate signing authority. A real hosted command and a fresh supported-system
test are still required before claiming the one-line installation is ready.

## Relocated explanations

### CMakeLists.txt

Original line 63 (comment):

```text
# Optional prototype dependency downloaded from NVIDIA's official NGX SDK.
```

Original line 64 (comment):

```text
# Its headers/loader remain outside the source distribution.
```

### benchmark.py

Original line 1 (docstring):

```text
Compare complete DXVK frame time with and without the native HDR bridge.
```

Original line 79 (comment):

```text
# Wine's stderr formatting can interleave with the layer's progress line.
```

### compositor.py

Original line 1 (docstring):

```text
Experimental Gamescope-side TrueHDR; no TrueHDR libraries in the game.

Source installation opts in with --external; this local install uses it for
XIVLauncher. Live FFXIV verification of this route remains pending.

```

Original line 18 (docstring):

```text
LD_PRELOAD splits on whitespace; launch the process with cwd=ROOT.
```

Original line 49 (comment):

```text
# No Gamescope WSI library is needed in SDR client processes either.
```

Original line 67 (comment):

```text
# Wayland may ignore an initial window position. Make the selected connector
```

Original line 68 (comment):

```text
# SDL's primary output before initialization, then select its new index 0.
```

Original line 69 (comment):

```text
# This hint is supported by the installed SDL2-compat/SDL3 Wayland backend.
```

### compositor_benchmark.py

Original line 1 (docstring):

```text
Paired compositor throughput measurements using KDE presentation feedback.

This compares Gamescope SDR with Gamescope TrueHDR, not with direct game output.
Display cadence includes rendering and compositor work; it is not input latency.

```

Original line 17 (comment):

```text
# Alternate order to reduce systematic warmup/thermal bias.
```

### compositor_probe.py

Original line 1 (docstring):

```text
Exercise display-side filtering using an isolated Wine/DXVK fixture.

Client FPS can include frames Gamescope drops, so it is not an end-to-end
performance acceptance test. This probe verifies transport and isolation.

```

Original line 36 (comment):

```text
# Comparison fixture only: the user's FFXIV route is never changed.
```

Original line 68 (comment):

```text
# Only the fixture's prefix is stopped; never the user's live game prefix.
```

### cuda_shared.py

Original line 1 (docstring):

```text
Import Vulkan's opaque GPU memory into the CUDA context used by TrueHDR.
```

Original line 29 (comment):

```text
# CUDA's v2 wait and signal parameter structures both occupy 144 bytes.
```

Original line 30 (comment):

```text
# Opaque binary semaphores require every parameter and reserved bit zero.
```

Original line 59 (comment):

```text
# The connection owns the Vulkan allocation and its imported CUDA mapping.
```

Original line 102 (comment):

```text
# Successful cudaImportExternalMemory consumes ownership of fd.
```

### desktop.py

Original line 1 (docstring):

```text
Load and supervise the native KDE effect without launching or modifying games.
```

Original line 61 (comment):

```text
# Preserve the established effective-display metadata contract, without
```

Original line 62 (comment):

```text
# starting the older Python/CUDA conversion worker just to poll monitors.
```

### desktop_probe.py

Original line 1 (docstring):

```text
Load the desktop effect onto an already-running window in an isolated KWin.

Uses a private D-Bus session, compositor, fixture and copied profile. Never
starts FFXIV, replaces the live compositor, or sends desktop-global input.

```

Original line 163 (comment):

```text
# A frozen producer must not look like a failed HDR component.
```

Original line 172 (comment):

```text
# The effect counts draw calls, which KDE may request again
```

Original line 173 (comment):

```text
# even when the application has submitted no new buffers.
```

Original line 180 (comment):

```text
# External atomic settings replacement must work without polling.
```

### displays.py

Original line 1 (docstring):

```text
Read KDE's effective HDR display brightness, including user overrides.
```

Original line 41 (docstring):

```text
Match the OS HDR output to SDL's display index without opening a window.
```

Original line 69 (docstring):

```text
Refresh outside the render loop; short query failures keep the last reading.
```

### engine.py

Original line 1 (docstring):

```text
Shared conversion engine and live global profile for preview/game worker.
```

Original line 59 (docstring):

```text
Bounded unsharp mask with a low-detail threshold; not NVIDIA's proprietary shader.
```

Original line 65 (comment):

```text
# Suppress amplification of low-amplitude detail rather than remove grain.
```

Original line 74 (docstring):

```text
Map sRGB into BT.2020/PQ at fixed SDR white, for a valid comparison.
```

Original line 111 (comment):

```text
# Output belongs to the SDK and is overwritten by the next call.
```

Original line 119 (comment):

```text
# Upload compact bytes first. Converting pageable CPU bytes to float in
```

Original line 120 (comment):

```text
# .to(cuda, float32) created a large CPU staging copy on every frame.
```

### external_session.py

Original line 1 (docstring):

```text
Manage the external compositor until the launcher/game session finishes.
```

### install.py

Original line 1 (docstring):

```text
Install local desktop entry and session services without changing game files.
```

Original line 47 (comment):

```text
# Stop earlier transient test services so the installed units take effect.
```

Original line 67 (comment):

```text
# Remove the obsolete registration only if it is our own symlink.
```

### launch.py

Original line 1 (docstring):

```text
Launch detected games through their existing launcher, with reversible HDR setup.
```

Original line 26 (comment):

```text
# XIVLauncher's environment syntax splits on spaces, without shell quoting.
```

Original line 62 (comment):

```text
# Back up only the modified field; never copy account or login data.
```

Original line 180 (comment):

```text
# Resolve and validate the display/build before changing launcher options.
```

Original line 182 (comment):

```text
# The installed launcher cannot wrap %command% in another program. Wrap its
```

Original line 183 (comment):

```text
# session instead, removing our old AdditionalArgs injection first.
```

### library.py

Original line 1 (docstring):

```text
Discover installed games locally; never inspect login or account secrets.
```

Original line 123 (comment):

```text
# Heroic handles its existing prefix, runner, and store launch.
```

### native/d3d11_probe.c

Original line 1 (comment):

```text
// Small Windows executable to verify Wine + DXVK presentation without game login.
```

### native/desktop/bootstrap.cpp

Original line 5 (comment):

```text
// KWin's public declarative-script loader loads this small QML module. It
```

Original line 6 (comment):

```text
// registers our consolidated build as a Qt plugin search path, allowing the
```

Original line 7 (comment):

```text
// normal effect loader to find the native effect without restarting KWin,
```

Original line 8 (comment):

```text
// requiring root, or writing project binaries into system directories.
```

### native/desktop/effect.cpp

Original line 259 (comment):

```text
// Fullscreen-effect ownership releases game pointer constraints;
```

Original line 260 (comment):

```text
// interception alone can leave relative/locked input in force.
```

Original line 338 (comment):

```text
// Preserve native HDR rather than applying SDR reconstruction twice.
```

Original line 356 (comment):

```text
// Selection must not silently turn a fast direct-buffer route into
```

Original line 357 (comment):

```text
// an extra capture pass. Capture remains an explicit diagnostic only.
```

Original line 385 (comment):

```text
// The preceding convert waited for the previous Vulkan fence.
```

Original line 386 (comment):

```text
// Keep both this buffer and its explicit release point alive
```

Original line 387 (comment):

```text
// until that happens again, so the producer cannot overwrite it.
```

Original line 411 (comment):

```text
// KWin's color conversion scales reference whites even for the
```

Original line 412 (comment):

```text
// no-adaptation intent. Tag the already-absolute PQ output with
```

Original line 413 (comment):

```text
// the physical output's SDR reference to avoid boosting it again
```

Original line 414 (comment):

```text
// by the user's SDR-brightness setting (550/203 on this setup).
```

Original line 422 (comment):

```text
// KWin supplies an output-device region excluding covered pixels.
```

Original line 423 (comment):

```text
// GLTexture's hardwareClipping only sets glScissor rectangles; it
```

Original line 424 (comment):

```text
// does not enable the test. Map the region to framebuffer space as
```

Original line 425 (comment):

```text
// KWin's item renderer does, or game repaints overwrite other windows.
```

Original line 451 (comment):

```text
// Readback is restricted to an explicitly enabled private-compositor
```

Original line 452 (comment):

```text
// regression test. It never runs in the user's graphical session.
```

Original line 474 (comment):

```text
// Render into a conventional GL texture first, then let KWin apply
```

Original line 475 (comment):

```text
// its actual output transform and color conversion. Drawing ImGui
```

Original line 476 (comment):

```text
// directly ignored the output's flipped Y axis and HDR encoding.
```

### native/desktop/gpu.hpp

Original line 6 (comment):

```text
// Shares two GPU images between the current OpenGL context and a private
```

Original line 7 (comment):

```text
// Vulkan queue. No game process, display connection, or CPU pixel copy.
```

Original line 24 (comment):

```text
// A failed import leaves the currently working capture route available.
```

Original line 25 (comment):

```text
// The caller holds the producer's buffer and release point until the next
```

Original line 26 (comment):

```text
// conversion has waited for our previous completion fence.
```

### native/desktop/presentation_metrics.hpp

Original line 27 (comment):

```text
// Ignore initial frames, where the first recorded commit may precede
```

Original line 28 (comment):

```text
// instrumentation and where hot-loading has warmed resources.
```

### native/desktop/session.cpp

Original line 96 (comment):

```text
// An independent Qt timer checks the component heartbeat, not game
```

Original line 97 (comment):

```text
// frame production. Static/minimized/paused games remain healthy.
```

### native/frame_client.cpp

Original line 26 (comment):

```text
// A stalled worker must not block game presentation indefinitely.
```

### native/frame_client.hpp

Original line 6 (comment):

```text
// One connection belongs to one swapchain; callers serialize frame conversions.
```

### native/game_layer.cpp

Original line 1 (comment):

```text
// Experimental SDR swapchain proxy. Explicit opt-in only until game validation.
```

Original line 61 (comment):

```text
// Opt-in diagnostics only. Normal launches keep the driver acquire function.
```

Original line 81 (comment):

```text
// Status is disposable runtime information. Keep its replacement atomic,
```

Original line 82 (comment):

```text
// but never fsync it on the presentation thread as QSaveFile::commit does.
```

Original line 83 (comment):

```text
// User profile saves remain durable in the controls application.
```

Original line 193 (comment):

```text
// Readback from uncached host memory is very slow on discrete GPUs.
```

Original line 325 (comment):

```text
// Render-pass dependencies cover overlay writes; the game's semaphore
```

Original line 326 (comment):

```text
// makes its earlier rendering visible. Only unsignaled submissions need
```

Original line 327 (comment):

```text
// an extra source image dependency here.
```

Original line 505 (comment):

```text
// Clone the Gamescope feature-chain prefix before enabling buffer device
```

Original line 506 (comment):

```text
// addresses. Never mutate the caller's feature structs or add duplicates.
```

Original line 555 (comment):

```text
// Gamescope renames its main thread after creating the first surface.
```

Original line 556 (comment):

```text
// Use the executable identity for compositor targeting, not comm.
```

Original line 586 (comment):

```text
// The 10-bit compositor source is normalized to RGBA8 by GPUFrames.
```

Original line 587 (comment):

```text
// It must never enter the CPU bridge, whose wire format is 8-bit only.
```

Original line 593 (comment):

```text
// Gamescope writes STORAGE images. Only its virtual SDR images need
```

Original line 594 (comment):

```text
// that usage; the real output is filled by our presentation pass.
```

Original line 597 (comment):

```text
// The application still renders through its original mutable SDR views.
```

Original line 598 (comment):

```text
// Only the real presentation image changes format.
```

### native/game_overlay.cpp

Original line 51 (comment):

```text
// The surface outlives swapchains. Keep control visibility here so a
```

Original line 52 (comment):

```text
// mouse-mode change or resize cannot silently close the overlay.
```

Original line 56 (comment):

```text
// Wine renders Vulkan into an input-transparent child surface. Its
```

Original line 57 (comment):

```text
// parent receives pointer events; both carry the same HWND userdata.
```

Original line 128 (comment):

```text
// Preserve positions and presses in arrival order. Collapsing motion
```

Original line 129 (comment):

```text
// to its final position can move a queued click onto a different control.
```

Original line 139 (comment):

```text
// Reconcile state when the hidden overlay discarded prior ImGui events.
```

Original line 165 (comment):

```text
// The conversion worker refreshes KDE's configured peak while playing.
```

### native/gpu_frames.cpp

Original line 75 (comment):

```text
// Direct NGX execution stays in this Vulkan queue and never transfers
```

Original line 76 (comment):

```text
// ownership to a CUDA context. Retain memory dependencies for reuse.
```

### native/gpu_input.comp

Original line 47 (comment):

```text
// Preserve exactly the bytes used by the working buffer path.
```

### native/input_capture.cpp

Original line 1 (comment):

```text
// Input forwarding for the overlay: Wayland in the legacy game hook, SDL in
```

Original line 2 (comment):

```text
// the external compositor. The external game's environment excludes this library.
```

Original line 28 (comment):

```text
// The compositor consumes SDL events instead; blocking its Wayland events
```

Original line 29 (comment):

```text
// here would stop SDL from receiving the overlay's mouse input too.
```

Original line 38 (comment):

```text
// Always forward releases so buttons held before Alt+Z cannot get stuck.
```

### native/native_executor.hpp

Original line 10 (comment):

```text
// Wine's Unix graphics-call stack is too small for NGX initialization's 1 MiB
```

Original line 11 (comment):

```text
// local buffer. All NVIDIA calls execute on a normal Linux pthread instead.
```

### native/native_hdr.cpp

Original line 1 (comment):

```text
// Dynamic calls to NVIDIA's installed runtime; proprietary binaries stay in
```

Original line 2 (comment):

```text
// the dependency environment and are never included with this source.
```

Original line 21 (comment):

```text
// NvCVImage's documented C ABI. Init wraps an externally owned GPU buffer.
```

Original line 52 (comment):

```text
// Keep libraries loaded until process exit: NGX uses process-wide state.
```

Original line 136 (comment):

```text
// Control updates must not alter SDK state used by an earlier frame.
```

Original line 146 (comment):

```text
// Swapchain frame counters are zero-based; this counter is incremented
```

Original line 147 (comment):

```text
// before capture. Capture both ends of exactly the same frame.
```

### native/ngx_vulkan_probe.cpp

Original line 1 (comment):

```text
// Isolated capability probe. Uses NVIDIA's public NGX headers and loader;
```

Original line 2 (comment):

```text
// never changes the installed game route or bypasses runtime checks.
```

### native/panel.cpp

Original line 228 (comment):

```text
// The linked bar still needs to follow a blocked spin box.
```

### native/presentation_audit.cpp

Original line 1 (comment):

```text
// Opt-in Wayland presentation feedback, requested on the output surface only.
```

### native/profiles.hpp

Original line 9 (comment):

```text
// Named presets contain picture controls only. Display selection and automatic
```

Original line 10 (comment):

```text
// peak brightness belong to the live configuration, never to a preset.
```

### native/sdl_input_capture.hpp

Original line 1 (comment):

```text
// Input stays in the compositor. SDL events are intercepted before Gamescope
```

Original line 2 (comment):

```text
// forwards them to clients, including games using relative mouse input.
```

Original line 63 (comment):

```text
// Releases reach clients, preventing stuck keys/buttons.
```

### native/test_input_capture.cpp

Original line 1 (comment):

```text
// Exercise actual Wayland proxy userdata and the overlay event forwarding boundary.
```

### native/test_profiles.cpp

Original line 26 (comment):

```text
// Changing displays must survive preset selection.
```

Original line 41 (comment):

```text
// A corrupt store must be reported, never replaced by a fresh empty store.
```

### native/test_sdl_driver.cpp

Original line 1 (comment):

```text
// Diagnostic-only event source for our compositor fixture. It never sends
```

Original line 2 (comment):

```text
// global desktop input, loads into a game, or runs in the production path.
```

Original line 14 (comment):

```text
// SDL2-compat cannot push SDL2 text events (its converter returns nullptr).
```

Original line 15 (comment):

```text
// Push the native SDL3 text event, then let the normal SDL2 receive path
```

Original line 16 (comment):

```text
// translate it, just as it does for physical keyboard text.
```

### native/vulkan_hdr.cpp

Original line 20 (comment):

```text
// NGX searches optional features during initialization. We check each actual
```

Original line 21 (comment):

```text
// TrueHDR result; avoid flooding the game log with missing unrelated features.
```

Original line 87 (comment):

```text
// This binding matches the installed TrueHDR 1.2.3 runtime. The public NGX
```

Original line 88 (comment):

```text
// headers reserve this feature slot; they do not promise a TrueHDR Vulkan ABI.
```

### native/vulkan_hdr.hpp

Original line 6 (comment):

```text
// Opt-in NGX backend: NVIDIA's installed TrueHDR feature records its work
```

Original line 7 (comment):

```text
// directly into the compositor's Vulkan command buffer.
```

### native_reference.py

Original line 1 (docstring):

```text
Independent byte-input VFX reference for the opt-in Vulkan backend.

Runs once on a saved diagnostic frame, never in the live presentation path.
Uses the documented NvCVImage C ABI, matching the installed native backend.

```

### overlay_probe.py

Original line 1 (docstring):

```text
Exercise real compositor controls using SDL events confined to our fixture.

Uses the registered KDE shortcut action, a copied profile and a disposable Wine
prefix. No FFXIV launch, desktop mouse injection or user-setting edits.

```

Original line 20 (comment):

```text
# Keep the Unix socket path below the kernel's 108-byte limit.
```

Original line 61 (comment):

```text
# Other mouse motion can legitimately arrive before the next audit.
```

Original line 62 (comment):

```text
# Verify the actual control change below, not a stationary cursor.
```

Original line 66 (comment):

```text
# A later motion must not relocate the preceding click.
```

Original line 97 (comment):

```text
# Exercise SDL text input and backspace, rather than
```

Original line 98 (comment):

```text
# setting the name buffer directly in a test hook.
```

Original line 132 (comment):

```text
# Once closed, client input must resume. The fixture counts actual
```

Original line 133 (comment):

```text
# Windows mouse-down messages; overlay clicks must not reach it.
```

### package_installer.py

Original line 1 (docstring):

```text
Create a source installer with optional personal picture profiles.

Dependencies are fetched by bootstrap from their publishers, not redistributed.
No game files, account data, screenshots, environment or build caches are packed.

```

### pipeline_probe.py

Original line 1 (docstring):

```text
Report only graphics-related state from the running FFXIV process.
```

### prepare_desktop.py

Original line 1 (docstring):

```text
Build the default KWin route against this machine's installed KWin ABI.
```

### prepare_external.py

Original line 1 (docstring):

```text
Build the external backend separately, preserving the working game hook.

Fetches only pinned public NGX SDK files from NVIDIA's official repository.
The installed TrueHDR runtime remains a separate dependency.

```

### preview.py

Original line 1 (docstring):

```text
Animated SDR-to-HDR preview with native Wayland/Vulkan presentation.
```

### probe.py

Original line 1 (docstring):

```text
Load NVIDIA TrueHDR, process a synthetic SDR frame, and time GPU conversion.

Uses NVIDIA's official nvidia-vfx Python API. It does not capture the desktop
or change display settings. Timings include API overhead and GPU completion,
but exclude input construction, output copying, encoding, and presentation.

```

Original line 24 (comment):

```text
# Neutral ramp plus color patches: reproducible input, no private pixels.
```

Original line 68 (comment):

```text
# Y from decoded linear BT.2020 RGB, not a measurement of the monitor.
```

### route_benchmark.py

Original line 1 (docstring):

```text
Compare complete external output with the existing direct HDR route.

All modes use the same isolated DXVK fixture, scene, profile, display and
presentation feedback. Measures displayed throughput, not input latency.

```

Original line 19 (docstring):

```text
Record activity without collecting command lines or changing other apps.
```

Original line 59 (comment):

```text
# VRR feedback can report zero; any fixed period must match the
```

Original line 60 (comment):

```text
# selected output. Do not accept a 240 Hz result from the other monitor.
```

### test_desktop_watch.py

Original line 1 (docstring):

```text
Exercise the event controller against real windows in a private compositor.
```

### worker.py

Original line 1 (docstring):

```text
Local shared-memory conversion worker for the explicit Vulkan layer.
```

Original line 54 (comment):

```text
# Readiness probe disconnected without submitting a frame.
```

Original line 118 (comment):

```text
# Copy before CUDA upload so array views cannot outlive mapping.
```

Original line 179 (comment):

```text
# Each writer gets its own temporary file.
```

Original line 193 (comment):

```text
# Initialization completes before accepting any game connections.
```

Original line 199 (comment):

```text
# Exclusive lock makes stale socket removal safe after a crash or reboot.
```

### xiv_session.py

Original line 1 (docstring):

```text
Keep the external display process alive across XIVLauncher -> Dalamud -> game.

Only process identities and graphics-library presence are inspected. Launcher
output, game arguments and login data are never recorded.

```

Original line 39 (comment):

```text
# Keep the Gamescope child alive even when XIVLauncher exits after creating
```

Original line 40 (comment):

```text
# the game via Dalamud. Killing or restarting the user's Wine server is unnecessary.
```


## Current production architecture

The supported active route is the native KDE Wayland effect. KWin supplies the
selected SDR window's submitted DMA-BUF. The Vulkan bridge imports that GPU
buffer, performs the existing fused preparation/sharpening step, calls NVIDIA
TrueHDR with debanding, and exposes the HDR output for KWin's color-managed draw.
The source buffer stays protected until the queued GPU work finishes. The queue
barriers, external semaphore handoff, buffer lifetime and final clipping behavior
are retained. Do not replace this with a CPU capture/copy route or add buffering
silently. Unsupported formats, transforms or color descriptions bypass filtering.
The full added-latency requirement still needs a matched measurement; a fast
inference time alone does not prove full compositor or input latency.

`native/desktop/effect.cpp` owns window approval/selection, the GPU bridge and
in-compositor controls. `gpu.cpp` owns shared image resources and preparation.
`native/vulkan_hdr.cpp` binds the pinned NVIDIA runtime. `profiles.hpp` handles
validated local picture presets. `session.cpp` owns event-driven startup and
conditional health monitoring. `desktop.py` provides short-lived attachment and
display helpers; its daemon entry replaces itself with the native controller.
`project.py` contains only shared local paths and atomic-file writing, avoiding
imports of game-discovery or historical launcher modules at daemon startup.

The root native target contains the shortcut helper. In production its ordinary
menu invocation opens the KWin controls; it does not inspect installed games.
The shortcut service waits for Alt+Z and writes only an owner-private toggle file.
The optional standalone preview controls are compiled only for development.

## Required components and why

| Component | Why it is needed | When it is used |
|---|---|---|
| KDE Plasma Wayland / KWin with matching development headers | Owns app buffers, window selection, composition, input and HDR output. Native effect ABI must match the running compositor. | Runtime; headers at build time. |
| NVIDIA RTX GPU and compatible Linux driver | Executes NVIDIA TrueHDR and provides required Vulkan/external-memory support. The installer does not change the driver. | Runtime. |
| HDR-enabled display and KDE peak configuration | Defines the output and automatic target brightness. | Runtime. |
| NVIDIA VFX package, pinned to 0.2.0.0 and TrueHDR 1.2.3 | Supplies the proprietary Linux TrueHDR engine. Its package may contain additional vendor components; do not delete shared libraries without proving they are unnecessary. | Acquired during setup; selected native engine loaded at runtime. |
| Pinned NVIDIA NGX headers/static loader and accompanying license | Allows the native Vulkan engine binding. Feature slot 14 is experimental; public headers do not promise a stable TrueHDR Vulkan ABI. | Headers at build time; loader in the linked module. |
| Vulkan loader/headers and GLSL compiler | Records GPU preparation/inference commands and builds the existing compute shader. | Loader at runtime; headers/compiler during build. |
| Qt 6 Core/Gui/Widgets/DBus/Quick | Native settings, event notifications, local IPC, input/shortcut helper and effect-loader integration. | Runtime and build. |
| KDE CoreAddons, Config, WindowSystem and GlobalAccel | KWin plugin integration, settings and Alt+Z registration/removal. | Runtime and build. |
| libepoxy, libdrm and Wayland server interfaces | Access to compositor GPU functions, buffer attributes and explicit synchronization. | Runtime and build. |
| kscreen-doctor | Reads KDE's effective display peak on startup and relevant display/settings events. | Short-lived helper; not per frame. |
| Python 3.12 and uv | Reproducible dependency acquisition and short-lived install/attachment helpers. | Setup and lifecycle events; no Python inference loop. |
| CMake and a C++23-capable compiler | Builds against the user's installed KWin version. | Build time. |
| Dear ImGui / OpenGL backend | Draws simple HDR controls inside the compositor. Upstream MIT notices are retained. | Controls rendering. |
| systemd user services | Login startup, controller crash recovery and service lifecycle. | User graphical session; no root HDR daemon. |

SDL2/SDL3, a second Gamescope compositor, a Python Torch conversion worker and
Wayland client/protocol-scanner build requirements are no longer part of the
active default route. The existing development environment is reused; it is not
reinstalled or blindly pruned. Large files on disk are not equivalent to resident
CPU/GPU overhead. System build packages are shared with other applications and
are therefore not removed during uninstall.

## Event-driven operation and light idle behavior

Window, settings-file, display and compositor-owner notifications trigger work.
The normal controller has no recurring idle polling. The two-second watchdog is
armed only while an approved window has HDR enabled. It checks the component's
heartbeat, not whether the game produces new frames, so static, paused or minimized
content is not treated as a hang. Closing the window or disabling HDR disarms it.
The overlay refresh timer is active only for controls or an active HDR target.
A bounded retry after a failed attach is recovery work, not healthy-idle polling.

Keep shader arithmetic, texture sampling, synchronization and buffer count stable
when doing source cleanup. Relocating comments improves code readability; it is
not presented as a GPU optimization. Private fixture tests are explicitly built
with `prepare_desktop.py --developer-checks`; ordinary setup builds production
components only. Previous working source is retained in project-local compressed
archives, rather than keeping old injection/worker/launcher code in the active
release or duplicating dependencies.

## Privacy contract and boundaries

The first-party normal runtime does not send telemetry, upload files, inspect
accounts, read game command lines/environments, or capture window pixels to disk.
The GPU image is necessarily processed to perform HDR. Window captions and
identities are used transiently for the chooser; only explicitly approved app
identities and user-selected picture settings are saved locally. Those are
required configuration, not an inventory uploaded to a service.

Production status is restricted to component health, timestamp, HDR enable state,
overlay visibility, direct-path state and errors. Window classes, process IDs,
cursor positions/buttons and application-option inventories are not persisted
in the production status. Profile/status files are written owner-only. The menu
opens the controls directly; Steam/Heroic/XIVLauncher library discovery has been
removed from the active release. The active installer and uninstaller do not read launcher account/configuration
files or change game launch options. Historical configuration migration tools
remain only in the project-local source archives.

Synthetic-fixture frame readback, control screenshots, detailed timings and
occlusion/input test hooks are development-only. They are excluded or inert in
production even if diagnostic environment variables are set. Do not enable a
development build on a user's live desktop as part of ordinary installation.
NVIDIA logging is configured off, its callback discards messages, and additional
NGX logging sinks are disabled. No explicit SDK updater/telemetry API is called.
The vendor engine/driver are opaque: these source checks do not independently
certify their internal behavior. Installation downloads from publishers expose
ordinary network metadata to those servers; no first-party analytics is added.

Public packaging uses a reviewed explicit file allowlist, checks first-party
source for email/local-home-path/common credential patterns, and defaults to a
non-personal archive. Profiles, app approvals, logs, captures, development
archives, build caches, binaries and installed dependencies are excluded.
Third-party license attribution is retained. Automated pattern checks supplement
review; they are not proof that every possible secret format is detectable.

## Installation and removal contract

The public-facing install must be a short single terminal command. It must not
require manual compilation, file placement or per-game launch flags. The existing
self-extracting installer handles those operations. A hosted one-line download
command is not ready until the actual release URL exists and a clean supported
system completes setup. Check pinned source/payload hashes and preserve profiles
on rerun. Match the running KWin ABI before loading anything.

`~/.local/bin/chaos-hdr-uninstall` is the one-command removal entry point. It points
back into this project rather than containing a separate application copy. It
stops/disables all owned HDR services, verifies they stopped, unloads the effect,
cleans our shortcut component and deletes owned service/menu/link registrations.
It preserves unrelated registrations, shared packages and saved project files.
If cleanup cannot stop a component it fails before deleting its required files.
`--purge` is an explicit destructive option that also deletes this entire project
folder, including saved profiles, archives and validation records. It checks the
folder name/project markers and is tested only on temporary fixtures. Never run
it on the development installation during maintenance validation.

## Validation and remaining release gates

Meaningful checks cover: normalized AST/token equality for comment relocation;
private KWin application approval/removal/relaunch, controls, pause/static-game
watchdog behavior, no idle timer writes, effect recovery and overlapping-window
clipping; production shader equality; production exclusion of readback strings;
owner-only local status; isolated install/uninstall, preserved profiles and
unrelated registrations, failure handling, explicit purge guards; public archive
allowlist/privacy/integrity/extraction and profile-preserving reinstall.

Validation records and private research stay inside `output/` and are not copied
into the public source package. No live game is launched for these checks.
Licensing/provenance review and a fresh supported-OS installation remain release
gates. The NVIDIA engine is separately licensed; this document does not make it
open-source or grant rights to distribute a combined KWin/NGX binary.
