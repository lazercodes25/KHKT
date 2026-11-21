#include "Arduino.h"
#include <ArduinoJson.h>
#include "DFRobotDFPlayerMini.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "FS.h"
#include "SD.h"
#include "driver/i2s.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>

#define I2S_WS 25
#define I2S_SD 22
#define I2S_SCK 26
#define I2S_PORT I2S_NUM_0
#define SD_CS 5
#define RECORD_TIME 10000
#define SAMPLE_RATE 16000
#define SENSOR_PIN 34
#define RELAY_PIN 12
#define CLEAN_BUTTON 39  // Nút yêu cầu dọn vệ sinh
#define CLEAN_TIMEOUT 14400000
#define SMOKE_PIN 21  //MP2
#define GREEN_LED 16
#define RED_LED 17
#define WATER_BUTTON 36
#define RECYCLE_PIN 33
#define SERVO_PIN 32
#define EVIRONMENT_PIN 35


const char* ssid = "Nha Tro2";
const char* password = "nhatro123123@";
const char* host = "192.168.137.1";
const int port = 5000;
const char* path = "/upload";
volatile bool isPlaying = false;
// URL API Vercel, ví dụ: https://toilet-dashboard.vercel.app/api/telemetry
const char* web_url = "https://smart-toilet-aiot.vercel.app/api/telemetry";
const char* web_device_key = "demo-key"; // phải trùng TOILET_DEVICE_KEY trên Vercel

bool cleaningRequested = false;
unsigned long requestTime = 0;

unsigned long data0ViolenceTime = 0;
unsigned long data1SmokeTime = 0;
unsigned long data2CleanTime = 0;
unsigned long data3RecycleTime = 0;
unsigned long data3LoudTime    = 0;
unsigned long data4ImpactTime  = 0;
unsigned long data5ScreamTime  = 0;
unsigned long data6NH3Time  = 0;

bool buttonState = HIGH;
bool lastButtonState = HIGH;
bool buttonState1 = HIGH;
bool lastButtonState1 = HIGH;

unsigned long lastDebounceTime = 0;
unsigned long lastDebounceTime1 = 0;
const unsigned long debounceDelay = 50;   // chống dội nút
const unsigned long debounceDelay1 = 50;  // chống dội nút

HardwareSerial mySerial(1);
DFRobotDFPlayerMini mp3;

// Queue chứa tên file cần upload
QueueHandle_t uploadQueue;
bool data0Violence = false;
int  data1Smoke = 0;
bool data2Clean = false;
bool data3Loud = false;
bool data4Impact = false;
bool data5Scream = false;
bool data6NH3 = false;

int waterUseCount = 0;
bool flag_recycle = false;

Servo myServo;

bool sendToWeb(String payload) {
  WiFiClientSecure client;
  client.setInsecure(); // Dùng được với ESP32; production nên dùng chứng chỉ CA.
  HTTPClient http;
  if (!http.begin(client, web_url)) {
    Serial.println("❌ Không mở được API web");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", web_device_key);
  int code = http.POST(payload);
  Serial.printf("🌐 API web trả về HTTP %d\n", code);
  http.end();
  return code >= 200 && code < 300;
}

void handleAlert(bool &flag,
                 unsigned long &lastTime,
                 uint8_t mp3Index)
{
  if (flag && (millis() - lastTime > 3000)) {
    lastTime = millis();

    isPlaying = true;
    mp3.play(mp3Index);

    sendTelemetryStates();
  }
}

void sendTelemetryStates() {
  String payload;
  payload = payload + "{\"DATA1\":" + String(data1Smoke);
  //sendEventToThingsBoard("VIOLENCE");
  if (data0Violence)
  {
    payload = payload + ",\"DATA0\":" + String(data0Violence ? 1 : 0) ;
    data0Violence = false;
  }

  if (data1Smoke)
  {
    //payload = payload + "\"DATA1\":" + String(data1Smoke) ;
    //data1Smoke = false;
  }

  if (data2Clean)
  {
    payload = payload + ",\"DATA2\":" + String(data2Clean ? 1 : 0) ;
    data2Clean = false;
  }
  if (data3Loud)
  {
    payload = payload + ",\"DATA3\":" + String(data3Loud ? 1 : 0) ;
    data3Loud = false;
  }
  if (data4Impact)
  {
    payload = payload + ",\"DATA4\":" + String(data4Impact ? 1 : 0) ;
    data4Impact = false;
  }
  if (data5Scream)
  {
    payload = payload + ",\"DATA5\":" + String(data5Scream ? 1 : 0);
    data5Scream = false;
  }
  if (data6NH3)
  {
    payload = payload + ",\"DATA6\":" + String(data6NH3 ? 1 : 0);
    data6NH3 = false;
  }
  payload = payload + "}";
  sendToWeb(String("{\"device_id\":\"toilet-01\",\"values\":") + payload.substring(payload.indexOf('{')) + "}");
  Serial.println("📡 Đã gửi trạng thái lên web: " + payload);
}

void sendWaterUsage() {
  String payload = "{\"DATA7\":" + String(waterUseCount) + "}";
  sendToWeb(String("{\"device_id\":\"toilet-01\",\"values\":") + payload + "}");
  Serial.println("📡 Đã gửi số lần sử dụng vòi nước lên web: " + String(waterUseCount));
}

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
    vTaskDelay(1);
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
      if (line == "\r" || line == "\n" || line.length() == 0) {
        inBody = true;
        continue;
      }
      if (inBody) {
        response += line;
      }
    }
    vTaskDelay(1);
    if (millis() - tStart > 20000) {
      Serial.println("⚠️ Timeout khi chờ phản hồi server!");
      break;
    }
  }
  response.trim();

  // ArduinoJson 7: StaticJsonBuffer/parseObject() belong to ArduinoJson 5.
  JsonDocument jsonDocument;
  DeserializationError jsonError = deserializeJson(jsonDocument, response);


  Serial.println("---- Server response ----");
  Serial.println(response);

  if (jsonError) {
    Serial.print("Loi phan tich JSON: ");
    Serial.println(jsonError.c_str());
    client.stop();
    return false;
  }

  JsonObject root = jsonDocument.as<JsonObject>();

  // Lấy dữ liệu từ server
  // data3Loud     = doc["loud"] == 1; // âm thanh lớn
  // data4Impact  = doc["impact"] == 1; // va cham
  // data5Scream  = doc["scream"] == 1; // tieng het
  // data0Violence = doc["violence_words"] == 1; // ngon tu bao luc
    data3Loud      = root["loud"] == 1;
  data4Impact   = root["impact"] == 1;
  data5Scream   = root["scream"] == 1;
  data0Violence = root["violence_words"] == 1;

  handleAlert(data0Violence, data0ViolenceTime, 1);

  handleAlert(data5Scream, data5ScreamTime, 2);

  handleAlert(data3Loud, data3LoudTime, 3);

  handleAlert(data4Impact, data4ImpactTime, 4);

  sendTelemetryStates();

  client.stop();
  return true;
}

void TaskRecord(void* param) {
  int index = 1;
  while (true) {
    char filename[20];
    while (isPlaying) {
      Serial.println("⏸ DFPlayer đang phát -> tạm dừng ghi âm...");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    sprintf(filename, "/record%d.wav", index);
    Serial.printf("🎙️ Ghi âm file %s ...\n", filename);
    if (recordAudio(filename)) {
      Serial.println("✅ Ghi âm xong, thêm vào hàng đợi upload");
      xQueueSend(uploadQueue, &filename, portMAX_DELAY);
    }
    index++;
    if (index > 9999) index = 1;
    vTaskDelay(pdMS_TO_TICKS(3000));
    yield();
  }
}

void TaskUpload(void* param) {
  char filename[20];
  while (true) {
    if (xQueueReceive(uploadQueue, &filename, portMAX_DELAY) == pdTRUE) {
      char formName[20];
      sprintf(formName, "%s", filename + 1);  // bỏ dấu "/" đầu
      Serial.printf("⬆️ Bắt đầu upload %s...\n", filename);
      if (uploadFile(filename, formName)) {
        SD.remove(filename);
        Serial.println("✅ Upload thành công, xóa file");
      } else {
        Serial.println("⚠️ Upload thất bại, giữ lại file");
      }
    }
  }
  vTaskDelay(pdMS_TO_TICKS(10000));
  yield();
}

void setup() {
  Serial.begin(115200);

  // ==== DFPlayer ====
  mySerial.begin(9600, SERIAL_8N1, 27, 14);
  if (!mp3.begin(mySerial)) {
    Serial.println("❌ Không kết nối được DFPlayer!");
  }
  mp3.volume(25);  // Âm lượng 0–30
  Serial.println("✅ DFPlayer sẵn sàng!");

  // ==== WiFi ====
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi OK!");

  // ==== SD Card ====
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD lỗi!");
  }

  setupI2S();

  pinMode(SENSOR_PIN, INPUT);  // cảm biến vật cản (input)
  pinMode(RELAY_PIN, OUTPUT);  // relay (output)
  pinMode(SMOKE_PIN, INPUT);
  pinMode(EVIRONMENT_PIN, INPUT);
  pinMode(CLEAN_BUTTON, INPUT_PULLUP);
  pinMode(WATER_BUTTON, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(RECYCLE_PIN, INPUT);

  digitalWrite(RELAY_PIN, LOW);  // ban đầu tắt relay

  uploadQueue = xQueueCreate(10, sizeof(char[20]));
  // Không cần ghi âm/upload WAV cho dashboard hiện tại.
  // xTaskCreatePinnedToCore(TaskRecord, "Record", 8192, NULL, 2, NULL, 1);
  // xTaskCreatePinnedToCore(TaskUpload, "Upload", 16384, NULL, 3, NULL, 0);
}

void loop() {
  if (isPlaying && mp3.available()) {
    uint8_t type = mp3.readType();
    if (type == DFPlayerPlayFinished) {
      Serial.println("🔇 DFPlayer đã phát xong!");
      isPlaying = false;
    }
  }

  

  int reading = digitalRead(CLEAN_BUTTON);
  int reading1 = digitalRead(WATER_BUTTON);

  int nh3 = digitalRead(EVIRONMENT_PIN); 
  if ((nh3 == HIGH) && (millis() - data6NH3Time >= 4 * 60 * 1000)) {
    data6NH3Time = millis();
    Serial.println("🚨 Phát hiện NH3!");
    if (!isPlaying) {
      isPlaying = true;
      mp3.play(7);  // 🔊 phát âm thanh NH3
    }
    data6NH3 = true;
    sendTelemetryStates();
    Serial.println("📡 Đã gửi cảnh báo NH3 lên ThingsBoard!");
  }
  if (nh3 != lastButtonState) {
    lastDebounceTime = millis();
  } 



  int sensorValue = digitalRead(SENSOR_PIN);
  if (sensorValue == LOW) {
    digitalWrite(RELAY_PIN, HIGH);  // bật relay
    digitalWrite(GREEN_LED, HIGH);
    if (millis() - data2CleanTime >= 3000) {
      if (!isPlaying) {
        isPlaying = true;
        mp3.play(3);  // 🔊 phát âm "Tay sạch điểm cộng cho bạn"
      }
      data2CleanTime = millis();
      waterUseCount++;
      Serial.println("🚿 Phát hiện sử dụng vòi nước lần " + String(waterUseCount));
      sendWaterUsage();  // Gửi ngay lên ThingsBoard
    }
  } else {
    if (millis() - data2CleanTime >= 4000) {
      digitalWrite(RELAY_PIN, LOW);  // tắt relay
      digitalWrite(GREEN_LED, LOW);
    }
  }




  // ====== 🚬 Phát hiện khói thuốc ======
  int smokeValue = digitalRead(SMOKE_PIN);
  if ((smokeValue == HIGH) && (millis() - data1SmokeTime >= 4 * 60 * 1000)) {
    data1SmokeTime = millis();
    Serial.println("🚨 Phát hiện khói thuốc!");
    if (!isPlaying) {
      isPlaying = true;
      mp3.play(2);  // 🔊 Phát âm cảnh báo khói (Vui lòng không hút thuốc trong nhà vệ sinh)
    }
    data1Smoke++;
    sendTelemetryStates();
    Serial.println("📡 Đã gửi cảnh báo khói thuốc lên ThingsBoard!");
  }
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }



  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      // Nếu nút được nhấn (LOW)
      if (buttonState == LOW) {
        if (!cleaningRequested) {
          //cleaningRequested = true;
          requestTime = millis();
          Serial.println("🧽 Gửi yêu cầu dọn vệ sinh...");
          data2Clean = true;
          sendTelemetryStates();
          Serial.println("📡 Đã gửi yêu cầu dọn vệ sinh lên ThingsBoard!");
          if (!isPlaying) {
            isPlaying = true;
            mp3.play(6);  // 🔊 phát âm cảnh báo (Đã gửi yêu cầu kiểm tra vệ sinh)
          }
        }
      }
    }
  }
  lastButtonState = reading;

  if (reading1 != lastButtonState1) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime1) > debounceDelay1) {
    if (reading1 != buttonState1) {
      buttonState1 = reading1;
      // Nếu nút được nhấn (LOW)
      if (buttonState1 == LOW) {
        if (!isPlaying) {
          isPlaying = true;
          mp3.play(4); // 🔊 phát âm cảnh báo khói (Cảm ơn bạn đã giữ sạch sẽ)
        }
      }
    }
  }
  lastButtonState1 = reading1;


  int recycleValue = digitalRead(RECYCLE_PIN);
  if (recycleValue == LOW) {
    if (flag_recycle == false) {
      if (millis() - data3RecycleTime >= 5000) {
        if (!isPlaying) {
          isPlaying = true;
          mp3.play(5); // 🔊 phát âm cảnh báo khói (Cảm ơn bạn đã bỏ rác vào thùng)
        }
        data3RecycleTime = millis();
      }
      myServo.attach(SERVO_PIN);
      for (int posDegrees = 100; posDegrees >= 20; posDegrees--) {
        myServo.write(posDegrees);
        delay(10);
      }
      myServo.detach();
      flag_recycle = true;
    }
  } else {
    if (flag_recycle == true) {

      if (millis() - data3RecycleTime >= 10000) {
        myServo.attach(SERVO_PIN);
        for (int posDegrees = 20; posDegrees <= 100; posDegrees++) {
          myServo.write(posDegrees);
          delay(10);
        }
        flag_recycle = false;
        myServo.detach();
      }
    }
  }

  delay(100);
}
