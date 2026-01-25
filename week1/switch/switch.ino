#include <WiFi.h>
#include <esp_now.h>

#define BUTTON_PIN BOOT_PIN

bool buttonState = false;     // Current button state / 현재 버튼 상태
bool lastButtonState = false; // Previous button state / 이전 버튼 상태
const uint8_t PEER_MAC[6] = {
    0x54, 0x32, 0x04,
    0x3F, 0x03, 0x58}; // Light device MAC address / 조명 장치 MAC 주소

void printMac(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16)
      Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}

// Ensure peer is registered in ESP-NOW / ESP-NOW에 피어 등록 확인 및 추가
bool ensurePeer(const uint8_t mac[6]) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = WiFi.channel();
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.println("[esp-now] add_peer failed");
      return false;
    }
  }
  return true;
}

// Send DIM command to light device / 조명 장치에 DIM 명령 전송
void sendDim() {
  if (!ensurePeer(PEER_MAC))
    return;
  const char *message = "DIM";
  esp_err_t result =
      esp_now_send(PEER_MAC, (const uint8_t *)message, strlen(message));
  Serial.print("[esp-now] sent: ");
  Serial.println(result == ESP_OK ? message : "FAIL");
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.print("[esp-now] sent to ");
  printMac(mac);
  Serial.print(" status=");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastButtonState = digitalRead(BUTTON_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed! Rebooting...");
    delay(500);
    ESP.restart();
    return;
  }
  esp_now_register_send_cb(onDataSent);

  // uint8_t mac[6];
  // WiFi.macAddress(mac);
  // Serial.print("Self MAC12: ");
  // printMac(mac);
  // Serial.println();
  Serial.println("ESP-NOW SWITCH ready.");

  ensurePeer(PEER_MAC);
}

void loop() {
  // Read button state (LOW = pressed due to INPUT_PULLUP) / 버튼 상태 읽기
  // (INPUT_PULLUP이므로 LOW = 눌림)
  buttonState = (digitalRead(BUTTON_PIN) == LOW);

  // Send DIM command on button press (rising edge) / 버튼 눌림 시 DIM 명령 전송
  // (상승 엣지)
  if (buttonState && !lastButtonState) {
    sendDim();
    Serial.println("[BUTTON] Pressed");
  }
  lastButtonState = buttonState;
  delay(50); // Debouncing delay / 디바운싱 지연
}
