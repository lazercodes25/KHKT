#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "FS.h"
#include "SD.h"
#include "driver/i2s.h"
#include <ArduinoJson.h>

#define I2S_WS 25
#define I2S_SD 22
#define I2S_SCK 26
#define I2S_PORT I2S_NUM_0
#define SD_CS 5
#define RECORD_TIME 20000
#define SAMPLE_RATE 16000

// ==== WIFI & SERVER ====
const char* ssid = "DaiDuy";
const char* password = "khongnho";
const char* host = "dominical-astrid-unamusingly.ngrok-free.dev";
const int port = 80;
const char* path = "/upload";



// ==== Queue chứa tên file cần upload ====
QueueHandle_t uploadQueue;

// ====================================
// ==== GHI ÂM ====
// ====================================
void writeWavHeader(File file, uint32_t sampleRate, uint32_t dataSize) {
  uint32_t fileSize = dataSize + 36;
  byte header[44] = {
    'R', 'I', 'F', 'F',
    (byte)(fileSize & 0xff), (byte)((fileSize >> 8) & 0xff),
    (byte)((fileSize >> 16) & 0xff), (byte)((fileSize >> 24) & 0xff),
    'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
    16, 0, 0, 0, 1, 0, 1, 0,
    (byte)(sampleRate & 0xff), (byte)((sampleRate >> 8) & 0xff),
    (byte)((sampleRate >> 16) & 0xff), (byte)((sampleRate >> 24) & 0xff),
    (byte)(sampleRate * 2 & 0xff), (byte)((sampleRate * 2 >> 8) & 0xff),
    (byte)((sampleRate * 2 >> 16) & 0xff), (byte)((sampleRate * 2 >> 24) & 0xff),
    2, 0, 16, 0, 'd', 'a', 't', 'a',
    (byte)(dataSize & 0xff), (byte)((dataSize >> 8) & 0xff),
    (byte)((dataSize >> 16) & 0xff), (byte)((dataSize >> 24) & 0xff)
  };
  file.write(header, 44);
}

void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
  };
  i2s_pin_config_t pin_cfg = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_cfg);
  i2s_zero_dma_buffer(I2S_PORT);
}

bool recordAudio(const char* filename) {
  File f = SD.open(filename, FILE_WRITE);
  if (!f) return false;
  writeWavHeader(f, SAMPLE_RATE, 0);
  uint32_t start = millis(), written = 0;
  int32_t samples[512];
  size_t bytesRead;
  while (millis() - start < RECORD_TIME) {
    i2s_read(I2S_PORT, (void*)samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    for (int i = 0; i < bytesRead / 4; i++) {
      int16_t s = samples[i] >> 14;
      f.write((byte*)&s, 2);
      written += 2;
    }
  }
  f.seek(0);
  writeWavHeader(f, SAMPLE_RATE, written);
  f.close();
  return true;
}

// ====================================
// ==== UPLOAD ====
// ====================================
bool uploadFile(const char* filename, const char* formName) {
  File file = SD.open(filename);
  if (!file) return false;

  String boundary = "----ESP32Boundary";
  String start_req =
    "--" + boundary + "\r\n"
                      "Content-Disposition: form-data; name=\"file\"; filename=\""
    + String(formName) + "\"\r\n"
                         "Content-Type: audio/wav\r\n\r\n";
  String end_req = "\r\n--" + boundary + "--\r\n";

  int contentLength = start_req.length() + file.size() + end_req.length();

  WiFiClient client;


  Serial.printf("\n🔗 Kết nối tới server %s:%d ...\n", host, port);
  int retries = 0;
  while (!client.connect(host, port) && retries < 3) {
    Serial.println("❌ Connection failed, retrying...");
    delay(2000);
    retries++;
  }
  if (retries == 3) {
    Serial.println("🚫 Gửi thất bại sau 3 lần thử.");
    file.close();
    return false;
  }
  Serial.println("✅ Đã kết nối server.");

  client.printf("POST %s HTTP/1.1\r\n", path);
  client.printf("Host: %s\r\n", host);
  client.println("User-Agent: ESP32");
  client.println("Connection: close");
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %d\r\n", contentLength);
  client.println();
  client.print(start_req);
  uint8_t buf[1024];
  while (file.available()) {
    size_t n = file.read(buf, sizeof(buf));
    client.write(buf, n);
  }
  client.print(end_req);
  file.close();

  Serial.printf("📤 Đã gửi %s lên server, chờ phản hồi...\n", filename);
  unsigned long tStart = millis();
  bool inBody = false;
  String response = "";
  // Đọc phản hồi server
  while (client.connected()) {
  while (client.available()) {
    String line = client.readStringUntil('\n');

    // Bỏ qua header, tìm dòng trống giữa header và body
    if (line == "\r" || line == "\n" || line.length() == 0) {
      inBody = true;
      continue;
    }

    // Khi tới phần body thì lấy toàn bộ nội dung
    if (inBody) {
      response += line;
    }
  }

  // timeout 10s
  if (millis() - tStart > 10000) {
    Serial.println("⚠️ Timeout khi chờ phản hồi server!");
    break;
  }
}
  // Phân tích JSON
  response.trim(); 
  int violent = response.toInt();
  Serial.println("---- Server response ----");
  Serial.println(response);
  Serial.println(violent);
  Serial.println("-------------------------");
  if (violent == 1) {
    Serial.println("⚠️ Cảnh báo: Phát hiện âm thanh bạo lực!");
  } else {
    Serial.println("✅ An toàn: Không phát hiện âm thanh bạo lực.");
  }

  client.stop();
  Serial.println("✅ Upload xong!");

  return true;
}

// ====================================
// ==== TASKS ====
// ====================================

// Task ghi âm (Core 1)
void TaskRecord(void* param) {
  int index = 1;
  while (true) {
    char filename[20];
    sprintf(filename, "/record%d.wav", index);
    Serial.printf("🎙️ Ghi âm file %s ...\n", filename);
    if (recordAudio(filename)) {
      Serial.println("✅ Ghi âm xong, thêm vào hàng đợi upload");
      xQueueSend(uploadQueue, &filename, portMAX_DELAY);
    }
    index++;
    if (index > 9999) index = 1;
    vTaskDelay(pdMS_TO_TICKS(5000));  // nghỉ 3 giây
    yield();
  }
}

// Task upload (Core 0)
void TaskUpload(void* param) {
  char filename[20];
  char formName[20];
  //sprintf(filename, "/record%d.wav", index);
  sprintf(formName, "record%d.wav", index);
  while (true) {
    if (xQueueReceive(uploadQueue, &filename, portMAX_DELAY) == pdTRUE) {

      Serial.printf("⬆️ Bắt đầu upload %s...\n", filename);
      if (uploadFile(filename, formName)) {
        SD.remove(filename);
        Serial.println("✅ Upload thành công, xóa file");
      } else {
        Serial.println("⚠️ Upload thất bại, giữ lại file");
      }
    }
  }
  vTaskDelay(pdMS_TO_TICKS(20000));
  yield();
}

// ====================================
// ==== SETUP ====
// ====================================
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("✅ WiFi OK!");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD lỗi!");
    while (1)
      ;
  }
  setupI2S();

  uploadQueue = xQueueCreate(10, sizeof(char[20]));
  xTaskCreatePinnedToCore(TaskRecord, "Record", 8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskUpload, "Upload", 16384, NULL, 3, NULL, 0);
}

void loop() {
  // Sau này bạn có thể đọc cảm biến, xử lý dữ liệu môi trường ở đây.
  // Không bị ảnh hưởng bởi upload hoặc ghi âm.
  //delay(1000);
}
