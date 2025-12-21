#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ======================
// ⚙️ WiFi Access Point
// ======================
const char* ssid = "ESP32_AP";
const char* password = "12345678";
unsigned long activeTime = 0;  
const unsigned long ACTIVE_DURATION = 15000; // 15 giây
int activeRoad = -1;            // Lưu trữ đường đang được chọn

WebServer server(80);

// ======================
// 🛣️ Danh sách đường (sửa ở đây nếu muốn)
// ======================
#define ROAD_LIST \
  ROAD(1, "Nguyen Hue") \
  ROAD(2, "Le Loi") \
  ROAD(3, "Tran Hung Dao") \
  ROAD(4, "Pasteur") \
  ROAD(5, "Vo Thi Sau") \
  ROAD(6, "Hai Ba Trung") \
  ROAD(7, "Ly Tu Trong")

// ======================
// ⚙️ Cấu trúc lưu thông tin
// ======================
struct Road {
  int id;
  const char* name;
  int status;  // 0 = chưa chọn, 1 = đang chọn
};

// Tạo mảng roads
Road roadList[] = {
#define ROAD(id, name) { id, name, 0 },
  ROAD_LIST
#undef ROAD
};


const size_t roadCount = sizeof(roadList) / sizeof(roadList[0]);

// ======================
// 📜 / (GET)
// ======================
void handleRoot() {
  server.send(200, "text/plain", "ESP32 API Server is running 🚀");
}

// ======================
// 🚗 /roads (GET)
// ======================
void handleRoads() {
  StaticJsonDocument<512> doc;
  JsonArray roads = doc.createNestedArray("roads");

  for (size_t i = 0; i < roadCount; i++) {
    JsonObject obj = roads.createNestedObject();
    obj["id"] = roadList[i].id;
    obj["name"] = roadList[i].name;
    obj["status"] = roadList[i].status;
  }

  String json;
  serializeJsonPretty(doc, json);
  server.send(200, "application/json", json);

  Serial.println("📤 Gửi danh sách roads:");
  Serial.println(json);
}

// ======================
// 🔧 /set_road (POST)
// ======================
void handleSetRoad() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Use POST method\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.println("📥 Nhận yêu cầu /set_road:");
  Serial.println(body);

  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int roadId = doc["roadId"] | -1;
  if (roadId == -1) {
    server.send(400, "application/json", "{\"error\":\"Missing roadId\"}");
    return;
  }
  // Nếu có đường đang được chọn, thì không cho chọn đường khác trả về thêm trường thời gian còn lại được chọn lại
  if (activeRoad != -1) {
    unsigned long timeLeft = ACTIVE_DURATION - (millis() - activeTime);
    StaticJsonDocument<128> res;
    res["error"] = "Another road is currently active";
    res["activeRoadId"] = roadList[activeRoad].id;
    res["timeLeftMs"] = timeLeft;

    String json;
    serializeJson(res, json);
    server.send(409, "application/json", json);
    Serial.println("⚠️ Đang có đường khác được chọn, không thể chọn đường mới.");
    return;
  }

  bool found = false;
  for (size_t i = 0; i < roadCount; i++) {
    if (roadList[i].id == roadId) {
      found = true;
      // Đặt đường được chọn = 1
      roadList[i].status = 1;
      Serial.printf("✅ Đã chọn đường: %s (ID=%d)\n", roadList[i].name, roadList[i].id);
      // Lưu trạng thái đường đang được chọn và thời gian
      activeRoad = i;
      activeTime = millis();
      break;
    }
  }

  if (!found) {
    server.send(404, "application/json", "{\"error\":\"Road not found\"}");
    return;
  }

  StaticJsonDocument<128> res;
  res["status"] = "ok";
  res["selectedRoadId"] = roadId;

  String json;
  serializeJson(res, json);
  server.send(200, "application/json", json);
}

// ======================
// 🔧 /api (POST)
// ======================
void handleAPI() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    Serial.println("📥 Dữ liệu nhận được:");
    Serial.println(body);

    StaticJsonDocument<200> doc;
    if (deserializeJson(doc, body)) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    const char* cmd = doc["cmd"];
    int roadId = doc["roadId"] | -1;

    Serial.printf("🔧 Lệnh: %s | RoadID: %d\n", cmd, roadId);

    String status = "unknown";
    if (String(cmd) == "on") status = "Device turned ON";
    else if (String(cmd) == "off") status = "Device turned OFF";

    StaticJsonDocument<200> res;
    res["status"] = "ok";
    res["message"] = status;
    res["roadId"] = roadId;

    String json;
    serializeJson(res, json);
    server.send(200, "application/json", json);
  } else {
    server.send(405, "application/json", "{\"error\":\"Use POST method\"}");
  }
}
// ======= Tự động reset sau 15s =======
void checkAutoReset() {
  if (activeRoad != -1 && millis() - activeTime > ACTIVE_DURATION) {
    // Reset trạng thái đường
    roadList[activeRoad].status = 0;
    Serial.printf("✅ Auto reset road %d -> 0\n", activeRoad + 1);
    activeRoad = -1;
  }
}

// ======================
// 🚀 setup()
// ======================
void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFi.softAP(ssid, password);
  Serial.print("📶 ESP32 phát Wi-Fi: ");
  Serial.println(ssid);
  Serial.print("📡 IP ESP32: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/roads", handleRoads);
  server.on("/set_road", handleSetRoad);
  server.on("/api", handleAPI);

  server.begin();
  Serial.println("✅ API Server đã khởi động!");
}

// ======================
// 🔁 loop()
// ======================
void loop() {
  server.handleClient();
  checkAutoReset();
  
}
