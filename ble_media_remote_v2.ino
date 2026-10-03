/*
  BLE Media Remote v2 — XIAO nRF52840
  Two buttons send BLE HID media keys; RGB LED flashes on each press.

  Pin map (attempt 2.0):
    D0 -> Next button      (other leg -> GND)
    D6 -> Previous button  (other leg -> GND)
    D1 -> RGB LED leg via resistor  (LED_R)
    D3 -> RGB LED leg via resistor  (LED_G)
    D5 -> RGB LED leg via resistor  (LED_B)
    LED common cathode -> GND

  If the LED colors come out wrong (e.g. "next" flashes red instead of
  green), just swap the three PIN_LED_* assignments below.
*/

#include <bluefruit.h>

// ---------- Pins ----------
const uint8_t PIN_BTN_NEXT = D0;
const uint8_t PIN_BTN_PREV = D6;

const uint8_t PIN_LED_R = D1;
const uint8_t PIN_LED_G = D3;
const uint8_t PIN_LED_B = D5;

// ---------- Timing ----------
const uint16_t DEBOUNCE_MS = 30;
const uint16_t FLASH_MS = 120;

// ---------- BLE HID ----------
BLEDis bledis;
BLEHidAdafruit blehid;

// ---------- Button state ----------
struct Button {
  uint8_t pin;
  bool lastReading;
  bool stableState;
  unsigned long lastChangeTime;
};

Button btnNext = { PIN_BTN_NEXT, HIGH, HIGH, 0 };
Button btnPrev = { PIN_BTN_PREV, HIGH, HIGH, 0 };

// ---------- LED flash state ----------
unsigned long ledOffTime = 0;
bool ledActive = false;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setLED(false, false, false);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("Alejandros AUX Control");

  bledis.setManufacturer("DIY");
  bledis.setModel("BLE Media Remote v2");
  bledis.begin();

  blehid.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  handleButton(btnNext, sendNext, 0, 1, 0);  // green flash
  handleButton(btnPrev, sendPrevious, 1, 0, 0);  // red flash
  updateLED();
}

// ---------- Debounced button handling (active LOW) ----------
void handleButton(Button &btn, void (*sendFunc)(), bool r, bool g, bool b) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastReading) {
    btn.lastChangeTime = millis();
  }

  if ((millis() - btn.lastChangeTime) > DEBOUNCE_MS) {
    if (reading != btn.stableState) {
      btn.stableState = reading;
      if (btn.stableState == LOW) {
        sendFunc();
        flashLED(r, g, b);
      }
    }
  }

  btn.lastReading = reading;
}

// ---------- HID media keys ----------
void sendNext() {
  blehid.consumerKeyPress(HID_USAGE_CONSUMER_SCAN_NEXT);
  delay(10);
  blehid.consumerKeyRelease();
}

void sendPrevious() {
  blehid.consumerKeyPress(HID_USAGE_CONSUMER_SCAN_PREVIOUS);
  delay(10);
  blehid.consumerKeyRelease();
}

// ---------- LED (common cathode: HIGH = on) ----------
void setLED(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

void flashLED(bool r, bool g, bool b) {
  setLED(r, g, b);
  ledActive = true;
  ledOffTime = millis() + FLASH_MS;
}

void updateLED() {
  if (ledActive && millis() >= ledOffTime) {
    setLED(false, false, false);
    ledActive = false;
  }
}
