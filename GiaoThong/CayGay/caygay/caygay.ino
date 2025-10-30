#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define BUTTON_PIN D5   // GPIO14
volatile bool buttonPressed = false;

const char* wifiPrefix = "TrafficLight_";  // Prefix SSID ESP32 phát
bool connected = false;
String esp32IP = "";

void IRAM_ATTR handleButtonInterrupt() {
  buttonPressed = true;   // Gắn cờ khi có ngắt
}

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
        esp32IP = WiFi.gatewayIP().toString(); // Thường là 192.168.4.1
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
  String body = "{\"roadId\":3}";
  int httpCode = http.POST(body);

  if (httpCode > 0) {
    Serial.printf("📨 Gửi thành công (HTTP %d)\n", httpCode);
    Serial.println(http.getString());
  } else {
    Serial.printf("⚠️ Lỗi gửi request: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);
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

  // Đọc lệnh từ Serial Monitor
  if (buttonPressed) {
    buttonPressed = false;
    Serial.println("🔘 Gửi lệnh ưu tiên roadId=3 ...");
    sendPriorityRequest();
  }

  // Đọc lệnh từ Serial Monitor
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("send")) {
      Serial.println("🔘 Gửi lệnh ưu tiên roadId=3 ...");
      sendPriorityRequest();
    } 
    else if (cmd.equalsIgnoreCase("r")) {
      Serial.println("♻️ Reconnect WiFi ...");
      WiFi.disconnect();
      connected = false;
      delay(1000);
      connectToTrafficLight();
    } 
    else if (cmd.length() > 0) {
      Serial.println("❓ Lệnh không hợp lệ. Gõ 'send' hoặc 'r'");
    }
  }

  delay(100);
}
