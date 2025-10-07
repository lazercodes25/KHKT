#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "FS.h"
#include "SD.h"
#include "driver/i2s.h"

// ==== WIFI CONFIG ====
const char* ssid     = "Thu Thanh 3";
const char* password = "ThuThanh345@";

// ==== SERVER CONFIG ====
const char* host = "dominical-astrid-unamusingly.ngrok-free.dev";  // KHÔNG có https://
const int port = 443;
const char* path = "/upload";

// ==== I2S PINS ====
#define I2S_WS  25   // LRCL
#define I2S_SD  22   // DOUT
#define I2S_SCK 26   // BCLK
#define I2S_PORT I2S_NUM_0

// ==== SD CARD ====
#define SD_CS   5

// ==== RECORD SETTINGS ====
#define RECORD_TIME 20000  // 20 giây
#define SAMPLE_RATE 16000

// ---- WAV HEADER ----
void writeWavHeader(File file, uint32_t sampleRate, uint32_t dataSize) {
  uint32_t fileSize = dataSize + 36;
  byte header[44] = {
      'R','I','F','F',
      (byte)(fileSize & 0xff), (byte)((fileSize >> 8) & 0xff),
      (byte)((fileSize >> 16) & 0xff), (byte)((fileSize >> 24) & 0xff),
      'W','A','V','E','f','m','t',' ',
      16,0,0,0,1,0,1,0,
      (byte)(sampleRate & 0xff), (byte)((sampleRate >> 8) & 0xff),
      (byte)((sampleRate >> 16) & 0xff), (byte)((sampleRate >> 24) & 0xff),
      (byte)(sampleRate*2 & 0xff), (byte)((sampleRate*2 >> 8) & 0xff),
      (byte)((sampleRate*2 >> 16) & 0xff), (byte)((sampleRate*2 >> 24) & 0xff),
      2,0,16,0,
      'd','a','t','a',
      (byte)(dataSize & 0xff), (byte)((dataSize >> 8) & 0xff),
      (byte)((dataSize >> 16) & 0xff), (byte)((dataSize >> 24) & 0xff)
  };
  file.write(header, 44);
}

// ---- SETUP I2S ----
void setupI2S() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK,
      .ws_io_num = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

// ---- GHI ÂM 20 GIÂY VÀ LƯU FILE ----
bool recordAudio(const char* filename) {
  File wavFile = SD.open(filename, FILE_WRITE);
  if (!wavFile) {
    Serial.printf("⚠️ Không thể mở file %s để ghi!\n", filename);
    return false;
  }

  writeWavHeader(wavFile, SAMPLE_RATE, 0);
  Serial.printf("🎙️ Bắt đầu ghi âm %s (20 giây)...\n", filename);

  uint32_t start = millis();
  uint32_t bytesWritten = 0;
  int32_t samples[512];
  size_t bytesRead;

  while (millis() - start < RECORD_TIME) {
    i2s_read(I2S_PORT, (void*)samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    for (int j = 0; j < bytesRead / 4; j++) {
      int16_t s = samples[j] >> 14;
      wavFile.write((byte*)&s, 2);
      bytesWritten += 2;
    }
  }

  wavFile.seek(0);
  writeWavHeader(wavFile, SAMPLE_RATE, bytesWritten);
  wavFile.close();

  Serial.printf("✅ Ghi xong file %s\n", filename);
  return true;
}

// ---- UPLOAD FILE WAV LÊN SERVER ----
bool uploadFile(const char* filename, const char* formName) {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.printf("⚠️ Không thể mở %s để upload!\n", filename);
    return false;
  }

  String boundary = "----ESP32Boundary";
  String start_request =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"" + String(formName) + "\"\r\n"
    "Content-Type: audio/wav\r\n\r\n";
  String end_request = "\r\n--" + boundary + "--\r\n";

  int contentLength = start_request.length() + file.size() + end_request.length();

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);  // 15 giây thay vì mặc định 5 giây


  Serial.printf("\n🔗 Kết nối tới server %s:%d ...\n", host, port);
  // if (!client.connect(host, port)) {
  //   Serial.println("❌ Kết nối thất bại!");
  //   file.close();
  //   return false;
  // }
  // Serial.println("✅ Đã kết nối server.");


  int retries = 0;
  while (!client.connect(host, port) && retries < 3) {
    Serial.println("❌ Connection failed, retrying...");
    delay(2000);
    retries++;
  }
  if (retries == 3) {
    Serial.println("🚫 Gửi thất bại sau 3 lần thử.");
    return false;
  }
  Serial.println("✅ Đã kết nối server.");

  // Gửi header HTTP
  client.printf("POST %s HTTP/1.1\r\n", path);
  client.printf("Host: %s\r\n", host);
  client.println("User-Agent: ESP32");
  client.println("Connection: close");
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %d\r\n", contentLength);
  client.println();

  // Gửi body
  client.print(start_request);
  uint8_t buffer[1024];
  while (file.available()) {
    size_t bytes = file.read(buffer, sizeof(buffer));
    client.write(buffer, bytes);
  }
  client.print(end_request);
  file.close();

  Serial.println("📤 Đã gửi file, chờ phản hồi server...");

  // Đọc phản hồi server
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response;
  while (client.available()) {
    response += (char)client.read();
  }

  Serial.println("---- Server response ----");
  Serial.println(response);
  Serial.println("-------------------------");

  client.stop();
  Serial.println("✅ Upload xong!");

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.print("🔌 Kết nối WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n✅ WiFi OK! IP: %s\n", WiFi.localIP().toString().c_str());

  // Khởi tạo SD
  if (!SD.begin(SD_CS)) {
    Serial.println("⚠️ Lỗi SD card!");
    while (1);
  }
  Serial.println("✅ SD card OK");

  setupI2S();
}

void loop() {
  static int index = 1;
  char filename[20];
  sprintf(filename, "/record%d.wav", index);

  // tên file gửi lên server (KHÔNG có dấu / ở đầu)
  char formName[20];
  sprintf(formName, "record%d.wav", index);

  if (recordAudio(filename)) {
    if (uploadFile(filename, formName)) {
      Serial.println("🗑️ Xóa file sau khi upload thành công...");
      SD.remove(filename);
    } else {
      Serial.println("⚠️ Upload thất bại, giữ file để thử lại sau.");
    }
  }

  index++;
  if (index > 9999) index = 1;

  Serial.println("⏳ Nghỉ 3 giây trước lần ghi tiếp theo...");
  delay(3000);
}
