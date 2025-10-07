#include <WiFi.h>
#include <WiFiClientSecure.h>

const char* ssid     = "Thu Thanh 3";
const char* password = "ThuThanh345@";

const char* host = "dominical-astrid-unamusingly.ngrok-free.dev"; // KHÔNG có https://
const int port = 443;
const char* path = "/upload";

void uploadDummyFile() {
  // tạo dữ liệu giả 1KB
  const int dataSize = 1024;
  uint8_t dummyData[dataSize];
  for (int i = 0; i < dataSize; i++) dummyData[i] = random(0, 256);

  String filename = "esp32_" + String(millis()) + ".wav";
  String boundary = "----ESP32Boundary";
  String start_request =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n"
    "Content-Type: application/octet-stream\r\n\r\n";
  String end_request = "\r\n--" + boundary + "--\r\n";

  int contentLength = start_request.length() + dataSize + end_request.length();

  WiFiClientSecure client;
  client.setInsecure(); // bỏ qua SSL cert check

  Serial.printf("\n🔗 Connecting to %s:%d ...\n", host, port);
  if (!client.connect(host, port)) {
    Serial.println("❌ Connection failed!");
    return;
  }
  Serial.println("✅ Connected to server");

  // gửi header HTTP
  client.printf("POST %s HTTP/1.1\r\n", path);
  client.printf("Host: %s\r\n", host);
  client.println("User-Agent: ESP32");
  client.println("Connection: close");
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %d\r\n", contentLength);
  client.println();

  // gửi body
  client.print(start_request);
  client.write(dummyData, dataSize);
  client.print(end_request);

  Serial.println("📤 File uploaded, waiting for server response...");

  // đọc response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }
  String response;
  while (client.available()) response += (char)client.read();

  Serial.println("---- Server response ----");
  Serial.println(response);
  Serial.println("-------------------------");

  client.stop();
  Serial.println("✅ Done. Connection closed.");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

void loop() {
  uploadDummyFile();   // gọi hàm upload
  delay(10000);        // chờ 10 giây rồi gửi tiếp
}