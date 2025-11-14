#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ======================
// ⚙️ WiFi Access Point
// ======================
const char* ssid = "TrafficLight_DBP_NTP";
const char* password = "12345678";
// 🛣️ Khai báo tên tuyến đường
String road0Name = "Điện Biên Phủ";
String road1Name = "Nguyễn Tri Phương";

WebServer server(80);

// ======================
// ⚙️ LED Giao thông / 74HC595
// ======================
#define DATA_PIN 23
#define LATCH_PIN 5
#define CLOCK_PIN 18
#define DATA_PIN2 4
#define LATCH_PIN2 17
#define CLOCK_PIN2 16

#define R1_PIN 12
#define G1_PIN 14
#define B1_PIN 27
#define R2_PIN 32
#define G2_PIN 33
#define B2_PIN 25

#define ROAD0_DISTANCE 10  // m
#define ROAD1_DISTANCE 20  // m
// ==== Bảng mã 7 đoạn ====
const byte digitCodeStd[10] = {
  B11000000, B11111001, B10100100, B10110000, B10011001,
  B10010010, B10000010, B11111000, B10000000, B10010000
};

bool INVERT = false;     // set true for common-anode RGB (HIGH = off)
bool IC2_FIRST = true;   // unused for now

// ==== Thời gian ====
int GREEN_TIME = 10;
int YELLOW_TIME = 3;
int RED_TIME = GREEN_TIME + YELLOW_TIME;

// ==== Ưu tiên động (được set từ app) ====
int priorityAddSeconds = 15;  // mặc định nếu app chưa gửi

// ==== Ưu tiên ====
bool priorityPending[2] = { false, false };
bool priorityActive[2] = { false, false };

// ==== Pha ====
enum Phase { GREEN, YELLOW, RED };
const char* phaseName[] = { "GREEN", "YELLOW", "RED" };

Phase phaseG0 = GREEN;
Phase phaseG1 = RED;
unsigned long phaseStartTimeG[2] = { 0, 0 };
int currentGroup = 0;  // 0 = Bắc–Nam đang chạy

bool updated = false;
int greenTime = GREEN_TIME;

// ======================
// ⚙️ Gửi dữ liệu 74HC595
// ======================
void sendBytesChain(byte b3, byte b2, byte b1, byte b0) {
  // Note: Your wiring may require a different byte order. Keep as original layout if it matches hardware.
  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(LATCH_PIN2, LOW);

  // first chain
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b1);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b0);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b3);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b2);

  // second chain (duplicate if you have second register set)
  shiftOut(DATA_PIN2, CLOCK_PIN2, MSBFIRST, b3);
  shiftOut(DATA_PIN2, CLOCK_PIN2, MSBFIRST, b2);
  shiftOut(DATA_PIN2, CLOCK_PIN2, MSBFIRST, b1);
  shiftOut(DATA_PIN2, CLOCK_PIN2, MSBFIRST, b0);

  digitalWrite(LATCH_PIN, HIGH);
  digitalWrite(LATCH_PIN2, HIGH);
}

void displayCountdown(int group0, int group1) {
  int g0_tens = constrain(group0 / 10, 0, 9);
  int g0_ones = constrain(group0 % 10, 0, 9);
  int g1_tens = constrain(group1 / 10, 0, 9);
  int g1_ones = constrain(group1 % 10, 0, 9);
  byte a = digitCodeStd[g0_tens];
  byte b = digitCodeStd[g0_ones];
  byte c = digitCodeStd[g1_tens];
  byte d = digitCodeStd[g1_ones];
  sendBytesChain(d, c, b, a);
}

// ==== Đèn RGB ====
// For YELLOW use R + G together. INVERT supports common-anode (HIGH=off) wiring.
void writeRGBPins(int rPin, int gPin, int bPin, bool r, bool g, bool b) {
  if (INVERT) {
    digitalWrite(rPin, r ? LOW : HIGH);
    digitalWrite(gPin, g ? LOW : HIGH);
    digitalWrite(bPin, b ? LOW : HIGH);
  } else {
    digitalWrite(rPin, r ? HIGH : LOW);
    digitalWrite(gPin, g ? HIGH : LOW);
    digitalWrite(bPin, b ? HIGH : LOW);
  }
}

void setRGBColor(int group, Phase phase) {
  bool r = false, g = false, b = false;
  switch (phase) {
    case GREEN:  g = true; break;               // green
    case YELLOW: r = true; g = true; break;     // yellow = red + green
    case RED:    r = true; break;               // red
  }
  if (group == 0) {
    writeRGBPins(R1_PIN, G1_PIN, B1_PIN, r, g, b);
  } else {
    writeRGBPins(R2_PIN, G2_PIN, B2_PIN, r, g, b);
  }
}
int readSerialJson() {
  static String buffer = "";


  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      // đã nhận xong 1 JSON
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, buffer);

      if (!err) {
        JsonObject gt = doc["green_time"];
        if (!gt.isNull()) {
          int g0 = gt["G0"] | -1;
          int g1 = gt["G1"] | -1;
          if (g0 >= 5 && g1 >= 5) {
            greenTime = max(g0, g1);  // đặt theo giá trị lơn nhất an toàn;
          }
          Serial.printf("✅ Cập nhật GREEN_TIME=%d từ JSON\n", greenTime);
        }
      } else {
        Serial.println("❌ JSON lỗi từ Python");
      }

      buffer = "";  // reset
    } else {
      buffer += c;
    }
  }
  // trả về giá trị đã cập nhật cả GREEN_TIME và RED_TIME
  return greenTime;
}

// ==== Ưu tiên ====
// requestPriority: set pending priority for next cycle; returns immediately
void requestPriority(int group, int addSecondsParam) {
  if (group < 0 || group > 1) return;
  priorityPending[group] = true;
  priorityAddSeconds = addSecondsParam; // update global default / used when active
  Serial.printf("⏳ Ưu tiên G%d sẽ áp dụng ở chu kỳ đỏ KẾ TIẾP (+%d s)\n", group, addSecondsParam);
}

// ==== Pha ====
void nextPhase() {
  unsigned long now = millis();
  unsigned long e0 = (now - phaseStartTimeG[0]) / 1000;
  unsigned long e1 = (now - phaseStartTimeG[1]) / 1000;
  int greenTime = readSerialJson();
  Serial.println(greenTime);
  if (currentGroup == 0) {
    int gDur = GREEN_TIME + (priorityActive[0] ? priorityAddSeconds : 0);
    if (phaseG0 == GREEN && e0 >= (unsigned)gDur) {
      phaseG0 = YELLOW;
      phaseStartTimeG[0] = now;
    } else if (phaseG0 == YELLOW && e0 >= (unsigned)YELLOW_TIME) {
      phaseG0 = RED;
      phaseG1 = GREEN;
      phaseStartTimeG[0] = phaseStartTimeG[1] = now;
      currentGroup = 1;
      priorityActive[0] = false;
      // activate pending priority for group1 if exists
      if (priorityPending[1]) {
        priorityActive[1] = true;
        priorityPending[1] = false;
      }
      updated = true;
    }
  } else {
    int gDur = GREEN_TIME + (priorityActive[1] ? priorityAddSeconds : 0);
    if (phaseG1 == GREEN && e1 >= (unsigned)gDur) {
      phaseG1 = YELLOW;
      phaseStartTimeG[1] = now;
    } else if (phaseG1 == YELLOW && e1 >= (unsigned)YELLOW_TIME) {
      phaseG1 = RED;
      phaseG0 = GREEN;
      phaseStartTimeG[0] = phaseStartTimeG[1] = now;
      currentGroup = 0;
      priorityActive[1] = false;
      if (priorityPending[0]) {
        priorityActive[0] = true;
        priorityPending[0] = false;
      }
      updated = true;
    }
  }
  if (updated) {
    // apply updated GREEN_TIME if changed
      GREEN_TIME = greenTime;
      RED_TIME = GREEN_TIME + YELLOW_TIME;
      Serial.printf("🔄 Cập nhật thời gian: GREEN_TIME=%d, RED_TIME=%d\n", GREEN_TIME, RED_TIME);
      updated = false;
  }
}


// ======================
// 🌐 API HANDLERS
// ======================
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Traffic API running 🚦");
}

// ✅ GET /status — returns phase, priority, remaining
void handleStatus() {
  StaticJsonDocument<256> doc;

  unsigned long now = millis();
  unsigned long e0 = (now - phaseStartTimeG[0]) / 1000;
  unsigned long e1 = (now - phaseStartTimeG[1]) / 1000;

  int gDur0 = GREEN_TIME + (priorityActive[0] ? priorityAddSeconds : 0);
  int gDur1 = GREEN_TIME + (priorityActive[1] ? priorityAddSeconds : 0);
  int rDur0 = RED_TIME + (priorityActive[1] ? priorityAddSeconds : 0);
  int rDur1 = RED_TIME + (priorityActive[0] ? priorityAddSeconds : 0);

  int remaining0 = (phaseG0 == GREEN) ? max(0, gDur0 - (int)e0) :
                   (phaseG0 == YELLOW) ? max(0, YELLOW_TIME - (int)e0) :
                   max(0, rDur0 - (int)e0);

  int remaining1 = (phaseG1 == GREEN) ? max(0, gDur1 - (int)e1) :
                   (phaseG1 == YELLOW) ? max(0, YELLOW_TIME - (int)e1) :
                   max(0, rDur1 - (int)e1);

  JsonObject G0 = doc.createNestedObject("G0");
  G0["name"] = road0Name;
  G0["phase"] = phaseName[phaseG0];
  G0["remaining"] = remaining0;
  G0["priorityActive"] = priorityActive[0];
  G0["priorityPending"] = priorityPending[0];

  JsonObject G1 = doc.createNestedObject("G1");
  G1["name"] = road1Name;
  G1["phase"] = phaseName[phaseG1];
  G1["remaining"] = remaining1;
  G1["priorityActive"] = priorityActive[1];
  G1["priorityPending"] = priorityPending[1];

  String json;
  serializeJsonPretty(doc, json);
  server.send(200, "application/json", json);
}


// ✅ POST /set_road { "roadId": 1,2 or 3, "speed":5 }
void handleSetRoad() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Use POST\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int roadId = doc["roadId"] | -1;
  float speed = doc["speed"] | 5.0f;  // m/s
  int addSeconds = 0;
  int waitSeconds = 0;

  // compute approach time (rounded)
  if (roadId == 1) {
    addSeconds = (int)round((float)ROAD0_DISTANCE / max(0.1f, speed));
  } else if (roadId == 2) {
    addSeconds = (int)round((float)ROAD1_DISTANCE / max(0.1f, speed));
  }

  StaticJsonDocument<256> res;

  if (roadId == 3) {
    // roadId=3: "priority for direction currently GREEN's opposite"
    int activeGroup = (phaseG0 == GREEN) ? 0 : (phaseG1 == GREEN) ? 1 : -1;
    if (activeGroup != -1) {
      int targetGroup = (activeGroup == 0) ? 1 : 0; // opposite group
      if (!priorityPending[targetGroup] && !priorityActive[targetGroup]) {
        Serial.printf("⚡ roadId=3 -> set priority for G%d (opposite of current green)\n", targetGroup);
        res["status"] = "priority set";
        res["group"] = targetGroup;
        requestPriority(targetGroup, max(1, addSeconds));
        // compute waitSeconds: time until that group's next red begins (approx)
        unsigned long now = millis();
        unsigned long eActive = (activeGroup == 0) ? (now - phaseStartTimeG[0]) / 1000 : (now - phaseStartTimeG[1]) / 1000;
        int activeGreenDur = GREEN_TIME + (priorityActive[activeGroup] ? priorityAddSeconds : 0);
        int remainingActive = (phaseG0 == GREEN && activeGroup == 0) || (phaseG1 == GREEN && activeGroup == 1) ?
                             max(0, activeGreenDur - (int)eActive) :
                             ( ( (phaseG0 == YELLOW && activeGroup==0) || (phaseG1==YELLOW && activeGroup==1) ) ? max(0, YELLOW_TIME - (int)eActive) : 0 );
        // wait until active group finishes (remainingActive) + its yellow (if still green)
        if ( (activeGroup==0 && phaseG0==GREEN) || (activeGroup==1 && phaseG1==GREEN) ) {
          waitSeconds = remainingActive + YELLOW_TIME;
        } else {
          waitSeconds = remainingActive;
        }
        res["wait_seconds"] = waitSeconds;
        res["bonus_seconds"] = addSeconds + RED_TIME;
      } else {
        res["status"] = "already active/pending";
        res["group"] = targetGroup;
      }
    } else {
      res["status"] = "no active green group";
    }
  } else if (roadId == 1 || roadId == 2) {
    int group = roadId - 1;
    Serial.printf("⚡ Received roadId=%d -> request priority for G%d\n", roadId, group);
    res["status"] = "ok";
    res["group"] = group;

    // compute remaining seconds for the opposite group (time until they get red)
    unsigned long now = millis();
    unsigned long eOpp = (group == 0) ? (now - phaseStartTimeG[1]) / 1000 : (now - phaseStartTimeG[0]) / 1000;
    Phase oppPhase = (group == 0) ? phaseG1 : phaseG0;
    int oppRemaining = 0;
    if (oppPhase == GREEN) oppRemaining = max(0, GREEN_TIME - (int)eOpp);
    else if (oppPhase == YELLOW) oppRemaining = max(0, YELLOW_TIME - (int)eOpp);
    else if (oppPhase == RED) oppRemaining = max(0, RED_TIME - (int)eOpp);

    // waitSeconds: if opp is green -> remaining + yellow; if opp is yellow -> remaining; if opp is red -> remaining + GREEN + YELLOW
    if (oppPhase == GREEN) waitSeconds = oppRemaining + YELLOW_TIME;
    else if (oppPhase == YELLOW) waitSeconds = oppRemaining;
    else waitSeconds = oppRemaining + GREEN_TIME + YELLOW_TIME;

    res["wait_seconds"] = waitSeconds;
    res["bonus_seconds"] = addSeconds + RED_TIME;

    requestPriority(group, max(1, addSeconds));
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid roadId\"}");
    return;
  }

  String json;
  serializeJsonPretty(res, json);
  Serial.println(json);
  server.send(200, "application/json", json);
}

// ======================
// 🚀 setup()
// ======================
void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN2, OUTPUT);
  pinMode(LATCH_PIN2, OUTPUT);
  pinMode(CLOCK_PIN2, OUTPUT);
  pinMode(R1_PIN, OUTPUT);
  pinMode(G1_PIN, OUTPUT);
  pinMode(B1_PIN, OUTPUT);
  pinMode(R2_PIN, OUTPUT);
  pinMode(G2_PIN, OUTPUT);
  pinMode(B2_PIN, OUTPUT);

  unsigned long now = millis();
  phaseStartTimeG[0] = phaseStartTimeG[1] = now;

  // API routes
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/set_road", handleSetRoad);
  server.begin();
  Serial.println("✅ Server started");
}

// ======================
// 🔁 loop()
// ======================
void loop() {
  server.handleClient();
  nextPhase();

  // Hiển thị
  setRGBColor(0, phaseG0);
  setRGBColor(1, phaseG1);

  unsigned long now = millis();
  unsigned long e0 = (now - phaseStartTimeG[0]) / 1000;
  unsigned long e1 = (now - phaseStartTimeG[1]) / 1000;
  int gDur0 = GREEN_TIME + (priorityActive[0] ? priorityAddSeconds : 0);
  int gDur1 = GREEN_TIME + (priorityActive[1] ? priorityAddSeconds : 0);
  int rDur0 = RED_TIME + (priorityActive[1] ? priorityAddSeconds : 0);
  int rDur1 = RED_TIME + (priorityActive[0] ? priorityAddSeconds : 0);

  int t0 = (phaseG0 == GREEN) ? max(0, gDur0 - (int)e0) : (phaseG0 == YELLOW) ? max(0, YELLOW_TIME - (int)e0)
                                                                              : max(0, rDur0 - (int)e0);
  int t1 = (phaseG1 == GREEN) ? max(0, gDur1 - (int)e1) : (phaseG1 == YELLOW) ? max(0, YELLOW_TIME - (int)e1)
                                                                              : max(0, rDur1 - (int)e1);

  displayCountdown(t0, t1);
  delay(100);
}
