// Chân kết nối 74HC595
#define DATA_PIN  23
#define LATCH_PIN 26
#define CLOCK_PIN 25

// LED đèn giao thông
#define RED_EW    16
#define YEL_EW    17
#define GRN_EW    18
#define RED_NS    19
#define YEL_NS    21
#define GRN_NS    22

// Bảng mã LED 7 đoạn (cathode chung)
byte digits[10] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);

  pinMode(RED_EW, OUTPUT);
  pinMode(YEL_EW, OUTPUT);
  pinMode(GRN_EW, OUTPUT);
  pinMode(RED_NS, OUTPUT);
  pinMode(YEL_NS, OUTPUT);
  pinMode(GRN_NS, OUTPUT);
}

void displayNumber(int num) {
  int tens = num / 10;
  int ones = num % 10;
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, digits[tens]);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, digits[ones]);
  digitalWrite(LATCH_PIN, HIGH);
}

void loop() {
  // EW xanh, NS đỏ
  digitalWrite(GRN_EW, HIGH); digitalWrite(RED_NS, HIGH);
  for (int t=15; t>0; t--) {
    displayNumber(t);
    delay(1000);
  }

  // EW vàng
  digitalWrite(GRN_EW, LOW); digitalWrite(YEL_EW, HIGH);
  for (int t=3; t>0; t--) {
    displayNumber(t);
    delay(1000);
  }
  digitalWrite(YEL_EW, LOW);
  digitalWrite(RED_NS, LOW);

  // NS xanh, EW đỏ
  digitalWrite(GRN_NS, HIGH); digitalWrite(RED_EW, HIGH);
  for (int t=15; t>0; t--) {
    displayNumber(t);
    delay(1000);
  }

  // NS vàng
  digitalWrite(GRN_NS, LOW); digitalWrite(YEL_NS, HIGH);
  for (int t=3; t>0; t--) {
    displayNumber(t);
    delay(1000);
  }
  digitalWrite(YEL_NS, LOW);
  digitalWrite(RED_EW, LOW);
}
