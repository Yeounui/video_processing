---
name: phase7-stream-review
description: Data race on currentTimeSecs_ found in Phase 7 realtime stream producer thread; all threading/teardown paths otherwise verified safe
metadata:
  type: project
---

Phase 7 stream input: `currentTimeSecs_` is written by the producer thread in `toImageBuffer()` (VideoInputService.cpp:486) without a mutex and read by the GUI thread at runtime (e.g., `currentTimeSecs()` and `positionChanged` emission). This is a confirmed data race.

**Why:** The field was originally GUI-only for file playback; the producer thread reuses `toImageBuffer()` without adding protection, causing UB on concurrent access.

**How to apply:** In future stream-related reviews, check whether fields shared between producer and GUI threads are mutex-protected or atomic. `latestFrame_` was correctly protected; `currentTimeSecs_` was not.
