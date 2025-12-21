#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ======================
// ⚙️ WiFi Access Point
// ======================
const char* ssid = "ESP32_AP1";
const char* password = "12345678";

WebServer server(80);

// ======================
// ⚙️ LED Giao thông
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

// ==== Bảng mã 7 đoạn ====
const byte digitCodeStd[10] = {
  B11000000, B11111001, B10100100, B10110000, B10011001,
  B10010010, B10000010, B11111000, B10000000, B10010000
};

// ==== Thời gian ====
const int GREEN_TIME = 10;
const int YELLOW_TIME = 3;
const int RED_TIME = GREEN_TIME + YELLOW_TIME;
const int PRIORITY_ADD = 15;

// ==== Ưu tiên ====
bool priorityPending[2] = { false, false };
bool priorityNextCycle[2] = { false, false };
bool priorityActive[2] = { false, false };

// ==== Pha ====
enum Phase { GREEN,
             YELLOW,
             RED };
const char* phaseName[] = { "GREEN", "YELLOW", "RED" };

Phase phaseG0 = GREEN;
Phase phaseG1 = RED;
unsigned long phaseStartTimeG[2] = { 0, 0 };
int currentGroup = 0;  // 0 = Bắc–Nam đang chạy

// ======================
// ⚙️ Gửi dữ liệu 74HC595
// ======================
void sendBytesChain(byte b3, byte b2, byte b1, byte b0) {
  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(LATCH_PIN2, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b1);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b0);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b3);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, b2);

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
void setRGBColor(int group, Phase phase) {
  bool r = false, g = false, b = false;
  switch (phase) {
    case GREEN: b = true; break;
    case YELLOW: g = true; break;
    case RED: r = true; break;
  }
  if (group == 0) {
    digitalWrite(R1_PIN, r);
    digitalWrite(G1_PIN, g);
    digitalWrite(B1_PIN, b);
  } else {
    digitalWrite(R2_PIN, r);
    digitalWrite(G2_PIN, g);
    digitalWrite(B2_PIN, b);
  }
}

// ==== Ưu tiên ====
void requestPriority(int group) {
  if (group < 0 || group > 1) return;
  if ((group == 0 && phaseG0 == RED) || (group == 0 && phaseG0 == YELLOW) || (group == 0 && phaseG0 == GREEN)) {
    priorityPending[group] = true;
    Serial.printf("⏳ Ưu tiên G%d sẽ áp dụng ở chu kỳ đỏ KẾ TIẾP\n", group);
  } 
  if ((group == 1 && phaseG1 == RED) || (group == 1 && phaseG1 == YELLOW) || (group == 1 && phaseG1 == GREEN))
  {
    priorityPending[group] = true;
    Serial.printf("✅ Ưu tiên G%d đã được đặt (áp dụng khi sang chu kỳ đỏ kế tiếp)\n", group);
  }
}
// ==== Pha ====
void nextPhase() {
  unsigned long now = millis();
  unsigned long e0 = (now - phaseStartTimeG[0]) / 1000;  //số giây đã trôi từ lúc bắt đầu pha hiện tại
  unsigned long e1 = (now - phaseStartTimeG[1]) / 1000;

  if (currentGroup == 0) { //hướng 0 đang xanh
    int gDur = GREEN_TIME + (priorityActive[0] ? PRIORITY_ADD : 0); //có tăng thêm cho đèn xanh hay không
    if (phaseG0 == GREEN && e0 >= (unsigned)gDur) {
      phaseG0 = YELLOW;
      phaseStartTimeG[0] = now;
    } else if (phaseG0 == YELLOW && e0 >= (unsigned)YELLOW_TIME) {
      phaseG0 = RED;
      phaseG1 = GREEN;
      phaseStartTimeG[0] = phaseStartTimeG[1] = now;
      currentGroup = 1; //chuyển hướng xanh qua 1

      priorityActive[0] = false;

      if (priorityPending[1]) {
        priorityActive[1] = true;
        priorityPending[1] = false;
      } else if (priorityNextCycle[1]) {
        priorityPending[1] = true;
        priorityNextCycle[1] = false;
      }
    }
  } else {
    int gDur = GREEN_TIME + (priorityActive[1] ? PRIORITY_ADD : 0);
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
      } else if (priorityNextCycle[0]) {
        priorityPending[0] = true;
        priorityNextCycle[0] = false;
      }
    }
  }
}

// ======================
// 🌐 API HANDLERS
// ======================
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Traffic API running 🚦");
}

// ✅ GET /status
void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["G0"]["phase"] = phaseName[phaseG0];
  doc["G1"]["phase"] = phaseName[phaseG1];
  doc["G0"]["priorityActive"] = priorityActive[0];
  doc["G1"]["priorityActive"] = priorityActive[1];
  doc["currentGroup"] = currentGroup;

  String json;
  serializeJsonPretty(doc, json);
  server.send(200, "application/json", json);
}

// ✅ POST /set_road { "roadId": 1 hoặc 2 }
void handleSetRoad() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Use POST\"}");
    return;
  }

  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int roadId = doc["roadId"] | -1;
  if (roadId < 1 || roadId > 2) {
    server.send(400, "application/json", "{\"error\":\"Invalid roadId\"}");
    return;
  }

  int group = roadId - 1;
  requestPriority(group);

  StaticJsonDocument<128> res;
  res["status"] = "ok";
  res["group"] = group;
  String json;
  serializeJson(res, json);
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

  if (Serial.available()) {
    char c = Serial.read();
    if (c == '0') requestPriority(0);
    if (c == '1') requestPriority(1);
  }

  nextPhase();

  // Hiển thị
  setRGBColor(0, phaseG0);
  setRGBColor(1, phaseG1);

  unsigned long now = millis();
  unsigned long e0 = (now - phaseStartTimeG[0]) / 1000;
  unsigned long e1 = (now - phaseStartTimeG[1]) / 1000;
  int gDur0 = GREEN_TIME + (priorityActive[0] ? PRIORITY_ADD : 0);
  int gDur1 = GREEN_TIME + (priorityActive[1] ? PRIORITY_ADD : 0);
  int rDur0 = RED_TIME + (priorityActive[1] ? PRIORITY_ADD : 0);
  int rDur1 = RED_TIME + (priorityActive[0] ? PRIORITY_ADD : 0);

  int t0 = (phaseG0 == GREEN) ? max(0, gDur0 - (int)e0) : (phaseG0 == YELLOW) ? max(0, YELLOW_TIME - (int)e0)
                                                                              : max(0, rDur0 - (int)e0);
  int t1 = (phaseG1 == GREEN) ? max(0, gDur1 - (int)e1) : (phaseG1 == YELLOW) ? max(0, YELLOW_TIME - (int)e1)
                                                                              : max(0, rDur1 - (int)e1);

  displayCountdown(t0, t1);
  delay(100);
}