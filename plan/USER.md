# User-Local Tasks And Constraints

Minimize this file. Default to handling work autonomously. Escalate only when the user must run something themselves or supply a local-only fact.

## Local Environment

- Project root: `/home/seuoh/workspace/qt_ui/`.
- Editor integration: `.vscode/cmake-kits.json` and `.vscode/settings.json` are present; CMake kit selection is the user's responsibility.
- Conda environment (if used) and Qt6 install location are user-managed; the plan documents do not record specific paths until the user confirms them.

## User-Run Tasks

- Install/locate Qt6 with Core, Gui, Qml, Quick, QuickControls2, OpenGL components.
- Install FFmpeg development packages (`avformat`, `avcodec`, `avutil`, `swscale`, `avdevice`).
- Provide an OpenGL-capable display environment for runtime UI verification; headless CI cannot validate the QML viewport.
- Run `valgrind` (or equivalent) sessions for memory verification when Phase 8 needs it.
- Capture screenshots / observed behaviour for UI sign-off against `ui.md` and `ui_template/desktop.webp`.

## Constraints Worth Recording (fill in when known)

- [ ] Qt6 install path / CMake prefix — user to record once confirmed.
- [ ] FFmpeg version and library path — user to record once confirmed.
- [ ] Target OpenGL version supported by the dev machine (baseline is 3.3 Core).
- [ ] GPU/driver constraints affecting GLSL parity testing.

## Role Split

- User: server start/stop, personal paths, secrets, hardware connections, Qt/FFmpeg install decisions, runtime UI observation for sign-off.
- Claude: input preparation, task splitting, result review, failure analysis, plan/architecture updates.
- Automation scripts (under `scripts/`, to be added when needed): repeated build/test runs with logs and backups.
