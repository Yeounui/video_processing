# Video Processing

A C99-based image and video processing program built with SDL2, OpenGL 3.3 Core Profile, FFmpeg, and stb.  
It can display static images, video files, webcams, and RTSP streams, apply 28 image-processing effects, and render the result through a hand-written GLSL resampling shader.

This project aims to implement the core image-processing algorithms directly in project-owned C and GLSL code.  
External libraries are used only for window creation, media decoding, image file I/O, terminal input, and OpenGL access. They are not used as replacements for the implemented effects or resampling filters.

![Video Processing](init_image.png)

## Features

- Static image input through `stb_image` and a directly implemented PPM P6 parser
- Video file input through FFmpeg (`libavformat`, `libavcodec`, `libavutil`, `libswscale`)
- Webcam and RTSP realtime stream input through FFmpeg device/network APIs and an SDL thread-backed ring buffer
- GLSL-first effect execution with CPU fallback
- Effect stack for video/realtime modes (`MAX_EFFECT_STACK = 3`)
- Direct CPU output editing and save support in static image mode
- Save formats: PPM, PNG, JPG, BMP, TGA
- SDL keyboard shortcuts and libedit-based terminal commands with history and Tab completion
- OpenGL hybrid resampling display path with directly implemented Sobel edge classification, Bilinear, Bicubic, Lanczos-3, and Box filtering

## Algorithms

The effect catalog contains 28 algorithms.

| 1-7 | 8-14 | 15-21 | 22-28 |
|---|---|---|---|
| 1. Brightness | 8. Rotation | 15. High-Pass Sharpening | 22. Difference of Gaussians |
| 2. Multiply | 9. Emboss | 16. Low-Pass-Removal Sharpening | 23. Histogram Stretching |
| 3. Gamma Correction | 10. Contrast Stretching | 17. Motion Blur Diagonal | 24. Endpoint Detection |
| 4. Threshold Fixed | 11. 3x3 Blur | 18. Motion Blur Horizontal | 25. Median Smoothing |
| 5. Threshold Average | 12. 5x5 Blur | 19. Horizontal Edge | 26. Grayscale Average |
| 6. Bitwise AND | 13. Gaussian Blur | 20. Vertical Edge | 27. Grayscale Luminosity |
| 7. Flip | 14. Sharpen | 21. Laplacian | 28. Grayscale Lightness |

## Requirements

- Build tools: C99 compiler, CMake 3.16 or newer, Ninja, `pkg-config`
- Libraries: SDL2, OpenGL, FFmpeg (`libavformat`, `libavcodec`, `libavutil`, `libswscale`, `libavdevice`), libedit, math library (`-lm`)
- Header-only dependency: download `stb_image.h` and `stb_image_write.h` into `third_party/stb/` before building

## License

The source code written for this repository is released under the MIT License. See the root `LICENSE` file for details.

External libraries follow their own project licenses, and this repository does not include their source code or binaries. stb headers are also not included in this repository; users download them from `https://github.com/nothings/stb` before building.

## External Libraries

This project uses the following external libraries and components.

| Component | Purpose | Source |
|---|---|---|
| SDL2 | Window creation, event handling, OpenGL context creation, threads, mutexes, conditions | https://github.com/libsdl-org/SDL |
| OpenGL | GPU textures, FBOs, shaders, final rendering | https://www.khronos.org/opengl/ |
| FFmpeg | Video file decoding, webcam/device capture, RTSP/network stream decoding, RGB conversion | https://ffmpeg.org/ |
| libedit | Terminal command input, history, Tab completion | https://thrysoee.dk/editline/ |
| stb_image.h | Static image decoding | https://github.com/nothings/stb/blob/master/stb_image.h |
| stb_image_write.h | PNG/JPG/BMP/TGA output | https://github.com/nothings/stb/blob/master/stb_image_write.h |

Download stb headers:

```bash
mkdir -p third_party/stb
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o third_party/stb/stb_image.h
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -o third_party/stb/stb_image_write.h
```

Because this repository does not include stb headers, the license terms for `stb_image.h` and `stb_image_write.h` are the terms written in the original headers downloaded by the user.

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

The main executable is created at:

```bash
./build/video_processing <image|video|/dev/videoN|rtsp-url>
```

Examples:

```bash
./build/video_processing samples/photo.png
./build/video_processing samples/movie.mp4
./build/video_processing /dev/video0
./build/video_processing rtsp://192.168.1.100:554/stream
```

## Controls

The program supports both SDL keyboard input and terminal commands.

General:

- `H`: print help for the current mode
- `Q` or `Esc`: quit
- `C`: configure default algorithm parameters
- `open <path>`: open another image, video, webcam device, or RTSP URL

Image mode:

- `save <png|jpg|bmp|tga> [path]`: save the current static image output
- `saveppm [path]` or `P`: save as PPM
- `Backspace` or `X`: reset to the original input
- Saving is available only in static image mode.

Video mode:

- `Space`: play/pause
- `0`-`9`: seek to 0%-90%
- `Home` / `End`: seek to start/end and pause
- `Left` / `Right`: frame step while paused
- `Up` / `Down`: adjust playback speed
- `Tab`: toggle loop
- `Backspace` or `X`: clear the effect stack

Realtime mode:

- `Space`: freeze/resume display
- `reconnect` or `P`: reconnect stream
- `buffer`: print buffer status
- `info`: print stream information
- `Backspace` or `X`: clear the effect stack

Common effect shortcuts:

- Hold `B`, `M`, `G`, `T`, `F`, `R`, or `K`, then press `+` or `-` to run the paired parameter action for that algorithm.
- Single-key effects include `A`, `E`, `U`, `D`, `Z`, `1`, `2`, `3`, `4`, `5`, and `6`.
- Terminal effect commands: `brighten`, `multiply`, `gamma`, `threshold`, `autothresh`, `bitand`, `flip`, `rotate`, `emboss`, `stretch`, `blur`, `blur5`, `gaussblur`, `sharpen`, `hipass`, `boost`, `diagblur`, `hblur`, `sobelh`, `sobelv`, `laplace`, `dog`, `histretch`, `endpoint`, `median`, `grayavg`, `graylum`, `graylight`.

Press `H` while running the program to see the exact command list for the current source mode.

## Tests

After building, run:

```bash
./build/test_cpu_reference
./build/test_image_saver
SDL_VIDEODRIVER=offscreen ./build/test_glsl_parity
```

The GLSL parity test requires an environment where SDL can create an OpenGL 3.3 context. On headless systems, driver support may still be required even with `SDL_VIDEODRIVER=offscreen`.

## Project Layout

```text
include/
src/
shaders/          GLSL shaders for effects and resampling
tests/            CPU, image saver, and GLSL parity tests
third_party/stb/  Local stb headers
```
