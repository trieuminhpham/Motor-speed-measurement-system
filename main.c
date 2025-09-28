#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khởi tạo LCD với địa chỉ I2C 0x27, 16 cột, 2 hàng
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Định nghĩa các chân kết nối
const int en1 = 2;    // Tín hiệu từ cảm biến Hall
const int sw1 = 4;    // Nút khởi động đo hoặc reset
const int sw2 = 5;    // Nút chọn tốc độ 50%
const int sw3 = 6;    // Nút chọn tốc độ 75% hoặc 100%
const int pwm1 = 10;  // PWM điều khiển động cơ

// Biến cho cảm biến và đo tốc độ
volatile unsigned int dem = 0;                // Đếm xung từ cảm biến Hall
unsigned long lastMeasureTime = 0;           // Thời điểm bắt đầu đo
const unsigned long startMeasureDelay = 3000; // Bắt đầu đếm sau 3 giây
const unsigned long measureDuration = 2000;   // Thời gian đo: 2 giây (từ giây 3 đến giây 5)
const unsigned int pulsesPerRevolution = 7;   // Số xung mỗi vòng quay
const float pi = 3.14159265359;              // Hằng số PI

// Biến trạng thái động cơ và hệ thống
int pwmValue = 0;               // Giá trị PWM cho động cơ
String speedLevel = "OFF";      // Mức công suất hiển thị trên LCD
float angularSpeed = 0.0;       // Tốc độ góc (rad/s)
float frequency = 0.0;          // Tần số (xung/s)
bool isMeasuring = false;       // Trạng thái đang đo
bool isLocked = false;          // Trạng thái khóa kết quả
bool isCounting = false;        // Trạng thái đang đếm xung (giây 3 đến giây 5)

// Biến debounce cho nút bấm và cảm biến
unsigned long lastSw1Time = 0;      // Thời gian nhấn nút sw1
unsigned long lastSw2Time = 0;      // Thời gian nhấn nút sw2
unsigned long lastSw3Time = 0;      // Thời gian nhấn nút sw3
unsigned long lastPulseTime = 0;    // Thời gian nhận xung Hall
const unsigned long debounceDelay = 100;      // Debounce 100ms cho nút
const unsigned long pulseDebounceDelay = 500;  // Debounce 500us cho cảm biến Hall

// Biến xử lý nhấn giữ nút sw3
unsigned long sw3PressTime = 0;  // Thời điểm bắt đầu nhấn sw3
bool sw3Pressed = false;         // Trạng thái nhấn giữ sw3

// Hàm xử lý ngắt từ cảm biến Hall
void dem_xung() {
  unsigned long now = micros();
  if (isCounting && (now - lastPulseTime > pulseDebounceDelay)) {
    dem++;
    lastPulseTime = now;
  }
}

// Hàm cập nhật LCD, chỉ khi nội dung thay đổi
void updateLCD(float angularSpeed, float prequency, String level, String status) {
  static String lastStatus = "";
  static float lastAngularSpeed = -2.0;
  static String lastLevel = "";

  if (lastStatus != status || lastAngularSpeed != angularSpeed || lastLevel != level) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(status);
    if (angularSpeed >= 0) {
      lcd.print(angularSpeed, 2);
      lcd.print("rad/s");
    }
    lcd.setCursor(0, 1);
    lcd.print("Ts: ");
    lcd.print(prequency, 0);
    lcd.print("Hz;P: ");
    lcd.print(level);

    lastStatus = status;
    lastAngularSpeed = angularSpeed;
    lastLevel = level;
  }
}

// Hàm kiểm tra trạng thái nút bấm
void checkButtons() {
  unsigned long now = millis();

  // Kiểm tra nút sw1 (khởi động đo hoặc reset)
  if (digitalRead(sw1) == HIGH && (now - lastSw1Time > debounceDelay)) {
    if (!isMeasuring && !isLocked) {
      // Bắt đầu đo
      isMeasuring = true;
      isCounting = false;
      dem = 0;
      lastMeasureTime = now;
      updateLCD(-1, 00, speedLevel, "Dang do...");
      Serial.println("Bat dau do...");
    } else if (isLocked) {
      // Reset khi đang khóa
      isLocked = false;
      pwmValue = 0;
      speedLevel = "OFF";
      updateLCD(-1, 00, speedLevel, "San sang do");
      Serial.println("Reset: San sang do");
    }
    lastSw1Time = now;
  }

  // Kiểm tra nút sw2 (50%)
  if (digitalRead(sw2) == HIGH && (now - lastSw2Time > debounceDelay) && !isMeasuring) {
    pwmValue = 128;
    speedLevel = "50%";
    isLocked = false; // Mở khóa để sẵn sàng đo
    lastSw2Time = now;
    updateLCD(-1, 0, speedLevel, "San sang do");
    Serial.println("Chon cong suat: 50%");
  }

  // Kiểm tra nút sw3 (75% hoặc 100%)
  if (digitalRead(sw3) == HIGH && (now - lastSw3Time > debounceDelay) && !isMeasuring) {
    if (!sw3Pressed) {
      sw3Pressed = true;
      sw3PressTime = now;
    }
    if (now - sw3PressTime > 1000) {
      pwmValue = 255;
      speedLevel = "100%";
    } else {
      pwmValue = 191;
      speedLevel = "75%";
    }
    isLocked = false; // Mở khóa để sẵn sàng đo
    lastSw3Time = now;
    updateLCD(-1, 0, speedLevel, "San sang do");
    Serial.println("Chon cong suat: " + speedLevel);
  } else if (digitalRead(sw3) == LOW && sw3Pressed) {
    sw3Pressed = false;
  }

  // Ghi giá trị PWM cho động cơ
  analogWrite(pwm1, pwmValue);
}

void setup() {
  // Khởi tạo Serial và LCD
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Cấu hình chân
  pinMode(en1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(en1), dem_xung, RISING);
  pinMode(pwm1, OUTPUT);
  pinMode(sw1, INPUT_PULLUP);
  pinMode(sw2, INPUT_PULLUP);
  pinMode(sw3, INPUT_PULLUP);

  // Hiển thị trạng thái ban đầu
  updateLCD(-1, 0, speedLevel, "San sang do");
  Serial.println("He thong khoi dong");
}

void loop() {
  checkButtons();

  if (isMeasuring && !isLocked) {
    unsigned long now = millis();
    unsigned long elapsedTime = now - lastMeasureTime;

    // Bắt đầu đếm xung từ giây thứ 3
    if (elapsedTime >= startMeasureDelay && elapsedTime < startMeasureDelay + measureDuration) {
      if (!isCounting) {
        isCounting = true;
        dem = 0; // Đặt lại bộ đếm trước khi bắt đầu đếm
        Serial.println("Bat dau dem xung...");
      }
    }
    // Kết thúc đo sau giây thứ 5
    else if (elapsedTime >= startMeasureDelay + measureDuration) {
      noInterrupts();
      unsigned int countCopy = dem;
      dem = 0;
      isCounting = false;
      interrupts();

      // Tính tốc độ góc (rad/s)
      angularSpeed = (float)countCopy * 2 * pi / (pulsesPerRevolution * (measureDuration / 1000.0)) / 50;
      float prequency = (float)countCopy / 2;

      // Hiển thị và chốt kết quả
      isMeasuring = false;
      isLocked = true;
      updateLCD(angularSpeed, prequency, speedLevel, "Chot: ");
      Serial.print("Toc do: ");
      Serial.print(angularSpeed, 2);
      Serial.println(" rad/s");
    }
  }
}