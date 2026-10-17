# Credits and related projects

[Back to Chaos HDR](../README.md)

Chaos HDR is Chaos Fox’s implementation of a particular HDR workflow. It was made with the help of **OpenAI Codex and Claude Code**. It integrates existing technology; it does not claim authorship of NVIDIA TrueHDR, KWin, Linux HDR or compositor filtering.

The projects below deserve recognition for their own work and give readers alternatives to explore. Links do not imply endorsement, partnership, code reuse or permission to relicense their work. Related projects use different methods and are not all implementations of NVIDIA’s model. We make no claim to being first or unique.

## Components directly used

| Component and maintainers | Role in Chaos HDR | Attribution / terms |
|---|---|---|
| [NVIDIA TrueHDR / Maxine VFX](https://developer.nvidia.com/maxine) | Proprietary HDR conversion engine | Separate vendor terms; not made open-source by this integration |
| [NVIDIA NGX / DLSS SDK](https://github.com/NVIDIA/DLSS) | Pinned Vulkan headers and static loader | Accompanying NVIDIA agreement retained with downloaded files |
| [KDE KWin](https://invent.kde.org/plasma/kwin) | Compositor effect boundary, window buffers, color management and input | Upstream GPL terms; linkage review pending |
| [Qt](https://www.qt.io/) and [KDE Frameworks](https://develop.kde.org/products/frameworks/) | Native events, configuration, local IPC and shortcuts | Their upstream terms; supplied by system packages |
| [Dear ImGui](https://github.com/ocornut/imgui) | In-compositor controls | Unmodified vendored source; [MIT notice](../vendor/imgui/LICENSE.txt) and [pinned provenance](../vendor/imgui/UPSTREAM.txt) |
| [Vulkan / Khronos](https://www.vulkan.org/) | GPU commands, external memory and synchronization interfaces | System loader/header/compiler dependencies retain upstream terms |
| [Linux DRM](https://dri.freedesktop.org/) and [Wayland](https://wayland.freedesktop.org/) | Graphics-buffer and display-server interfaces | Provided by the Linux graphics stack |
| [libepoxy](https://github.com/anholt/libepoxy) | OpenGL function access | Upstream project and system package |
| [systemd](https://systemd.io/) | User-session startup and process recovery | System service manager |
| [Python](https://www.python.org/), [uv](https://github.com/astral-sh/uv), [CMake](https://cmake.org/) and [glslang](https://github.com/KhronosGroup/glslang) | Setup, dependency acquisition, builds and shader compilation | Upstream tools; not original Chaos HDR components |

The complete distinction between code bundled here, downloaded proprietary dependencies and system-provided libraries is recorded in [third-party notices](../THIRD_PARTY_NOTICES.rst) and [architecture](architecture.md). Required copyright and license notices remain intact.

## NVIDIA reference material

- [Official NVIDIA VFX Python samples](https://github.com/NVIDIA-Maxine/nvidia-vfx-python-samples): demonstrations of NVIDIA Video Effects SDK bindings.
- [Official TrueHDR sample application](https://github.com/NVIDIA-Maxine/VFX-SDK-Samples/tree/main/apps/TrueHDREffectApp): an engine-level reference, distinct from a KWin desktop effect.
- [ComfyUI NVIDIA VFX nodes](https://github.com/Comfy-Org/ComfyUI_Nvidia_VFX_Nodes): another way to use NVIDIA VFX effects in media workflows.

These establish that Linux access to NVIDIA’s engine and non-desktop uses already exist. Sample licenses do not relicense NVIDIA’s proprietary runtime.

## Related methods and alternatives

| Project | Method / contribution | Difference from this integration |
|---|---|---|
| [LinRTX-OBS — NoID1290](https://github.com/NoID1290/LinRTX-OBS) | Native Linux OBS filters using real NVIDIA TrueHDR; source uses CUDA–OpenGL interop | Processes OBS sources rather than the selected existing KWin window |
| [KWin Effect Shaders — kevinlekiller](https://github.com/kevinlekiller/kwin-effect-shaders) | Per-window GLSL effects, hotkeys, sharpening/debanding and HDR-like appearance | Shader filtering rather than NVIDIA TrueHDR inference |
| [KWin Effect Upscale — JensKSP](https://github.com/JensKSP/kwin-effect-upscale) | KWin-native upscaling and sharpening | Shares the compositor boundary, with a different processing goal |
| [Gamescope — Valve and contributors](https://github.com/ValveSoftware/gamescope) | Gaming compositor with GPU scaling and color/HDR processing infrastructure | A separate compositor route rather than this KWin effect |
| [CachyOS-Auto-HDR — Aurion8](https://github.com/Aurion8/CachyOS-Auto-HDR) | Gamescope-based SDR-to-HDR conversion using shader processing and display detection | Does not use NVIDIA’s TrueHDR inference engine |
| [kms-hdr — Sigmachan](https://github.com/Sigmachan/kms-hdr) | KMS color-pipeline tooling | Different integration layer; roadmap items are not treated as completed features |
| [nvtruehdr — TwoToneEddy](https://github.com/TwoToneEddy/nvtruehdr) | Vulkan-layer HDR-like processing using its own shader | Different interception boundary and explicitly not NVIDIA’s model |
| [vkBasalt — DadSchoorse and contributors](https://github.com/DadSchoorse/vkBasalt) | Linux Vulkan post-processing | API-layer effects rather than native KWin TrueHDR processing |

[DXVK](https://github.com/doitsujin/dxvk), [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton), [Wine](https://www.winehq.org/) and [Proton](https://github.com/ValveSoftware/Proton) also deserve recognition for the Linux gaming routes many users rely on. They are not required for native applications and are not installed into games by Chaos HDR.

## Scope and corrections

This comparison records a bounded primary-source review, not an exhaustive history or an independent benchmark of these projects. Source visibility, APIs and implementations can change. Chaos HDR’s contribution is its particular direct-buffer KWin integration and controls workflow. Another approach may suit a user’s needs better.

If a description, attribution or provenance detail is incomplete, please propose a correction with the upstream reference. Community credit should remain specific and verifiable. Credit is not a substitute for honoring licenses.
