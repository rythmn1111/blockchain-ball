#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_BMP280.h>
#include <MPU6050.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define BUTTON_PIN 25

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Adafruit_BMP280 bmp;
MPU6050 mpu;

enum AppState { HOME, READY_TO_THROW, THROWING, SHOW_RESULT };
AppState state = HOME;

bool throwStarted = false;
float baselineAltitude = 0;
float peakAltitude = 0;
unsigned long throwStartTime;
unsigned long peakTime;
unsigned long lastTelemetryUpdate = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  u8g2.begin();

  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found!");
    while (1);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_1);

  mpu.initialize();
  showHomeScreen();
}

void loop() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    delay(50); // debounce

    if (state == HOME) {
      state = READY_TO_THROW;
      baselineAltitude = bmp.readAltitude(1013.25);
      lastTelemetryUpdate = millis();
      showReadyTelemetry();
    } else if (state == SHOW_RESULT) {
      state = HOME;
      showHomeScreen();
    }
  }
  lastButtonState = currentButtonState;

  if (state == READY_TO_THROW && !throwStarted) {
    // Show telemetry live
    if (millis() - lastTelemetryUpdate >= 200) {
      showReadyTelemetry();
      lastTelemetryUpdate = millis();
    }

    // Detect throw
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    float az_g = az / 16384.0;
    if (az_g > 1.5) {
      throwStarted = true;
      throwStartTime = millis();
      peakAltitude = baselineAltitude;
      showThrowStarted();
      state = THROWING;
    }
  }

  if (state == THROWING) {
    float currentAlt = bmp.readAltitude(1013.25);
    if (currentAlt > peakAltitude) {
      peakAltitude = currentAlt;
      peakTime = millis();
    } else {
      float verticalDistance = peakAltitude - baselineAltitude;
      float timeToPeak = (peakTime - throwStartTime) / 1000.0;
      float verticalSpeed = verticalDistance / timeToPeak;

      showResult(verticalDistance, verticalSpeed);
      throwStarted = false;
      state = SHOW_RESULT;
    }
    delay(50);
  }
}

void showHomeScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 30, "Press to Start");
  u8g2.sendBuffer();
}

void showReadyTelemetry() {
  float currentAlt = bmp.readAltitude(1013.25);
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float az_g = az / 16384.0;

  char buf1[24], buf2[24], buf3[24];
  sprintf(buf1, "Base:   %.2f m", baselineAltitude);
  sprintf(buf2, "Live:   %.2f m", currentAlt);
  sprintf(buf3, "Accel Z: %.2f g", az_g);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, "Ready to Throw");
  u8g2.drawStr(0, 30, buf1);
  u8g2.drawStr(0, 45, buf2);
  u8g2.drawStr(0, 60, buf3);
  u8g2.sendBuffer();
}

void showThrowStarted() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 30, "Throw Started!");
  u8g2.sendBuffer();
}

void showResult(float dist, float speed) {
  char buffer1[20];
  char buffer2[20];
  sprintf(buffer1, "Height: %.2f m", dist);
  sprintf(buffer2, "Speed: %.2f m/s", speed);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 20, "Result:");
  u8g2.drawStr(0, 40, buffer1);
  u8g2.drawStr(0, 60, buffer2);
  u8g2.sendBuffer();
}
