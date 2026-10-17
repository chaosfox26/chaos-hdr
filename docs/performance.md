# Performance and testing

[Back to Chaos HDR](../README.md)

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
