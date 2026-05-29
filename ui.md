# UI Design Specification

## Design Goal

The application should feel like a calm, elegant desktop image/video processing workspace. The UI must prioritize image inspection, fast algorithm selection, parameter adjustment, preview comparison, and video playback control.

The visual direction combines:

- Layout: source/navigation panel, central processing viewport, right algorithm inspector, bottom status/playback strip.
- Color mood: soft sky, muted blue, off-white, pale icy blue, and gentle beige tones from `ui_template/desktop.webp`.
- Element style: light dashboard/user-panel UI elements with white cards, squared dashboard tiles, compact icon rails, filled active buttons, soft gray inputs, subtle progress bars, and blue-accent interaction states.

QML is the primary UI technology. C++ provides backend state, command methods, image/video processing, and the custom OpenGL viewport.

## Visual Source Translation

### Color Mood

Use `ui_template/desktop.webp` as the color source.

Observed palette:

| Token | Hex | Use |
|-------|-----|-----|
| `sky.base` | `#8FA9C4` | main accent, calendar/header-like surfaces |
| `sky.deep` | `#6F86AB` | selected controls, active buttons, playback panels |
| `sky.light` | `#DDEEF4` | navigation highlight, subtle panel tint |
| `paper` | `#F8F8F5` | app background and primary surface |
| `paper.warm` | `#E9E3D4` | secondary quiet blocks, disabled tile backgrounds |
| `ink` | `#2F3438` | primary text |
| `ink.muted` | `#6E747A` | secondary text |
| `line` | `#E5E8EB` | dividers and input borders |
| `white` | `#FFFFFF` | card and field base |

Do not use saturated electric blue. The accent should look dusty, airy, and slightly desaturated.

### Element Style

Use `ui_template/light-collection-blue-elegant-ui-ux-elements-ux-dashboard-user-panel-template-user-interface_992252-1091.jpg` as the interaction style source, but recolor it with the `desktop.webp` palette.

Element traits:

- Light gray app canvas with white cards.
- Compact vertical icon rails and side navigation strips.
- Filled rectangular active states, recolored from bright blue to muted `sky.deep`.
- Minimal line icons inside square icon buttons.
- Input fields with soft gray fill, low-contrast placeholder text, and subtle borders.
- Compact dashboard modules: profile card, progress card, form card, search/list card, action card.
- Gentle shadows and thin separators rather than heavy outlines.
- Mostly rectangular components with modest radius; reserve full pills for segmented filters and status badges.
- Clear but quiet hierarchy with active blocks standing out through fill color rather than large typography.

## Application Layout

Use a QML `ApplicationWindow` with five primary regions.

```text
+-------------------------------------------------------------------+
| Top Bar: source actions, quick process, compare, save, status      |
+-------------+---------------------------------------+-------------+
| Source      |                                       | Algorithm   |
| Panel       |          ProcessingViewport            | Inspector   |
|             |        image/video render area         |             |
|             |                                       |             |
+-------------+---------------------------------------+-------------+
| Bottom Bar: playback, zoom, frame info, effect status, messages    |
+-------------------------------------------------------------------+
```

### Top Bar

Purpose:

- Provide global actions without crowding the viewport.

Contents:

- Open Image.
- Open Video.
- Open Stream.
- Save Image.
- Reset.
- Before/After compare toggle.
- Preview toggle.
- Current mode indicator.

Style:

- Height: 52 px.
- Background: `paper`.
- Bottom border: `line`.
- Buttons: icon-first, compact, rounded 6 px.
- Primary action: filled `sky.deep`, white text/icon.
- Secondary action: transparent or white with `line` border.
- Icon-only actions may use square white tiles with a thin `line` border.

### Source Panel

Purpose:

- Show current source, source metadata, history, and quick navigation.

Width:

- Default 260 px.
- Minimum 220 px.
- Collapsible.

Sections:

- Source card: filename, dimensions, mode, frame rate if applicable.
- History list: original, applied effects, reset point.
- Navigation/categories: Image, Video, Stream, Effects, Output.

Style:

- Background: `paper`.
- Section header strip: `sky.light`.
- Cards: `white`, radius 6-8 px, subtle shadow.
- Active row: filled `sky.deep` with white text/icon when it is a primary navigation selection.
- Secondary selected row: `sky.light` background with `sky.deep` icon/text.
- Icon rail buttons: square 42 px tiles, white background, `line` border, active tile filled `sky.deep`.

### ProcessingViewport

Purpose:

- Central QML-visible custom render item for OpenGL display.
- Displays CPU `outImage` or GPU processed texture.
- Owns pan/zoom/fit visual behavior.

Behavior:

- Default mode: fit image/video to available area while preserving aspect ratio.
- Background: near-white for empty state, black letterbox/pillarbox around rendered content.
- Supports zoom controls: fit, 100%, zoom in/out.
- Supports pan when zoomed.
- Supports compare modes:
  - original/result toggle.
  - split vertical before/after.
  - side-by-side optional.

Style:

- Viewport outer surface: `paper`.
- Actual media background: `#0F1114` or black.
- Empty state: simple centered icon and short label.
- No decorative gradients or unrelated illustration.

### Algorithm Inspector

Purpose:

- Replace deep menus with a fast, visible algorithm workflow.

Width:

- Default 340 px.
- Minimum 300 px.
- Collapsible.

Sections:

- Algorithm category tabs.
- Algorithm list.
- Parameter controls.
- Preview/Apply/Reset buttons.
- Stack/status summary.

Categories:

- Point.
- Geometry.
- Filter.
- Edge.
- Morphology.
- Grayscale.

Parameter controls:

- Sliders for continuous numeric values.
- Spin boxes for exact numeric values.
- Segmented controls for mode choices.
- Switches for preview and compare.
- Compact combo boxes for kernel size and speed options.

Style:

- Background: `paper`.
- Active tab: `sky.deep` fill with white text/icon.
- Inactive tab: `white` with `line` border.
- Parameter cards: `white`, radius 6-8 px.
- Search/filter fields: pale gray fill, no heavy outline, right-aligned search icon.
- Select controls: gray fill with compact chevron.
- Checkboxes: blue check fill using `sky.deep`.
- Slider track: `sky.light`.
- Slider handle: `sky.deep`.
- Progress bars: thin, track `line`, progress `sky.deep`, percent labels aligned right.
- Apply button: filled `sky.deep`, rectangular with 6 px radius.
- Destructive/reset action: white or transparent, not red unless truly destructive.

### Bottom Bar

Purpose:

- Playback control, processing status, and precise inspection details.

Height:

- Static image: 44 px.
- Video/stream: 64 px.

Contents:

- Play/pause.
- Step forward.
- Timeline scrubber.
- Speed selector.
- Loop toggle.
- Zoom percentage.
- Frame/time.
- Processing backend indicator: CPU, GLSL, CPU+GLSL.
- Last message.

Style:

- Background: `paper`.
- Top border: `line`.
- Playback controls use icon buttons.
- Timeline track: `sky.light`.
- Timeline progress: `sky.deep`.

## QML Component Model

Recommended QML components:

- `App.qml`: root `ApplicationWindow`.
- `TopBar.qml`: global command bar.
- `SourcePanel.qml`: source metadata and history.
- `ProcessingViewport.qml`: wrapper around the C++ render item.
- `AlgorithmInspector.qml`: category, algorithm list, parameter editor.
- `ParameterEditor.qml`: dynamic parameter controls.
- `BottomTransport.qml`: playback/status strip.
- `StatusBadge.qml`: reusable mode/backend/effect-status badge.
- `IconButton.qml`: consistent icon button.
- `PillButton.qml`: segmented filter/status action.
- `IconRail.qml`: vertical square icon navigation.
- `SearchField.qml`: gray-filled search/input field.
- `ProgressMeter.qml`: compact progress bar with percentage label.
- `PanelCard.qml`: reusable card surface.

C++ objects exposed to QML:

- `AppController`
  - source state.
  - current image/video metadata.
  - algorithm list model.
  - command methods: open, save, apply, preview, reset, playback.
- `AlgorithmModel`
  - category.
  - display name.
  - parameters.
  - backend support.
- `ProcessingViewportItem`
  - `QSGRenderNode`-backed custom render item.
  - receives current texture/image update notifications.

## Interaction Rules

### Static Image

- Opening an image immediately displays it in the viewport.
- Selecting an algorithm shows parameters in the inspector.
- Preview updates viewport without committing to `outImage`.
- Apply commits to `outImage`.
- Reset restores original `inImage`.
- Save writes current `outImage`.
- Static image applies are cumulative and have no fixed apply cap.

### Video / Stream

- Opening video/stream switches bottom bar to transport mode.
- Selecting an algorithm appends to effect stack after parameter confirmation.
- Preview shows candidate effect before append when possible.
- Save is disabled.
- Reset clears effect stack.
- Fourth stack effect is rejected.

### Compare

Support at least:

- Hold/toggle original.
- Split before/after.

Compare controls should be near the top bar and also available in the viewport overlay.

## Visual States

### Empty State

Viewport:

- Background: `paper`.
- Centered simple icon.
- Text: “Open an image or video”.
- Primary button: Open Image.

### Loading State

- Use a small progress ring or indeterminate bar.
- Do not block repaint.
- Show source path or operation in bottom status.

### Error State

- Non-fatal errors appear as bottom-bar messages and optional toast.
- Avoid modal dialogs except for destructive confirmations or unrecoverable failures.
- Preserve previous valid image/frame.

### Disabled State

- Disabled controls use `ink.muted` at low opacity.
- Disabled backgrounds use `paper.warm` or `line`.
- Save is disabled outside static image mode.

## Typography

Use a clean sans-serif font available through Qt.

Suggested hierarchy:

- Window/body: 13 px.
- Panel section title: 13 px, semibold.
- Card title: 14 px, semibold.
- Inspector algorithm name: 16 px, semibold.
- Numeric readouts: 12 px, tabular if available.

Do not use script/display fonts from `desktop.webp`; keep the color mood, not the decorative typography.

## Spacing And Shape

- App padding: 12 px.
- Panel inner spacing: 10 px.
- Card padding: 12 px.
- Control height: 32 px.
- Toolbar button size: 34 px.
- Icon size: 18 px.
- Card radius: 6-8 px.
- Button radius: 6 px.
- Input radius: 4-6 px.
- Pill radius: 999 px.

Use soft shadows only on cards and floating overlays:

- Low blur.
- Low opacity.
- No heavy black drop shadows.

## Color Usage Rules

- `paper` is the dominant app background.
- `white` is used for cards, fields, and control surfaces.
- `sky.light` is used for subtle selected rows and section bands.
- `sky.deep` is used for active states and primary actions.
- `paper.warm` is reserved for secondary tiles or disabled blocks.
- Keep media viewport letterbox black for accurate inspection.

Avoid:

- Purple/blue gradients.
- Overly saturated neon blue.
- Large beige-only regions.
- Decorative orbs or purely atmospheric backgrounds.

## Implementation Notes

- Use QML for layout, controls, animation, and responsive panels.
- Use C++ for processing state, file/video IO, algorithm execution, and OpenGL render item integration.
- Keep the processing core independent of QML.
- The custom viewport uses a `QSGRenderNode` render path and exposes minimal QML properties: source size, zoom, fit mode, compare mode, backend status.
- The renderer must not use QML `Image` as the main video path for processed frames.
- UI animations should be short and functional: panel collapse, hover, selection, preview state.

## Acceptance Criteria

- The first screen is the working processing interface, not a landing page.
- QML UI uses the `desktop.webp` color mood.
- Dashboard-style controls are light, rounded, and elegant.
- All 28 algorithms are discoverable from the inspector.
- Parameter controls are visible without console input.
- The central viewport remains the visual focus.
- Static image, video, and stream modes have distinct but consistent states.
- The design can be implemented without Qt Widgets.
