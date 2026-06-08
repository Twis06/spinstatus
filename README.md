# spinstatus

Smart washer control-panel prototype for a Hosyond/LCDWIKI ESP32-32E 4.0 inch touch display.

## Firmware

`src/main.cpp` is the PlatformIO/Arduino firmware for the real ESP32 screen. It uses LovyanGFX for the ST7796S display and XPT2046 touch.

```sh
pio run
pio run -t upload
pio device monitor
```

Target hardware:

- ESP32-WROOM-32E
- 4.0 inch ST7796S TFT, 480 x 320 landscape UI
- XPT2046 resistive touch
- LCD SPI: SCK 14, MOSI 13, MISO 12, CS 15, DC 2
- Backlight 27, touch CS 33, touch IRQ 36

Touch calibration runs on first boot. To recalibrate, hold `BOOT`, tap `RESET`, then release `BOOT`.

## Preview

`preview/` is the browser demo for design review and video recording. Open `preview/index.html` locally or deploy that folder to Vercel.

Live preview: https://preview-mauve-gamma.vercel.app

Both versions show the same flow: tap entry, load setup, wash countdown, green pickup grace period, red overdue idle timer, remind, and collected reset.
