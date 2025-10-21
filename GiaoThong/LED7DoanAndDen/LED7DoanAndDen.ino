#include <Arduino.h>

// ==== Cấu hình chân 74HC595 ====
#define DATA_PIN 23
#define LATCH_PIN 5
#define CLOCK_PIN 18

// ==== Đèn RGB1 ====
#define R1_PIN 21
#define G1_PIN 22
#define B1_PIN 32

// ==== Đèn RGB2 ====
#define R2_PIN 25
#define G2_PIN 26
#define B2_PIN 27

// ==== Bảng mã 7 đoạn ====
const byte digitCode[10] = {
    B11000000, //0
    B11111001, //1
    B10100100, //2
    B10110000, //3
    B10011001, //4
    B10010010, //5
    B10000011, //6
    B11111000, //7
    B10000000, //8
    B10011000  //9
};

// ==== Hiển thị một số có 6 chữ số ====
void displayNumber(long number) {
  byte digits[8];
  
  // Chia tách thành từng chữ số
  for (int i = 7; i >= 0; i--) {
    digits[i] = number % 10;
    number /= 10;
  }

  digitalWrite(LATCH_PIN, LOW);
  
  // Gửi lần lượt 6 byte cho 6 LED (từ LED phải sang trái)
  for (int i = 7; i >= 0; i--) {
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, digitCode[digits[i]]);
  }

  digitalWrite(LATCH_PIN, HIGH);
}

// ==== Hàm đặt màu cho LED RGB ====
void setRGBColor(bool red1, bool green1, bool blue1, bool red2, bool green2, bool blue2) {
  // Nếu là common cathode → HIGH bật, LOW tắt
  // Nếu là common anode → ngược lại (áp dụng ~red nếu cần)
  digitalWrite(R1_PIN, red1   ? HIGH : LOW);
  digitalWrite(G1_PIN, green1 ? HIGH : LOW);
  digitalWrite(B1_PIN, blue1  ? HIGH : LOW);
  digitalWrite(R2_PIN, red2   ? HIGH : LOW);
  digitalWrite(G2_PIN, green2 ? HIGH : LOW);
  digitalWrite(B2_PIN, blue2  ? HIGH : LOW);
}

void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  pinMode(R1_PIN, OUTPUT);
  pinMode(G1_PIN, OUTPUT);
  pinMode(B1_PIN, OUTPUT);
  pinMode(R2_PIN, OUTPUT);
  pinMode(G2_PIN, OUTPUT);
  pinMode(B2_PIN, OUTPUT);

  Serial.begin(115200);
}

void loop() {

    // Vòng nháy màu: xanh → đỏ → vàng (đỏ+xanh)
  setRGBColor(false, true, false, false, true, false);   // xanh
  delay(500);
  setRGBColor(true, false, false, true, false, false);   // đỏ
  delay(500);
  setRGBColor(true, true, false, true, true, false);    // vàng
  delay(500);

  for (long num = 11111111; num <= 99999999; num++) {
    displayNumber(num);
    delay(100); // tốc độ hiển thị
  }
}
