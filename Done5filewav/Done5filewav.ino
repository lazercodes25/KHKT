#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "driver/i2s.h"

// ==== CHÂN I2S ====
#define I2S_WS  25   // LRCL
#define I2S_SD  22   // DOUT
#define I2S_SCK 26   // BCLK
#define I2S_PORT I2S_NUM_0

// ==== CHÂN SD CARD ====
#define SD_CS   5

// ==== THỜI GIAN GHI (ms) ====
#define RECORD_TIME 20000  
#define SAMPLE_RATE 16000

// ==== FILE OUTPUT ====
File wavFile;

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
      (byte)((sampleRate*2) & 0xff), (byte)(((sampleRate*2)>>8)&0xff),
      (byte)(((sampleRate*2)>>16)&0xff), (byte)(((sampleRate*2)>>24)&0xff),
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SD.begin(SD_CS)) {
    Serial.println("⚠️ Không thể khởi tạo SD card!");
    while (true);
  }
  Serial.println("✅ SD card OK");

  setupI2S();

  for (int i = 1; i <= 5; i++) {
    // Tạo tên file khác nhau
    char filename[20];
    sprintf(filename, "/record%d.wav", i);

    wavFile = SD.open(filename, FILE_WRITE);
    if (!wavFile) {
      Serial.printf("⚠️ Không thể mở file %s để ghi!\n", filename);
      continue;
    }

    // Ghi header tạm
    writeWavHeader(wavFile, SAMPLE_RATE, 0);

    Serial.printf("🎙️ Bắt đầu ghi âm lần %d (20 giây)...\n", i);
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

    // Cập nhật header với kích thước thật
    wavFile.seek(0);
    writeWavHeader(wavFile, SAMPLE_RATE, bytesWritten);
    wavFile.close();

    Serial.printf("✅ Ghi xong file %s\n", filename);
    delay(2000);  // nghỉ 2 giây giữa các lần ghi
  }
}

void loop() {
  // không làm gì thêm
}
