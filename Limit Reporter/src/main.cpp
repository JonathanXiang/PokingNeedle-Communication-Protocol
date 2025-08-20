#include <Arduino.h>

const char* DEVICE_ID = "D01";

const uint8_t TOP_PIN    = 2;
const uint8_t BOTTOM_PIN = 3;

const bool TOP_ACTIVE_LOW    = true;
const bool BOTTOM_ACTIVE_LOW = true;

const unsigned long DEBOUNCE_MS  = 25;
const unsigned long HEARTBEAT_MS = 1000;

struct Sw {
  uint8_t pin;
  bool activeLow;
  bool lastRaw;       
  bool stable;        
  unsigned long lastChangeMs;
};
Sw swTop    = {TOP_PIN,    TOP_ACTIVE_LOW,    false, false, 0};
Sw swBottom = {BOTTOM_PIN, BOTTOM_ACTIVE_LOW, false, false, 0};

unsigned long lastHeartbeatMs = 0;

void sendLine(const String& body) {
  uint8_t chk = 0; for (size_t i = 0; i < body.length(); ++i) chk ^= (uint8_t)body[i];
  char hex[3]; snprintf(hex, sizeof(hex), "%02X", chk);
  Serial.print(body); Serial.print('*'); Serial.print(hex); Serial.print("\r\n");
}

String makeArrayPayload() {
  String p = "[";
  p += (swTop.stable ? "1" : "0");
  p += ",";
  p += (swBottom.stable ? "1" : "0");
  p += "]";
  return p;
}

void sendEvent() {
  // D01,E,<ts>,[t,b]
  String body = String(DEVICE_ID) + ",E," + String(millis()) + "," + makeArrayPayload();
  sendLine(body);
}

void sendStatus() {
  String body = String(DEVICE_ID) + ",S," + String(millis()) + "," + makeArrayPayload();
  sendLine(body);
}

void sendHeartbeat() {
  String body = String(DEVICE_ID) + ",H," + String(millis()) + "," + makeArrayPayload();
  sendLine(body);
}

inline bool rawPressed(uint8_t pin, bool activeLow) {
  int v = digitalRead(pin);
  return activeLow ? (v == LOW) : (v == HIGH);
}

void sampleAndDebounce(Sw& sw, unsigned long now, bool& anyChanged) {
  bool raw = rawPressed(sw.pin, sw.activeLow);
  if (raw != sw.lastRaw) {
    sw.lastRaw = raw;
    sw.lastChangeMs = now;
  }
  if ((now - sw.lastChangeMs) >= DEBOUNCE_MS) {
    if (raw != sw.stable) {
      sw.stable = raw;
      anyChanged = true;
    }
  }
}

void setup() {
  pinMode(TOP_PIN, INPUT_PULLDOWN);
  pinMode(BOTTOM_PIN, INPUT_PULLDOWN);
  Serial.begin(115200);
  delay(50);

  swTop.lastRaw    = rawPressed(swTop.pin, swTop.activeLow);
  swBottom.lastRaw = rawPressed(swBottom.pin, swBottom.activeLow);
  swTop.stable     = swTop.lastRaw;
  swBottom.stable  = swBottom.lastRaw;

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, (swTop.stable || swBottom.stable) ? HIGH : LOW);

  sendStatus();
}

void loop() {
  unsigned long now = millis();

  bool changed = false;
  sampleAndDebounce(swTop, now, changed);
  sampleAndDebounce(swBottom, now, changed);

  if (changed) {
    digitalWrite(LED_BUILTIN, (swTop.stable || swBottom.stable) ? HIGH : LOW);
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