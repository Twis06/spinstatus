# Hosyond ESP32 Washer Panel

PlatformIO/Arduino firmware for the Hosyond/LCDWIKI E32R40T-style 4.0 inch ESP32-32E display module. The UI uses LovyanGFX for the ST7796S display and XPT2046_Touchscreen for touch input.

## Hardware Target

- ESP32-WROOM-32E module
- 4.0 inch ST7796S TFT, 320x480 native resolution
- XPT2046 resistive touch
- Landscape app layout: 480x320

GPIO mapping follows LCDWIKI's E32R40T documentation:

| Function | GPIO |
| --- | ---: |
| TFT CS | 15 |
| TFT DC/RS | 2 |
| TFT SCK | 14 |
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT reset | EN |
| TFT backlight | 27 |
| Touch CS | 33 |
| Touch IRQ | 36 |

## Build And Upload

```sh
pio run
pio run -t upload
pio device monitor
```

This workspace did not have `pio` on PATH when the project was generated. Install PlatformIO Core or open the folder with the VS Code PlatformIO extension before building.

## Touch Calibration

On first boot after this LovyanGFX rewrite, the firmware enters a 5-point touch calibration screen and saves the result in ESP32 NVS flash.

To force recalibration later:

1. Hold the board's `BOOT` button.
2. Press and release `RESET`.
3. Release `BOOT` when the calibration screen appears.
4. Tap and hold each target.

The firmware prints raw calibration points to Serial only during calibration. Normal touch handling does not continuously log touch data, which keeps the UI more responsive.
