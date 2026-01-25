#include <WiFi.h>
#include <esp_now.h>

#define LED_PIN 1
#define PWM_FREQ 5000
#define PWM_RES 8
#define DIM_STEP                                                               \
  20 // Brightness decrease amount when button pressed / 버튼 눌렀을 때 밝기
     // 감소량
#define UPDATE_INT                                                             \
  10 // Brightness update interval (ms) / 밝기 업데이트 간격 (밀리초)
#define PLOT_INT                                                               \
  50 // Serial plotter output interval (ms) / 시리얼 플로터 출력 간격 (밀리초)
#define FADE_DUR 7000 // Fade in duration (ms) / 페이드 인 지속 시간 (밀리초)
#define FADE_OUT_DUR                                                           \
  1000 // Fade out duration (ms) / 페이드 아웃 지속 시간 (밀리초)

// Brightness control / 밝기 제어
int brightness = 255; // Current brightness (0-255) / 현재 밝기
int lastUpdateTime =
    0; // Last brightness update time / 마지막 밝기 업데이트 시간
int lastPlotTime =
    0; // Last serial plotter output time / 마지막 시리얼 플로터 출력 시간

// Fade in (brightening) / 페이드 인 (밝아지기)
int fadeStartTime = 0; // Fade in start time / 페이드 인 시작 시간
int fadeStartBrightness =
    0;               // Brightness when fade in started / 페이드 인 시작 시 밝기
bool fadeIn = false; // Fade in active flag / 페이드 인 활성화 플래그

// Fade out (dimming) / 페이드 아웃 (어두워지기)
int fadeOutStartTime = 0; // Fade out start time / 페이드 아웃 시작 시간
int fadeOutStartBrightness =
    0; // Brightness when fade out started / 페이드 아웃 시작 시 밝기
int fadeOutTargetBrightness =
    0;                // Target brightness for fade out / 페이드 아웃 목표 밝기
bool fadeOut = false; // Fade out active flag / 페이드 아웃 활성화 플래그

void printMac(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16)
      Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

void setBrightness(int b) {
  if (b < 0)
    b = 0;
  if (b > 255)
    b = 255;
  brightness = b;
  ledcWrite(LED_PIN, brightness);
}

// Update brightness with fade effects / 페이드 효과를 적용한 밝기 업데이트
void updateBrightness() {
  int currentTime = (int)millis();
  if (currentTime - lastUpdateTime < UPDATE_INT)
    return;
  lastUpdateTime = currentTime;

  // Handle fade out (dimming) - has priority / 페이드 아웃 처리 (우선순위 높음)
  if (fadeOut) {
    int elapsedTime = currentTime - fadeOutStartTime;
    if (elapsedTime >= FADE_OUT_DUR) {
      // Fade out complete / 페이드 아웃 완료
      brightness = fadeOutTargetBrightness;
      fadeOut = false;
      setBrightness(brightness);
      // Start fade in if not at minimum / 최소값이 아니면 페이드 인 시작
      if (brightness > 0) {
        fadeStartTime = currentTime;
        fadeStartBrightness = brightness;
        fadeIn = true;
      }
    } else {
      // Ease-in quadratic curve (t²): slow start, fast end / 이징 곡선: 초반
      // 느리게, 후반 빠르게
      float progress = (float)elapsedTime / FADE_OUT_DUR;
      int newBrightness =
          fadeOutStartBrightness -
          (int)(progress * progress *
                (fadeOutStartBrightness - fadeOutTargetBrightness));
      setBrightness(newBrightness);
    }
  }
  // Handle fade in (brightening) / 페이드 인 처리
  else if (fadeIn) {
    int elapsedTime = currentTime - fadeStartTime;
    if (elapsedTime >= FADE_DUR) {
      // Fade in complete / 페이드 인 완료
      brightness = 255;
      fadeIn = false;
      setBrightness(brightness);
    } else {
      // Ease-in quadratic curve (t²): slow start, fast end / 이징 곡선: 초반
      // 느리게, 후반 빠르게
      float progress = (float)elapsedTime / FADE_DUR;
      int newBrightness =
          fadeStartBrightness +
          (int)(progress * progress * (255 - fadeStartBrightness));
      setBrightness(newBrightness);
    }
  }
  // Auto start fade in if below maximum / 최대값 미만이면 자동으로 페이드 인
  // 시작
  else if (brightness < 255) {
    fadeStartTime = currentTime;
    fadeStartBrightness = brightness;
    fadeIn = true;
  }
}

// ESP-NOW receive callback / ESP-NOW 수신 콜백
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len <= 0)
    return;

  Serial.print("[esp-now] received from ");
  printMac(info->src_addr);
  Serial.print(": ");

  // Parse received message / 수신된 메시지 파싱
  String msg;
  for (int i = 0; i < len; i++) {
    char c = (char)data[i];
    if (isprint(c))
      msg += c;
  }
  msg.trim();
  msg.toUpperCase();
  Serial.println(msg);

  // Handle DIM command / DIM 명령 처리
  if (msg == "DIM") {
    fadeIn = false; // Stop fade in / 페이드 인 중지
    int targetBrightness = brightness - DIM_STEP;
    if (targetBrightness < 0)
      targetBrightness = 0;
    // Start fade out / 페이드 아웃 시작
    fadeOutStartTime = (int)millis();
    fadeOutStartBrightness = brightness;
    fadeOutTargetBrightness = targetBrightness;
    fadeOut = true;
    Serial.print("[LIGHT] Fade out: ");
    Serial.print(brightness);
    Serial.print(" -> ");
    Serial.println(targetBrightness);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed! Rebooting...");
    delay(500);
    ESP.restart();
    return;
  }
  esp_now_register_recv_cb(onDataRecv);

  // uint8_t mac[6];
  // WiFi.macAddress(mac);
  // Serial.print("Self MAC12: ");
  // printMac(mac);
  // Serial.println();
  Serial.println("ESP-NOW LIGHT ready.");

  ledcAttach(LED_PIN, PWM_FREQ, PWM_RES);
  setBrightness(255);
}

void loop() {
  updateBrightness(); // Update brightness with fade effects / 페이드 효과로
                      // 밝기 업데이트

  // Output brightness to serial plotter / 시리얼 플로터에 밝기 출력
  int currentTime = (int)millis();
  if (currentTime - lastPlotTime >= PLOT_INT) {
    Serial.println(brightness);
    lastPlotTime = currentTime;
  }

  delay(10);
}
