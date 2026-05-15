# ZMK Config — Lily58 Pro

## Hardware

- **Keyboard**: Lily58 Pro (split, 58-key)
- **MCU**: Mikoto v7.2 (nice!nano-based, nRF52840)
- **Displays**: Dual SSD1306 OLED, 128×32px each, mounted **vertically** (portrait orientation — effectively 32px wide × 128px tall per half)
- **Connection**: Wired split (no BLE between halves in use; battery/BT widgets intentionally disabled)

## Repository Structure

```
zmk-config/
├── build.yaml                          # Build matrix (board/shield combos)
├── config/
│   ├── lily58.keymap                   # Keymap (3 layers)
│   ├── lily58.conf                     # Firmware feature flags
│   └── west.yml                        # ZMK dependency manifest (ZMK v0.3)
├── boards/shields/nice_oled/           # Local clone of zmk-nice-oled (see below)
├── zephyr/module.yml                   # Zephyr module declaration
└── .github/workflows/build.yml         # GitHub Actions CI (ZMK v0.3 workflow)
```

## Build Targets

Defined in `build.yaml`:

| Side  | Board       | Shield                      | Notes                     |
|-------|-------------|-----------------------------|---------------------------|
| Left  | mikoto@7.2  | lily58_left nice_oled       | + snippet: studio-rpc-usb-uart |
| Right | mikoto@7.2  | lily58_right nice_oled      |                           |
| —     | mikoto@7.2  | settings_reset              | Recovery firmware         |

CI builds via GitHub Actions on push/PR. Artifacts are the `.uf2` flash files.

## Keymap (`config/lily58.keymap`)

Three layers, activated via `&mo`:

- **Layer 0 (default)**: QWERTY. Thumb cluster: Alt, GUI, LOWER, Space | Space, RAISE, PrintScreen, Grave. Combo: keys 53+54 → Enter.
- **Layer 1 (lower)**: F1–F12, arrow keys, brackets. Left encoder: Vol up/down.
- **Layer 2 (raise)**: Media controls (prev/next/play, volume), mouse scroll (SCRL_LEFT/DOWN/UP/RIGHT). Left encoder: Vol up/down.

Notable: Layer 0 left modifier column is Left Shift (row 3), Left Ctrl (row 4) — non-standard placement.

## Display System

### Widget Module

`boards/shields/nice_oled/` is a **local clone** of [mctechnology17/zmk-nice-oled](https://github.com/mctechnology17/zmk-nice-oled), embedded directly rather than referenced as an external module. This allows in-tree modifications without forking the upstream module system.

It was cloned to:
1. Remove hard-coded BT/battery indicators (not needed for wired use)
2. Fine-tune animation positioning and peripheral animation selection

### Portrait Rendering Pipeline

The physical OLEDs are 128×32 but mounted rotated 90°. The rendering pipeline compensates:

1. LVGL draws into a square canvas buffer (`CANVAS_HEIGHT × CANVAS_HEIGHT`)
2. `rotate_canvas()` in `widgets/util.c` applies a 90° rotation (`900` centidegrees via `lv_canvas_transform`)
3. The rotated output maps correctly to the physical portrait display

The canvas logical dimensions are set via Kconfig:
- `CONFIG_NICE_OLED_CUSTOM_CANVAS_WIDTH` (default: 32)
- `CONFIG_NICE_OLED_CUSTOM_CANVAS_HEIGHT` (default: 128)

When positioning widgets, coordinates are in the **pre-rotation canvas space** — x increases right, y increases down, before the 90° rotation is applied. This means a widget's apparent on-screen position is rotated from where you set it.

### Active Widgets (set in `config/lily58.conf`)

**Left half (central):**
- `CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT=y` — WPM-reactive bongo cat animation
- `CONFIG_NICE_OLED_WIDGET_WPM_GRAPH=y` — WPM history graph
- Bongo cat X position: `CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT_CUSTOM_X=0`

**Right half (peripheral):**
- `CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_HEAD=y` — animated head graphic

### Disabled Widgets

These are explicitly disabled because the board is wired (no battery) and no BT profile switching is needed:

```
CONFIG_NICE_OLED_WIDGET_BATTERY=n
CONFIG_NICE_OLED_WIDGET_OUTPUT=n          # BT/USB indicator
CONFIG_NICE_OLED_WIDGET_LAYER=n
CONFIG_NICE_OLED_WIDGET_PROFILE=n
CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS=n
CONFIG_NICE_OLED_WIDGET_RAW_HID_DRIVER=n
```

### Widget Source Layout

```
boards/shields/nice_oled/
├── custom_status_screen.c      # Entry point: initialises zmk_widget_screen
├── widgets/
│   ├── screen.c/.h             # Central half screen — main draw loop
│   ├── screen_peripheral.c/.h  # Peripheral half screen
│   ├── bongo_cat.c/.h          # WPM bongo cat animation
│   ├── responsive_bongo_cat.c  # Alternative bongo variant
│   ├── wpm.c/.h                # WPM graph/speedometer
│   ├── animation.c/.h          # Animation framework (peripheral)
│   ├── util.c/.h               # rotate_canvas(), draw helpers, CANVAS_* macros
│   └── ...                     # battery, output, layer, profile, modifiers, luna, etc.
├── assets/                     # Compiled image/animation data (.c files)
├── src/fonts/                  # Compiled fonts (pixel_operator_mono 8/12/16/22pt + TTF sources)
├── src/raw_hid/                # Raw HID driver (disabled)
├── Kconfig.defconfig           # All widget feature flags and their defaults
├── CMakeLists.txt              # Conditional compilation per Kconfig
├── nice_oled.conf              # Shield conf: SSD1306, I2C, display enable
└── nice_oled.overlay           # Minimal (display node defined by ZMK shield infrastructure)
```

## ZMK Studio

Enabled on the left (central) half via the `studio-rpc-usb-uart` snippet. Locking is disabled.

```
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_STUDIO_LOCKING=n
```

## Key Modification Notes

- `e3bfd51` — Removed BT/battery widgets (wired board, not needed)
- `e89eb60` — Removed ZMK event subscription that was causing input lag
- `964cc73` — Fixed key input lag: removed WORK_QUEUE_SYSTEM, reduced display tick to 33ms
- `77671d6` — Adjusted bongo cat positioning

## Common Tasks

**Changing what displays on the OLEDs**: Edit `config/lily58.conf` — toggle `CONFIG_NICE_OLED_WIDGET_*` flags.

**Repositioning a widget**: Find the `lv_obj_align(... CUSTOM_X, CUSTOM_Y)` call in `widgets/screen.c` (central) or `widgets/screen_peripheral.c` (peripheral), or set the corresponding `CONFIG_NICE_OLED_WIDGET_*_CUSTOM_X/Y` Kconfig options.

**Adding a new widget**: Follow the pattern in `widgets/screen.c` — add a `#if IS_ENABLED(CONFIG_...)` block in `draw_canvas()`, define a listener with `ZMK_DISPLAY_WIDGET_LISTENER`, and initialise in `zmk_widget_screen_init()`.

**Building locally**: Requires a west workspace with ZMK v0.3. The GitHub Actions workflow handles CI builds automatically on push.

**Flashing**: Double-tap reset on the Mikoto to enter bootloader, drag the `.uf2` artifact to the mounted drive.
