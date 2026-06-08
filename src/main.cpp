#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <string.h>

namespace {

constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 320;

constexpr int PIN_LCD_CS = 15;
constexpr int PIN_LCD_DC = 2;
constexpr int PIN_LCD_BL = 27;
constexpr int PIN_TOUCH_CS = 33;
constexpr int PIN_TOUCH_IRQ = 36;
constexpr int PIN_BOOT = 0;
constexpr int PIN_SPI_SCK = 14;
constexpr int PIN_SPI_MISO = 12;
constexpr int PIN_SPI_MOSI = 13;

constexpr uint32_t WASH_DURATION_MS = 120000UL;
constexpr uint32_t TOUCH_POLL_MS = 18;
constexpr uint32_t UI_TICK_MS = 100;
constexpr uint32_t REMIND_FLASH_MS = 1800;
constexpr uint32_t IDLE_GRACE_MS = 300000UL;
constexpr uint32_t TOUCH_CAL_MAGIC = 0x53503233UL;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

 public:
  LGFX() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = HSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 27000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = PIN_SPI_SCK;
      cfg.pin_mosi = PIN_SPI_MOSI;
      cfg.pin_miso = PIN_SPI_MISO;
      cfg.pin_dc = PIN_LCD_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = panel_.config();
      cfg.pin_cs = PIN_LCD_CS;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      panel_.config(cfg);
    }

    {
      auto cfg = light_.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }

    {
      auto cfg = touch_.config();
      auto buscfg = bus_.config();
      cfg.spi_host = buscfg.spi_host;
      cfg.pin_sclk = buscfg.pin_sclk;
      cfg.pin_mosi = buscfg.pin_mosi;
      cfg.pin_miso = buscfg.pin_miso;
      cfg.pin_cs = PIN_TOUCH_CS;
      cfg.pin_int = PIN_TOUCH_IRQ;
      cfg.freq = 2500000;
      cfg.x_min = 200;
      cfg.x_max = 3900;
      cfg.y_min = 200;
      cfg.y_max = 3900;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }

    setPanel(&panel_);
  }
};

LGFX lcd;
Preferences prefs;

enum class ScreenState {
  Welcome,
  Setup,
  Running,
  IdleLoaded,
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;

  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool pressed;
};

struct SavedTouchCalibration {
  uint32_t magic;
  uint16_t data[8];
};

SavedTouchCalibration calibration = {};
ScreenState state = ScreenState::Welcome;
uint8_t loadIndex = 1;
bool delicates = false;
bool wasTouching = false;
bool dirty = true;
bool touchCalibrated = false;
bool remindFlash = false;
bool idleOverdueShown = false;

uint32_t stateStartedAt = 0;
uint32_t lastTouchPoll = 0;
uint32_t lastUiTick = 0;
uint32_t lastSecondsShown = UINT32_MAX;
uint32_t lastProgressShown = UINT32_MAX;
uint32_t remindFlashUntil = 0;

const char *loadLabels[] = {"SMALL", "MED", "LARGE"};

constexpr Rect LOAD_BUTTONS[] = {
    {36, 118, 122, 46},
    {179, 118, 122, 46},
    {322, 118, 122, 46},
};
constexpr Rect DELICATES_BUTTON = {36, 228, 190, 46};
constexpr Rect START_BUTTON = {274, 214, 166, 58};
constexpr Rect FINISH_BUTTON = {326, 216, 110, 42};
constexpr Rect REMIND_BUTTON = {34, 252, 132, 42};
constexpr Rect RETRIEVED_BUTTON = {278, 252, 168, 42};
constexpr Rect TIMER_RECT = {24, 74, 432, 136};
constexpr Rect TIMER_VALUE_RECT = {36, 108, 408, 82};
constexpr Rect PROGRESS_RECT = {44, 236, 270, 12};

uint16_t color(uint8_t r, uint8_t g, uint8_t b) {
  return lcd.color565(r, g, b);
}

const uint16_t C_BG = color(249, 247, 253);
const uint16_t C_PANEL = color(255, 255, 255);
const uint16_t C_PANEL_2 = color(245, 242, 250);
const uint16_t C_LINE = color(229, 224, 239);
const uint16_t C_TEXT = color(45, 41, 54);
const uint16_t C_MUTED = color(126, 118, 143);
const uint16_t C_HEADER = color(184, 164, 232);
const uint16_t C_ACCENT = color(126, 96, 202);
const uint16_t C_ACCENT_SOFT = color(239, 234, 252);
const uint16_t C_READY = color(47, 163, 107);
const uint16_t C_READY_SOFT = color(232, 247, 239);
const uint16_t C_READY_PANEL = color(251, 255, 252);
const uint16_t C_RED = color(217, 63, 67);
const uint16_t C_RED_DARK = color(252, 232, 232);
const uint16_t C_RED_PANEL = color(255, 248, 248);

int16_t clampToScreen(long value, int16_t maxValue) {
  if (value < 0) {
    return 0;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return static_cast<int16_t>(value);
}

void text(const lgfx::IFont *font, uint16_t fg, uint16_t bg, textdatum_t datum) {
  lcd.setFont(font);
  lcd.setTextColor(fg, bg);
  lcd.setTextDatum(datum);
}

void text(const lgfx::GFXfont *font, uint16_t fg, uint16_t bg, textdatum_t datum) {
  lcd.setFont(font);
  lcd.setTextColor(fg, bg);
  lcd.setTextDatum(datum);
}

void centered(const char *value, int16_t x, int16_t y, const lgfx::IFont *font,
              uint16_t fg, uint16_t bg = C_BG) {
  text(font, fg, bg, middle_center);
  lcd.drawString(value, x, y);
}

void centered(const char *value, int16_t x, int16_t y, const lgfx::GFXfont *font,
              uint16_t fg, uint16_t bg = C_BG) {
  text(font, fg, bg, middle_center);
  lcd.drawString(value, x, y);
}

void label(const char *value, int16_t x, int16_t y, uint16_t bg = C_BG) {
  text(&fonts::Font2, C_MUTED, bg, top_left);
  lcd.drawString(value, x, y);
}

void valueText(const char *value, int16_t x, int16_t y, uint16_t bg = C_BG) {
  text(&fonts::FreeSansBold12pt7b, C_TEXT, bg, top_left);
  lcd.drawString(value, x, y);
}

void card(const Rect &r, uint16_t fill = C_PANEL) {
  lcd.fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
  lcd.drawRoundRect(r.x, r.y, r.w, r.h, 8, C_LINE);
}

void button(const Rect &r, const char *caption, bool active, uint16_t accent) {
  uint16_t fill = active ? accent : C_PANEL_2;
  uint16_t fg = active ? C_PANEL : C_TEXT;
  lcd.fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
  lcd.drawRoundRect(r.x, r.y, r.w, r.h, 8, active ? accent : C_LINE);
  text(&fonts::FreeSansBold12pt7b, fg, fill, middle_center);
  lcd.drawString(caption, r.x + r.w / 2, r.y + r.h / 2);
}

void outlineButton(const Rect &r, const char *caption, uint16_t accent) {
  lcd.fillRoundRect(r.x, r.y, r.w, r.h, 8, C_PANEL);
  lcd.drawRoundRect(r.x, r.y, r.w, r.h, 8, accent);
  text(&fonts::FreeSansBold12pt7b, accent, C_PANEL, middle_center);
  lcd.drawString(caption, r.x + r.w / 2, r.y + r.h / 2);
}

void header(uint16_t accent) {
  lcd.fillRect(0, 0, SCREEN_W, 66, C_HEADER);

  text(&fonts::FreeSansBold18pt7b, C_PANEL, C_HEADER, top_left);
  lcd.drawString("spinstatus", 22, 15);
  lcd.fillCircle(438, 33, 6, C_PANEL);
  lcd.fillCircle(438, 33, 3, accent);
}

TouchPoint readTouch() {
  uint16_t x = 0;
  uint16_t y = 0;
  if (!touchCalibrated || !lcd.getTouch(&x, &y)) {
    return {0, 0, false};
  }

  return {clampToScreen(x, SCREEN_W - 1), clampToScreen(y, SCREEN_H - 1), true};
}

bool calibrationLooksValid(const SavedTouchCalibration &cal) {
  if (cal.magic != TOUCH_CAL_MAGIC) {
    return false;
  }
  for (uint8_t i = 0; i < 8; ++i) {
    if (cal.data[i] != 0) {
      return true;
    }
  }
  return false;
}

bool loadTouchCalibration() {
  prefs.begin("touch", true);
  size_t read = prefs.getBytes("cal", &calibration, sizeof(calibration));
  prefs.end();
  touchCalibrated = read == sizeof(calibration) && calibrationLooksValid(calibration);
  if (touchCalibrated) {
    lcd.setTouchCalibrate(calibration.data);
  }
  return touchCalibrated;
}

void saveTouchCalibration(const uint16_t data[8]) {
  calibration.magic = TOUCH_CAL_MAGIC;
  memcpy(calibration.data, data, sizeof(calibration.data));
  prefs.begin("touch", false);
  prefs.putBytes("cal", &calibration, sizeof(calibration));
  prefs.end();
  touchCalibrated = true;
}

void runTouchCalibration() {
  lcd.fillScreen(C_BG);
  header(C_ACCENT);
  centered("TOUCH CALIBRATION", SCREEN_W / 2, 116, &fonts::FreeSansBold18pt7b, C_TEXT);
  centered("tap each target accurately", SCREEN_W / 2, 158, &fonts::Font2, C_MUTED);
  delay(700);

  uint16_t data[8] = {};
  lcd.calibrateTouch(data, C_ACCENT, C_BG, 14);
  lcd.setTouchCalibrate(data);
  saveTouchCalibration(data);
  lcd.fillScreen(C_BG);
  centered("CALIBRATION SAVED", SCREEN_W / 2, 150, &fonts::FreeSansBold18pt7b, C_ACCENT);
  delay(900);
}

void drawTimer(uint32_t seconds, const char *prefix, uint16_t fg, uint16_t bg) {
  char value[24];
  snprintf(value, sizeof(value), "%s%02lu:%02lu", prefix,
           static_cast<unsigned long>(seconds / 60),
           static_cast<unsigned long>(seconds % 60));

  lcd.fillRect(TIMER_VALUE_RECT.x, TIMER_VALUE_RECT.y, TIMER_VALUE_RECT.w,
               TIMER_VALUE_RECT.h, bg);
  text(&fonts::FreeSansBold24pt7b, fg, bg, middle_center);
  lcd.drawString(value, SCREEN_W / 2, TIMER_VALUE_RECT.y + TIMER_VALUE_RECT.h / 2 + 4);
}

void drawProgress(uint32_t remainingMs, uint16_t accent) {
  uint32_t elapsed = WASH_DURATION_MS - min(remainingMs, WASH_DURATION_MS);
  int16_t fillW = static_cast<int16_t>((static_cast<uint64_t>(elapsed) * PROGRESS_RECT.w) /
                                       WASH_DURATION_MS);

  lcd.fillRoundRect(PROGRESS_RECT.x, PROGRESS_RECT.y, PROGRESS_RECT.w,
                    PROGRESS_RECT.h, 6, C_PANEL_2);
  if (fillW > 0) {
    lcd.fillRoundRect(PROGRESS_RECT.x, PROGRESS_RECT.y, fillW,
                      PROGRESS_RECT.h, 6, accent);
  }
}

void drawWelcome() {
  lcd.fillScreen(C_BG);
  header(C_ACCENT);

  Rect welcomeCard = {30, 112, 420, 116};
  card(welcomeCard, C_PANEL);
  lcd.fillCircle(102, 170, 26, C_ACCENT_SOFT);
  lcd.drawRoundRect(90, 153, 24, 34, 3, C_ACCENT);
  lcd.drawFastHLine(95, 161, 14, C_ACCENT);
  lcd.drawFastHLine(95, 168, 14, C_ACCENT);

  text(&fonts::FreeSansBold18pt7b, C_TEXT, C_PANEL, top_left);
  lcd.drawString("TAP TO BEGIN", 154, 154);
}

void drawSetup() {
  lcd.fillScreen(C_BG);
  header(C_ACCENT);

  lcd.fillRoundRect(24, 78, 432, 106, 8, C_PANEL);
  lcd.drawRoundRect(24, 78, 432, 106, 8, C_LINE);
  label("LOAD SIZE", 40, 94, C_PANEL);
  for (uint8_t i = 0; i < 3; ++i) {
    button(LOAD_BUTTONS[i], loadLabels[i], loadIndex == i, C_ACCENT);
  }

  lcd.fillRoundRect(24, 198, 214, 88, 8, C_PANEL);
  lcd.drawRoundRect(24, 198, 214, 88, 8, C_LINE);
  label("FABRIC", 40, 207, C_PANEL);
  button(DELICATES_BUTTON, delicates ? "DELICATE" : "NORMAL", delicates, C_ACCENT);

  lcd.fillRoundRect(258, 198, 198, 88, 8, C_PANEL);
  lcd.drawRoundRect(258, 198, 198, 88, 8, C_LINE);
  button(START_BUTTON, "START", true, C_ACCENT);
}

void drawRunningFrame() {
  lcd.fillScreen(C_BG);
  header(C_ACCENT);

  lcd.fillRoundRect(TIMER_RECT.x, TIMER_RECT.y, TIMER_RECT.w, TIMER_RECT.h, 8, C_PANEL);
  lcd.drawRoundRect(TIMER_RECT.x, TIMER_RECT.y, TIMER_RECT.w, TIMER_RECT.h, 8, C_LINE);
  label("TIME REMAINING", 52, 92, C_PANEL);
  char meta[48];
  snprintf(meta, sizeof(meta), "%s / %s", loadLabels[loadIndex],
           delicates ? "DELICATE" : "NORMAL");
  text(&fonts::Font2, C_MUTED, C_PANEL, top_right);
  lcd.drawString(meta, 426, 92);
  drawTimer(WASH_DURATION_MS / 1000, "", C_TEXT, C_PANEL);

  label("CYCLE PROGRESS", 44, 218);
  drawProgress(WASH_DURATION_MS, C_ACCENT);
  outlineButton(FINISH_BUTTON, "FINISH", C_ACCENT);

  lastSecondsShown = UINT32_MAX;
  lastProgressShown = UINT32_MAX;
}

void drawIdleLoadedFrame() {
  bool overdue = millis() - stateStartedAt >= IDLE_GRACE_MS;
  idleOverdueShown = overdue;
  uint16_t stateColor = overdue ? C_RED : C_READY;
  uint16_t panelColor = overdue ? C_RED_PANEL : C_READY_PANEL;
  uint16_t noticeFill = overdue ? C_RED_DARK : C_READY_SOFT;

  lcd.fillScreen(C_BG);
  header(stateColor);

  lcd.fillRoundRect(TIMER_RECT.x, TIMER_RECT.y, TIMER_RECT.w, TIMER_RECT.h, 8, panelColor);
  lcd.drawRoundRect(TIMER_RECT.x, TIMER_RECT.y, TIMER_RECT.w, TIMER_RECT.h, 8, stateColor);
  label("LOADED IDLE TIME", 52, 92, panelColor);
  drawTimer(0, "+", stateColor, panelColor);

  lcd.fillRoundRect(36, 218, 408, 24, 6, noticeFill);
  centered(overdue ? "GRACE PERIOD OVER - FAIR TO REMOVE" : "READY FOR PICKUP",
           SCREEN_W / 2, 230, &fonts::Font2, stateColor, noticeFill);

  if (remindFlash) {
    button(REMIND_BUTTON, "SENT", true, C_ACCENT);
  } else {
    outlineButton(REMIND_BUTTON, "REMIND", C_ACCENT);
  }
  button(RETRIEVED_BUTTON, "COLLECTED", true, stateColor);

  lastSecondsShown = UINT32_MAX;
  lastProgressShown = UINT32_MAX;
}

void changeState(ScreenState next) {
  state = next;
  stateStartedAt = millis();
  lastSecondsShown = UINT32_MAX;
  lastProgressShown = UINT32_MAX;
  remindFlash = false;
  remindFlashUntil = 0;
  idleOverdueShown = false;
  dirty = true;
}

void redraw() {
  switch (state) {
    case ScreenState::Welcome:
      drawWelcome();
      break;
    case ScreenState::Setup:
      drawSetup();
      break;
    case ScreenState::Running:
      drawRunningFrame();
      break;
    case ScreenState::IdleLoaded:
      drawIdleLoadedFrame();
      break;
  }
  dirty = false;
}

void updateRunning(uint32_t now) {
  uint32_t elapsed = now - stateStartedAt;
  if (elapsed >= WASH_DURATION_MS) {
    changeState(ScreenState::IdleLoaded);
    return;
  }

  uint32_t remaining = WASH_DURATION_MS - elapsed;
  uint32_t seconds = (remaining + 999) / 1000;
  uint32_t progressStep = (elapsed * 100UL) / WASH_DURATION_MS;

  if (seconds != lastSecondsShown) {
    drawTimer(seconds, "", C_TEXT, C_PANEL);
    lastSecondsShown = seconds;
  }
  if (progressStep != lastProgressShown) {
    drawProgress(remaining, C_ACCENT);
    lastProgressShown = progressStep;
  }
}

void updateIdle(uint32_t now) {
  bool overdue = now - stateStartedAt >= IDLE_GRACE_MS;
  if (overdue != idleOverdueShown) {
    drawIdleLoadedFrame();
  }

  if (remindFlash && now > remindFlashUntil) {
    remindFlash = false;
    outlineButton(REMIND_BUTTON, "REMIND", C_ACCENT);
  }

  uint32_t seconds = (now - stateStartedAt) / 1000;
  if (seconds != lastSecondsShown) {
    drawTimer(seconds, "+", overdue ? C_RED : C_READY,
              overdue ? C_RED_PANEL : C_READY_PANEL);
    lastSecondsShown = seconds;
  }
}

void handleTouch(const TouchPoint &p) {
  switch (state) {
    case ScreenState::Welcome:
      changeState(ScreenState::Setup);
      return;

    case ScreenState::Setup:
      for (uint8_t i = 0; i < 3; ++i) {
        if (LOAD_BUTTONS[i].contains(p.x, p.y)) {
          loadIndex = i;
          dirty = true;
          return;
        }
      }
      if (DELICATES_BUTTON.contains(p.x, p.y)) {
        delicates = !delicates;
        dirty = true;
        return;
      }
      if (START_BUTTON.contains(p.x, p.y)) {
        changeState(ScreenState::Running);
        return;
      }
      return;

    case ScreenState::Running:
      if (FINISH_BUTTON.contains(p.x, p.y)) {
        Serial.println("Demo cycle finished manually");
        changeState(ScreenState::IdleLoaded);
      }
      return;

    case ScreenState::IdleLoaded:
      if (REMIND_BUTTON.contains(p.x, p.y)) {
        remindFlash = true;
        remindFlashUntil = millis() + REMIND_FLASH_MS;
        button(REMIND_BUTTON, "SENT", true, C_ACCENT);
        Serial.println("Reminder requested");
        return;
      }
      if (RETRIEVED_BUTTON.contains(p.x, p.y)) {
        changeState(ScreenState::Welcome);
        return;
      }
      return;
  }
}

void pollTouch(uint32_t now) {
  if (now - lastTouchPoll < TOUCH_POLL_MS) {
    return;
  }
  lastTouchPoll = now;

  TouchPoint p = readTouch();
  if (p.pressed && !wasTouching) {
    handleTouch(p);
  }
  wasTouching = p.pressed;
}

void tickUi(uint32_t now) {
  if (now - lastUiTick < UI_TICK_MS) {
    return;
  }
  lastUiTick = now;

  if (dirty) {
    redraw();
    return;
  }

  if (state == ScreenState::Running) {
    updateRunning(now);
  } else if (state == ScreenState::IdleLoaded) {
    updateIdle(now);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(PIN_BOOT, INPUT_PULLUP);

  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255);
  lcd.fillScreen(C_BG);

  bool forceCalibration = digitalRead(PIN_BOOT) == LOW;
  if (forceCalibration || !loadTouchCalibration()) {
    runTouchCalibration();
  }

  stateStartedAt = millis();
  redraw();
  Serial.println("spinstatus LovyanGFX panel ready");
}

void loop() {
  uint32_t now = millis();
  pollTouch(now);
  tickUi(now);
}
