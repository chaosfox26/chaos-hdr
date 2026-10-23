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

# Performance and testing

## Measure the right interval

- GPU preparation time: sharpening and input conversion.
- TrueHDR execution time: the NVIDIA engine’s GPU work.
- Full effect cost: includes relevant waiting, interop and final drawing.
- Displayed frame interval: cadence of frames KWin actually presents.
- KWin receipt-to-presentation: excludes work before KWin receives a frame.
- Input-to-display latency: requires an appropriate end-to-end measurement.

These quantities are not interchangeable. A short engine time does not prove the complete effect stays below the project’s half-millisecond target. Frame generation also makes rendered and displayed frame counts differ.

## Practical comparisons

1. Keep resolution, upscaling, frame generation, caps, VRR and picture settings consistent.
2. Warm up the game and shader compilation before comparing.
3. Alternate HDR-on and HDR-off samples rather than always measuring one first.
4. Use repeated comparable sequences; ordinary gameplay variation remains a confounder.
5. Report sample duration, averages, tail frame times and errors, not a single FPS value.
6. Separate inference cost from queue/synchronization waits and presentation scheduling.

Do not publish a universal overhead claim from one machine or treat a compositor interval as HDR’s added penalty. The project’s private validation records are excluded from source installers. Public benchmark disclosure also needs review against applicable NVIDIA terms; see [notices](../THIRD_PARTY_NOTICES.rst).

## Regression checks

Development fixtures cover approval/removal/reopen, paused/static content, conditional watchdog, idle without heartbeat writes, controls, clipping under overlapping windows and presentation tracing. Production diagnostics are disabled. Detailed build/test procedures are in [contributing](contributing.md). Do not enable a development build on someone’s live desktop as ordinary setup.

Documentation and branding changes should not alter shader arithmetic, queue synchronization, GPU passes or buffering. Claims of improvement require fresh measurements.

## Audit candidate validation

The 0.4.0 candidate removes profile polling from each processed frame, debounces settings persistence outside paint callbacks, and limits the overlay texture and repaint region to the sidebar and active popups. The accepted local 1440p GPU-processing baseline is approximately 0.27 ms. That is a regression reference on the tested system, not an end-to-end latency promise. Recheck both steady state and transitions after changing synchronization or resource lifetimes.

The direct route checks its previous submission without a CPU wait before reusing its command resources. The producer fence is handled by the existing GPU semaphore wait, without a duplicate CPU readiness poll. A pending HDR job repeats the last valid HDR output rather than switching to SDR. That repeat can delay visible new content; unchanged GPU processing cost does not make it free. No extra conversion job, frame queue, image copy or GPU pass is introduced. Teardown retains resources while polling Vulkan completion and a retirement-only GL fence with zero timeout. An unresolved retirement disables processing for the compositor session and retains one context until exit. Software completion-failure tests are distinct from real device-loss or driver-hang tests; the latter require a disposable system with its own GPU.

For matched synthetic comparisons, run `desktop_probe.py --gpu-input --benchmark` in separate baseline and candidate development roots. Set `CHAOS_HDR_TEST_WIDTH` and `CHAOS_HDR_TEST_HEIGHT` to the same dimensions; `CHAOS_HDR_BENCH_FULL=1` includes both independent modes and bypass. `CHAOS_HDR_BENCH_TRANSITIONS=1` adds repeated sidebar motion, effect toggles and forced GPU reinitialization. Keep other GPU jobs out of the comparison and reverse run order between resolutions.

Processing trace rows contain completed conversions, so a repeated output or asynchronous initialization is not represented as a conversion cost. Presentation feedback attached to the current surface commit does not establish the age of a cached image. Track output reuse separately when examining gaps; neither measurement is physical input-to-display latency. Forced cold reinitialization is separate from toggling settings with already allocated resources. Development feedback includes counts of KWin's actual VSync, asynchronous and adaptive presentation modes. Normal runtime has these diagnostics disabled and leaves KWin's VRR policy unchanged.
