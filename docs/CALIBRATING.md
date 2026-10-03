# Calibrating a new panel

This protocol is for **320×240, LVGL rotation 90°, swap_xy=false,
mirror_x=true, mirror_y=false**. Other orientations, GT911 touch, and other
controllers require a separate profile. Don't change rotation or mirroring
partway through the measurement.

The filter in this repo stabilizes the XPT2046 measurements. Calibration then
corrects offset, scale, and skew. Calibration does not fix a defective
panel, a loose connection, unstable power, or random outliers.

## Preparing

1. Use the screen's own ESPHome YAML, the one Tessera wrote when it was
   installed, and add your own `calibration.yaml` to its `packages:` (it starts as
   identity). Don't carry over another panel's correction.

```yaml
packages:
  display: ...                                   # what Tessera put there
  local_overrides: !include kitchen.local.yaml   # idem
  calibration: !include calibration.yaml
```
2. Check the data cable and the USB port with `python -m serial.tools.list_ports`.
3. Flash from the repo root:

```sh
python -m esphome -s CALIBRATION_ON_BOOT true run <your-screen>.yaml --device <USB_PORT>
```

4. Physically check for the **dark screen with five white + marks**.
   It stays on, and tile actions aren't accessible here.
5. Stop the ESPHome log reader with Ctrl+C. Leave the board and USB cable connected.
   Only one program may use the serial port at a time.

Wi-Fi/HA aren't needed for this USB route. If the board is already on Wi-Fi,
the screen can also be opened via the encrypted API:

```sh
python diagnostics/control_ui.py touch_diagnostics --host display-kitchen.local --name display-kitchen
```

That doesn't change the boot mode. For a new installation, use the
USB route above, so calibration mode also stays active after a reset.

## Measurement A: collecting data

```sh
python tools/calibrate.py capture --port <USB_PORT> --device-name display-kitchen --output measurements-before.json
```

The wizard guides you through **one target at a time**:

1. Read in the terminal which crosshair is up next.
2. Press Enter, then tap only that crosshair **three times**.
3. Hold each tap for about half a second and release fully in between.
4. Wait for the next terminal prompt. Don't move on to another corner yourself.

Order: top-left, top-right, bottom-right, bottom-left, center.
Use a suitably blunt stylus or a small, deliberate fingertip tap; no
sharp point. The targets are at `(20,20)`, `(299,20)`, `(299,219)`, `(20,219)`,
and `(160,120)`.

The wizard only reads touch logs with `calibration=1`. It doesn't flash anything, make
any network connection, or send any HA actions. An incomplete measurement is
not saved as a valid file. On a mistaken tap: Ctrl+C and start
over. Use a new filename if a measurement already exists.

## Calculating a correction

```sh
python tools/calibrate.py fit measurements-before.json --output calibration.yaml --replace
```

The script:

- takes the median of the three raw ADC measurements per corner;
- determines an affine correction from the four corners;
- checks the **center point, which is not used for the fit**;
- rejects errors that are too large (>12 px), spread (>18 px), and unusable measurement geometry;
- saves the existing `calibration.yaml` as a dated `.bak` before
  placing a new, complete version.

The output contains only the four base bounds and six affine coefficients in
an ESPHome substitutions map, which the screen's YAML imports as the
calibration package. Don't change the coefficients by feel, and don't add a second correction
in the driver. The `raw=` logs contain the filtered **physical ADC values**,
even once a correction is already set; recalibrating therefore doesn't stack twice.

A successful fit on existing data is not yet a successful physical test.

## Measurement B: independently testing the flashed result

Flash the correction, still with the measurement screen:

```sh
python -m esphome -s CALIBRATION_ON_BOOT true run <your-screen>.yaml --device <USB_PORT>
```

Close the log reader with Ctrl+C and collect **new taps**:

```sh
python tools/calibrate.py capture --port <USB_PORT> --device-name display-kitchen --output measurements-after.json
python tools/calibrate.py verify measurements-after.json
```

`verify` uses the firmware's real `native=` coordinates, rotates them
to screen coordinates, and compares them with the targets. It does **not
run a new fit**. All five points must pass. Don't accidentally
use the pre-flash file for this check.

Doesn't work? Check which point fails, check the order/pressure, measure again,
and read [TROUBLESHOOTING.md](TROUBLESHOOTING.md). Don't just raise the tolerance to get the
test to pass. Report any remaining hardware deviation honestly.

## Back to normal operation

```sh
python -m esphome run <your-screen>.yaml --device <USB_PORT>
```

No `-s CALIBRATION_ON_BOOT true` is given here; the profile keeps it `false`.
Then check the real thing: the tiles, the navigation and a few taps in the corners.

Most screens never need this. A CYD shows its calibration on the screen itself when
it first starts, and there are three ways back to it: **Settings -> This screen -> Calibrate touch**
on the screen itself, **Screen settings -> This screen -> Calibrate touch** in Tessera, and the
**Calibrate touch** button in Home Assistant (on the screen's ESPHome device). All three start the same
wizard. This USB route is for a panel that stays off after that, or for a measurement report.

If calibration mode was opened only via the API, you can close it:

```sh
python diagnostics/control_ui.py end_touch_diagnostics --host display-kitchen.local --name display-kitchen
```

Keep both measurement files and `calibration.yaml` with this physical screen.
Start over after replacing the panel; give only the neutral
starter files to a next owner.
