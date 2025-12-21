#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define BUTTON_PIN D5  // GPIO14
#define RELAY_PIN D1   // GPIO5
volatile bool buttonPressed = false;

const char* wifiPrefix = "NgaTu_";  // Prefix SSID ESP32 phát
bool connected = false;
String esp32IP = "";
// === Ngắt nút nhấn ===

// === Kết nối WiFi bắt đầu bằng TrafficLight_ ===
void connectToTrafficLight() {
  Serial.println("🔍 Đang quét WiFi...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.startsWith(wifiPrefix)) {
      Serial.printf("📶 Tìm thấy ESP32 SSID: %s\n", ssid.c_str());
      WiFi.begin(ssid.c_str(), "12345678");
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
      }
      if (WiFi.status() == WL_CONNECTED) {
        esp32IP = WiFi.gatewayIP().toString();  // Thường là 192.168.4.1
        Serial.printf("\n✅ Kết nối thành công tới %s | IP ESP32: %s\n", ssid.c_str(), esp32IP.c_str());
        connected = true;
        return;
      }
    }
  }
  Serial.println("❌ Không tìm thấy ESP32 TrafficLight");
  delay(3000);
}

void sendPriorityRequest() {
  if (!connected) {
    Serial.println("⚠️ Chưa kết nối WiFi!");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String url = "http://" + esp32IP + "/set_road";

  if (!http.begin(client, url)) {
    Serial.println("❌ Không thể bắt đầu HTTP request");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  String body = "{\"roadId\":3,\"speed\":0.6}";


  int httpCode = http.POST(body);

  if (httpCode == 200) {
    Serial.printf("📨 Gửi thành công (HTTP %d)\n", httpCode);
    Serial.println(http.getString());
    // phản hồi có trường json wait_seconds
    // TODO: parse JSON and handle wait_seconds
    String response = http.getString();
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, response);
    if (!err) {
      int wait_seconds = doc["wait_seconds"] | -1;
      String status = doc["status"] | "";
      //Đếm ngược nếu như có wait_seconds, nếu đêm về 0 thì bật relay chân D1
      if (wait_seconds >= 0) {
        Serial.printf("⏳ Ưu tiên sẽ được áp dụng sau %d giây\n", wait_seconds);
      } else {
        Serial.println("ℹ️ Phản hồi không có wait_seconds");
      }
      while (wait_seconds > 0) {
        Serial.printf("⏳ Còn %d giây để ưu tiên được áp dụng...\n", wait_seconds);
        delay(1000);
        wait_seconds--;
      }
      if (status == "priority set") {
        digitalWrite(RELAY_PIN, HIGH);  // Bật relay khi đếm ngược xong
        Serial.println("✅ Ưu tiên đã được áp dụng! Relay BẬT.");
        delay(5000);                   // Giữ relay bật trong 5 giây
        digitalWrite(RELAY_PIN, LOW);  // Tắt relay sau 5 giây
        Serial.println("ℹ️ Relay TẮT.");
      }
    } else {
      Serial.println("❌ Lỗi phân tích JSON phản hồi");
    }

  } else {
    Serial.printf("⚠️ Lỗi gửi request: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Đảm bảo relay tắt lúc đầu
  Serial.println("\nESP8266 Priority Controller 🚦");
  connectToTrafficLight();
  Serial.println("Nhập 'send' để gửi ưu tiên, 'r' để reconnect WiFi");
}

void loop() {
  // Nếu mất kết nối, thử lại
  if (WiFi.status() != WL_CONNECTED) {
    connected = false;
    connectToTrafficLight();
  }

  //Nhấn button để gửi lệnh ưu tiên
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("🔘 Nút nhấn được nhấn, gửi lệnh ưu tiên roadId=3 ...");
    sendPriorityRequest();
    // Chờ nút nhấn được thả ra
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(100);
    }
    delay(500);  // debounce
  }

  // Đọc lệnh từ Serial Monitor
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("send")) {
      Serial.println("🔘 Gửi lệnh ưu tiên roadId=3 ...");
      sendPriorityRequest();
    } else if (cmd.equalsIgnoreCase("r")) {
      Serial.println("♻️ Reconnect WiFi ...");
      WiFi.disconnect();
      connected = false;
      delay(1000);
      connectToTrafficLight();
    } else if (cmd.length() > 0) {
      Serial.println("❓ Lệnh không hợp lệ. Gõ 'send' hoặc 'r'");
    }
  }

  delay(100);
}