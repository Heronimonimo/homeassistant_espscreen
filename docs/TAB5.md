# M5Stack Tab5, ST7121 variant

This experimental profile is only for the M5Stack Tab5 variant whose factory firmware reports:

```text
Detected ST7121 touch controller (FW version: 1), using ST7121 display
```

The reported unit has an ESP32-P4 eco2 at chip revision v1.3, 16 MB SPI flash and 32 MB PSRAM. The screen is 720 × 1280
pixels, used as a 1280 × 720 landscape layout. The profile selects ESPHome's `M5STACK-TAB5-ST7121` MIPI-DSI model.
Other Tab5 display and touch variants are not covered. A profile for the wrong display variant can leave the screen
blank, so check the factory log before choosing it.

## Install and test

In **New screen**, choose **M5Stack Tab5**, model **Tab5 ST7121**. The board is marked experimental because the project
has not yet tested this firmware on physical hardware. Its configuration is based on the factory log and the
[community ESPHome configuration](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL), not an acceptance test of this
project's firmware.

The display uses the explicit ST7121 model. ESPHome's available touch platform is named `st7123`; it is configured
here for the ST7121 controller using the community reference. The display and touch behavior on this board still needs
to be checked on glass.

The landscape grid defaults to three rows of tiles. In **New screen**, choose four rows to fit more, smaller tiles on a
page. The portrait grid remains one column by five rows.

After flashing, confirm that the screen boots, has a stable picture with correct colors, responds at the four corners
and across the surface, changes pages, pairs with Home Assistant, and remains working after a restart and a cold start.
Check the USB log and report the firmware and board variant with the results.

## Hardware references

- [M5Stack Tab5 product page](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4)
- [ESPHome MIPI-DSI display](https://esphome.io/components/display/mipi_dsi/)
- [ESPHome ST7123 touchscreen](https://esphome.io/components/touchscreen/st7123/)
- [Community ESPHome configuration](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL)
