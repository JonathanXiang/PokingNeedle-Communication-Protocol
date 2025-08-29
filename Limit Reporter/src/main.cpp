#include <Arduino.h>

const char* DEVICE_ID = "D01";

const uint8_t PINS[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

const uint8_t PINMODES[] = {
  INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN
};

const bool ACTIVE_LOW[] = {false, false, false, false, false, false, false, false, false, false, false, false};

const unsigned long DEBOUNCE_MS  = 25;
const unsigned long HEARTBEAT_MS = 1000;

#define ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))
const uint8_t NUM_SLOTS = ARRAY_LEN(PINS);
static_assert(ARRAY_LEN(PINMODES)   == ARRAY_LEN(PINS), "PINMODES size must match PINS");
static_assert(ARRAY_LEN(ACTIVE_LOW) == ARRAY_LEN(PINS), "ACTIVE_LOW size must match PINS");

bool lastRaw[ARRAY_LEN(PINS)];
bool stable[ARRAY_LEN(PINS)];
unsigned long lastChangeMs[ARRAY_LEN(PINS)];
unsigned long lastHeartbeatMs = 0;

static void sendLine(const String& body) {
  uint8_t chk = 0; for (size_t i = 0; i < body.length(); ++i) chk ^= (uint8_t)body[i];
  char hex[3]; snprintf(hex, sizeof(hex), "%02X", chk);
  Serial.print(body); Serial.print('*'); Serial.print(hex); Serial.print("\r\n");
}

static String makeArray() {
  String p = "[";
  for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
    p += (stable[i] ? "1" : "0");
    if (i + 1 < NUM_SLOTS) p += ",";
  }
  p += "]";
  return p;
}

static void sendEvent()     { sendLine(String(DEVICE_ID) + ",E," + String(millis()) + "," + makeArray()); }
static void sendStatus()    { sendLine(String(DEVICE_ID) + ",S," + String(millis()) + "," + makeArray()); }
static void sendHeartbeat() { sendLine(String(DEVICE_ID) + ",H," + String(millis()) + "," + makeArray()); }

inline bool rawPressed(uint8_t pin, bool activeLow) {
  int v = digitalRead(pin);
  return activeLow ? (v == LOW) : (v == HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(20);

  for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
    pinMode(PINS[i], PINMODES[i]);
    lastRaw[i]      = rawPressed(PINS[i], ACTIVE_LOW[i]);
    stable[i]       = lastRaw[i];
    lastChangeMs[i] = 0;
  }

  sendStatus();
}

void loop() {
  unsigned long now = millis();

  bool changed = false;
  for (uint8_t i = 0; i < NUM_SLOTS; ++i) {
    bool raw = rawPressed(PINS[i], ACTIVE_LOW[i]);
    if (raw != lastRaw[i]) {
      lastRaw[i] = raw;
      lastChangeMs[i] = now;
    }
    if ((now - lastChangeMs[i]) >= DEBOUNCE_MS) {
      if (raw != stable[i]) {
        stable[i] = raw;
        changed = true;
      }
    }
  }

  if (changed) {
    bool any = false; for (uint8_t i=0;i<NUM_SLOTS;++i) any |= stable[i];
    digitalWrite(LED_BUILTIN, any ? HIGH : LOW);
    sendEvent();
  }

  if (now - lastHeartbeatMs >= HEARTBEAT_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }

  if (Serial.available()) {
    if ((char)Serial.read() == '?') sendStatus();
  }
}