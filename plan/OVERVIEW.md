# OpenCV Migration Overview

## Goal

Replace the project's image and video processing internals with OpenCV-backed
implementations while preserving the Qt/QML application shell, controller API,
undo/redo model, source selection UI, transport controls, and viewport behavior.

The migration should reduce project-owned algorithm code, remove direct FFmpeg
and GLSL processing maintenance where possible, and establish one image
processing data model that works for still images, video files, and real-time
streams.

## Scope

In scope:

- Still image load/save through an OpenCV-backed I/O path.
- Video file and real-time stream frame acquisition through an OpenCV-backed
  video path if behavior matches application requirements.
- CPU algorithm processing through `cv::Mat` and OpenCV primitives where
  behavior can be matched or tolerated.
- A backend abstraction that can later choose CPU, OpenCL/UMat, CUDA, or another
  accelerated implementation.
- Preservation of `ImageBuffer` at controller, test, and viewport boundaries
  unless a later reviewed decision approves a broader API break.
- Preservation of current algorithm IDs, parameter keys, names, categories, and
  stack-size behavior.

Out of scope:

- Redesigning the QML UI.
- Replacing the Qt Quick viewport with an OpenCV window.
- Making CUDA mandatory.
- Expanding beyond the current 28 algorithm IDs.
- Rewriting undo/redo semantics.
- Exposing OpenCV types directly through QML-facing APIs.
- Changing visible algorithm behavior without an explicit reviewed decision.

## Primary Constraints

- Qt remains responsible for UI state, QML exposure, timers, signals, URLs,
  viewport rendering, and user interaction.
- OpenCV becomes the internal processing substrate for decoding, image I/O,
  transforms, filters, statistics, and optional acceleration.
- The application boundary color contract remains RGB/RGBA. OpenCV's BGR/BGRA
  defaults must not leak beyond local bridge code.
- Alpha behavior is a first-class compatibility requirement, especially after
  rotate and before later algorithms in a stack.
- The current maximum video effect stack size is 3 and should stay unchanged
  unless a separate product decision changes it.

## Migration Principles

- Define data, color, statistics, and alpha specs before replacing behavior.
- Build the CPU OpenCV path first and treat it as the reference path.
- Keep the existing public APIs stable until CPU parity is reviewed.
- Move image I/O and video decode behind the same `ImageBuffer` contract.
- Replace or wrap the existing GPU path only after CPU behavior is correct.
- Runtime-select acceleration based on OpenCV build/runtime capability.
- Remove direct FFmpeg and GLSL dependencies only after replacement paths meet
  the review criteria in [[REVIEW]].

## Missing Facts

- Target OpenCV version and modules available in the `videoprocess` Conda
  environment.
- Whether the packaged OpenCV `videoio` backend has FFmpeg support.
- Required codec/container and stream scheme matrix.
- Required latency budgets for local video, real-time streams, and stacked
  algorithms.
- Expected GPU hardware and whether CUDA support is required, optional, or not
  expected.
