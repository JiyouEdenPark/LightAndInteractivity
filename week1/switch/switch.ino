#include <WiFi.h>
#include <esp_now.h>

// ============================================================================
// Pin Definitions
// ============================================================================
#define BUTTON_PIN BOOT_PIN  // Boot button pin (NanoC6)

// ============================================================================
// Global Variables
// ============================================================================
static bool buttonState = false;
static bool lastButtonState = false;

// Target peer MAC address (replace with actual MAC address of light device)
static const uint8_t PEER_LIGHT_MAC[6] = {0x54, 0x32, 0x04, 0x3F, 0x03, 0x58};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Print MAC address as 12-digit hex string
 */
static void printMac12(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) Serial.print('0');
    Serial.print(mac[i], HEX);
  }
}


/**
 * Ensure peer is registered in ESP-NOW
 */
static bool ensurePeer(const uint8_t mac[6]) {
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = WiFi.channel();  // Use current WiFi channel
    peerInfo.ifidx = WIFI_IF_STA;       // Use station interface
    peerInfo.encrypt = false;
    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK) {
      Serial.print("[esp-now] add_peer failed: ");
      Serial.println(result);
      return false;
    }
  }
  return true;
}

/**
 * Send dim command to peer (called when button is pressed)
 */
static void sendDimCommand() {
  if (!ensurePeer(PEER_LIGHT_MAC)) return;
  
  // Send DIM command when button is pressed
  const char* message = "DIM";
  esp_err_t result = esp_now_send(PEER_LIGHT_MAC, (const uint8_t*)message, strlen(message));
  
  if (result == ESP_OK) {
    Serial.print("[esp-now] sent: ");
    Serial.println(message);
  } else {
    Serial.print("[esp-now] send failed: ");
    Serial.println(result);
  }
}

// ============================================================================
// ESP-NOW Callbacks
// ============================================================================

/**
 * Callback when data is sent
 */
static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("[esp-now] sent to ");
  printMac12(mac_addr);
  Serial.print(" status=");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ============================================================================
// Setup Function
// ============================================================================

/**
 * Initialize the device and ESP-NOW communication
 */
void setup() {
  Serial.begin(115200);
  delay(100);

  // Configure button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastButtonState = digitalRead(BUTTON_PIN);

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed! Rebooting...");
    delay(500);
    ESP.restart();
    return;
  }
  esp_now_register_send_cb(onDataSent);

  // Print own MAC address for peer configuration
  uint8_t selfMac[6];
  WiFi.macAddress(selfMac);
  Serial.print("Self MAC12: ");
  printMac12(selfMac);
  Serial.println();
  Serial.println("ESP-NOW SWITCH ready.");

  // Ensure peer is registered
  ensurePeer(PEER_LIGHT_MAC);
}

// ============================================================================
// Main Loop
// ============================================================================

/**
 * Main program loop
 * Monitors button press and sends ESP-NOW message only when button is pressed
 */
void loop() {
  // Read button state (LOW = pressed due to INPUT_PULLUP)
  buttonState = (digitalRead(BUTTON_PIN) == LOW);
  
  // Send DIM command only when button is pressed (falling edge)
  if (buttonState && !lastButtonState) {
    sendDimCommand();
    Serial.println("[BUTTON] Pressed -> DIM command sent");
  }
  
  lastButtonState = buttonState;
  delay(50);  // Debouncing delay
}
