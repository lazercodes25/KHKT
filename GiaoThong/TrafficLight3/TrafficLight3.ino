#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ======================
// ⚙️ WiFi Access Point
// ======================
const char* ssid = "NgaTu_HN_NTP";
const char* password = "12345678";
// 🛣️ Khai báo tên tuyến đường
String road1Name = "Hàm Nghi";
String road0Name = "Nguyễn Tri Phương";

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

#define ROAD0_DISTANCE 20  // m
#define ROAD1_DISTANCE 10  // m
// ==== Bảng mã 7 đoạn ====
const byte digitCodeStd[10] = {
  B11000000, B11111001, B10100100, B10110000, B10011001,
  B10010010, B10000010, B11111000, B10000000, B10010000
};
int GREEN_MAX = 35;
int GREEN_MIN = 10;

bool INVERT = false;    // set true for common-anode RGB (HIGH = off)
bool IC2_FIRST = true;  // unused for now

// ==== Thời gian ====
int GREEN_TIME = 10;
int YELLOW_TIME = 3;
int RED_TIME = GREEN_TIME + YELLOW_TIME;

// ==== Ưu tiên động (được set từ app) ====
int priorityAddSeconds = 15;  // mặc định nếu app chưa gửi

// ==== Ưu tiên ====
bool priorityPending[2] = { false, false };
bool priorityActive[2] = { false, false };
bool priorityActive2[2] = { false, false };

// ==== Pha ====
enum Phase { GREEN,
             YELLOW,
             RED };
const char* phaseName[] = { "GREEN", "YELLOW", "RED" };

Phase phaseG0 = GREEN;
Phase phaseG1 = RED;
unsigned long phaseStartTimeG[2] = { 0, 0 };
int currentGroup = 0;  // 0 = Bắc–Nam đang chạy
float minSpeed = 1000;

bool updated = false;
int greenTime = GREEN_TIME;
int gtime0 = GREEN_TIME;
int gtime1 = GREEN_TIME;
int preAddSeconds = 15;

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
          // Nếu group hiện tại là pha RED, thì cập nhật GREEN_TIME theo giá trị của group đó
          if (g0 != 0 && g0 !=0)
          {
            if (phaseG0 == RED) {
              greenTime = g0; 
            } else {
              greenTime = g1;   
            }
            gtime0 = g0;
            gtime1 = g1; 
          }
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

  Serial.print("Receiver Request");
  priorityPending[group] = true;
  int afterAddSeconds;
  int checktime = 0;
  if (group == 0){
    checktime = gtime0 ;
  }
  else 
  {
    checktime = gtime1;
  }

  if ((checktime + YELLOW_TIME) < addSecondsParam)
  {
    preAddSeconds = addSecondsParam - checktime - YELLOW_TIME;  // update global default / used when active
  }
  else 
  {
    preAddSeconds = 0;
  }

  afterAddSeconds = checktime + preAddSeconds;
  if (afterAddSeconds > GREEN_MAX) {
    preAddSeconds = GREEN_MAX - checktime;
  }
  
}

// ==== Pha ====
void nextPhase() {
  // Serial.print("priorityAddSeconds: ");
  // Serial.println(priorityAddSeconds);
  unsigned long now = millis();
  unsigned long e0 = (now - phaseStartTimeG[0]) / 1000;
  unsigned long e1 = (now - phaseStartTimeG[1]) / 1000;
  
  int greenTime = readSerialJson();
  if (currentGroup == 0) {
    int gDur1 = GREEN_TIME + (priorityActive[0] ? priorityAddSeconds : 0);
    if (phaseG0 == GREEN && e0 >= (unsigned)gDur1) {
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
        priorityActive2[1] = true;
        priorityAddSeconds =  preAddSeconds;
      }

      updated = true;
    }
  }
  else {
    int gDur2 = GREEN_TIME + (priorityActive[1] ? priorityAddSeconds : 0);
    if (phaseG1 == GREEN && e1 >= (unsigned)gDur2) {
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
        priorityActive2[0] = true;
        priorityAddSeconds =  preAddSeconds;
      }

      updated = true;
    }
  }
  if (updated) {
    // apply updated GREEN_TIME if changed
    GREEN_TIME = greenTime;
    RED_TIME = GREEN_TIME + YELLOW_TIME;
    updated = false;
    minSpeed = 1000;  // reset maxSpeed after phase change
  }
}



// ======================
// 🌐 API HANDLERS
// ======================
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Traffic API running 🚦");
}
void handleUpdate() {
  StaticJsonDocument<128> doc;
      int activeGroup = (phaseG0 == GREEN || phaseG0 == YELLOW) ? 0 
                    : (phaseG1 == GREEN || phaseG1 == YELLOW) ? 1 
                    : -1;
  doc["green_time"] = GREEN_TIME + (priorityActive[activeGroup] ? priorityAddSeconds : 0); // chỉ trả về đúng GREEN_TIME

  String json;
  serializeJsonPretty(doc, json);
  server.send(200, "application/json", json);
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

  int remaining0 = (phaseG0 == GREEN) ? max(0, gDur0 - (int)e0) : (phaseG0 == YELLOW) ? max(0, YELLOW_TIME - (int)e0)
                                                                                      : max(0, rDur0 - (int)e0);

  int remaining1 = (phaseG1 == GREEN) ? max(0, gDur1 - (int)e1) : (phaseG1 == YELLOW) ? max(0, YELLOW_TIME - (int)e1)
                                                                                      : max(0, rDur1 - (int)e1);

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
  int gTi = 0;
  Serial.println(gTi);
  // compute approach time (rounded)
  if (roadId == 1) {
    addSeconds = (int)round((float)ROAD0_DISTANCE / speed);
    gTi = gtime0;
  } else if (roadId == 2) {
    addSeconds = (int)round((float)ROAD1_DISTANCE / speed);
    gTi = gtime1;
  }
  Serial.print("Add Second tinh ra: ");
  Serial.println(addSeconds);

  StaticJsonDocument<256> res;


  if (roadId == 3) {
    // roadId=3: "priority for direction currently GREEN or YELLOW's opposite"
    //Lệnh này trả về group nào đang chạy (nghĩa là group đó đang xanh hoặc vàng)
    int activeGroup = (phaseG0 == GREEN || phaseG0 == YELLOW) ? 0 
                    : (phaseG1 == GREEN || phaseG1 == YELLOW) ? 1 
                    : -1;

    int requestGroup = 0;
    if (activeGroup == 0) {
      requestGroup = 1;
      addSeconds = 14;//đường 20m
    } else {
      requestGroup = 0;
      addSeconds = 32;//đường 10m
    }
    //addSeconds = 30;
    if (activeGroup != -1) {
      if (!priorityActive[requestGroup]) {
        if (minSpeed > speed) {
          res["status"] = "priority set";
          res["group"] = activeGroup;
          minSpeed = (int)speed;
          requestPriority(requestGroup, addSeconds);
          // compute waitSeconds: time until that group's next red begins (approx)
          unsigned long now = millis();
          unsigned long eActive = (activeGroup == 0) ? (now - phaseStartTimeG[0]) / 1000 : (now - phaseStartTimeG[1]) / 1000;
          int gr_time = GREEN_TIME + (priorityActive[activeGroup] ? priorityAddSeconds : 0);
          int remainingActive = (phaseG0 == GREEN) || (phaseG1 == GREEN) ? max(0, gr_time - (int)eActive) : (((phaseG0 == YELLOW) || (phaseG1 == YELLOW)) ? max(0, YELLOW_TIME - (int)eActive) : 0);
          // wait until active group finishes (remainingActive) + its yellow (if still green)
          if ((phaseG0 == GREEN) || (phaseG1 == GREEN)) {
            waitSeconds = remainingActive + YELLOW_TIME;
          } else {
            waitSeconds = remainingActive;
          }
          res["wait_seconds"] = waitSeconds;
          res["bonus_seconds"] = addSeconds + RED_TIME;
        } else {
          res["status"] = "speed not higher than previous";
          res["group"] = activeGroup;
        }
      } else {
        res["status"] = "already active/pending";
        res["group"] = activeGroup;
      }
    } else {
      res["status"] = "no active green group";
    }
  }
  else if (roadId == 1 || roadId == 2) {
    int group = roadId - 1;
    if (priorityPending[group]) {
      // check speed if slower than previous
      if (minSpeed > speed) {

        res["status"] = "ok";
        res["group"] = group;
        minSpeed = speed;
        
        unsigned long now = millis();
        unsigned long pStart1 = phaseStartTimeG[1];
        unsigned long pStart0 = phaseStartTimeG[0];
        unsigned long eOpp = (group == 0) ? (now - pStart1) / 1000 : (now - pStart0) / 1000;
        Phase oppPhase = (group == 0) ? phaseG1 : phaseG0;
        int oppRemaining = 0;
        int gDurOpp = GREEN_TIME + (priorityActive[(group == 0) ? 1 : 0] ? priorityAddSeconds : 0);
        int rDurOpp = RED_TIME   + (priorityActive[(group == 0) ? 0 : 1] ? priorityAddSeconds : 0);

        if (oppPhase == GREEN) {
          oppRemaining = max(0, gDurOpp - (int)eOpp);
        } else if (oppPhase == YELLOW) {
          oppRemaining = max(0, YELLOW_TIME - (int)eOpp);
        } else {
          oppRemaining = max(0, rDurOpp - (int)eOpp);
        }

        if (oppPhase == GREEN) {
            waitSeconds = oppRemaining + YELLOW_TIME;

            res["oppRemaining"] = oppRemaining;
            res["RedCase"] = 0;   // Không phải RED
        } 
        else if (oppPhase == YELLOW) {
            waitSeconds = oppRemaining;

            res["oppRemaining"] = oppRemaining;
            res["RedCase"] = 0;   // Không phải RED
        } 
        else {
            waitSeconds = oppRemaining + ((group == 0) ? gtime1 : gtime0) + YELLOW_TIME;
            res["oppRemaining"] = oppRemaining;
            res["RedCase"] = 1;   // RED case
        }
        Serial.print("Add Second");
        Serial.println(YELLOW_TIME);
        Serial.println(gTi);
        Serial.println(addSeconds);

        res["wait_seconds"] = waitSeconds;
        res["bonus_seconds"] = addSeconds + gTi + YELLOW_TIME;

        requestPriority(group, addSeconds);

      } else {
        res["status"] = "speed not higher than previous";
        res["group"] = group;
      }
    } else {
      minSpeed = speed;
      res["status"] = "ok";
      res["group"] = group;

      unsigned long now = millis();
      unsigned long pStart1 = phaseStartTimeG[1];
      unsigned long pStart0 = phaseStartTimeG[0];
      unsigned long eOpp = (group == 0) ? (now - pStart1) / 1000 : (now - pStart0) / 1000;
      Phase oppPhase = (group == 0) ? phaseG1 : phaseG0;
      int oppRemaining = 0;
      int gDurOpp = GREEN_TIME + (priorityActive[(group == 0) ? 1 : 0] ? priorityAddSeconds : 0);
      int rDurOpp = RED_TIME   + (priorityActive[(group == 0) ? 0 : 1] ? priorityAddSeconds : 0);
      
      if (oppPhase == GREEN) {
        oppRemaining = max(0, gDurOpp - (int)eOpp);
      } else if (oppPhase == YELLOW) {
        oppRemaining = max(0, YELLOW_TIME - (int)eOpp);
      } else {
        oppRemaining = max(0, rDurOpp - (int)eOpp);
      } 
      if (oppPhase == GREEN) {
          waitSeconds = oppRemaining + YELLOW_TIME;

          res["oppRemaining"] = oppRemaining;
          res["RedCase"] = 0;   // Không phải RED
      } 
      else if (oppPhase == YELLOW) {
          waitSeconds = oppRemaining;

          res["oppRemaining"] = oppRemaining;
          res["RedCase"] = 0;   // Không phải RED
      } 
      else {
          waitSeconds = oppRemaining + ((group == 0) ? gtime1 : gtime0) + YELLOW_TIME;

          res["oppRemaining"] = oppRemaining;
          res["RedCase"] = 1;   // RED case
      }
      Serial.print("Add Second2");
      Serial.println(YELLOW_TIME);
      Serial.println(gTi);
      Serial.println(addSeconds);
      requestPriority(group, addSeconds);
      res["wait_seconds"] = waitSeconds;     
      res["bonus_seconds"] = preAddSeconds + gTi + YELLOW_TIME;
    }
  } 
  
  
  
  else {
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
  server.on("/update", handleUpdate);
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
