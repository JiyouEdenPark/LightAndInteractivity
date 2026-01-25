#include <WiFi.h>
#include <esp_now.h>

// ============================================================================
// Pin Definitions
// ============================================================================
#define LED_PIN    1   // LED pin G1 (PWM capable)

// PWM settings
#define PWM_FREQUENCY 5000
#define PWM_RESOLUTION 8  // 8-bit resolution (0-255)

// ============================================================================
// Global Variables
// ============================================================================

// Brightness control variables
static int currentBrightness = 255;  // Current brightness (0-255)
static unsigned long lastUpdate = 0;
static const int dimStep = 20;       // Brightness decrease step when button is pressed
static const int brightenSpeed = 2;  // Brightness increase speed when idle (higher = faster)
static const int updateInterval = 100; // Update interval in milliseconds

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

// ============================================================================
// LED Control Functions
// ============================================================================

/**
 * Set LED brightness
 * @param brightness Brightness value (0-255)
 */
static void setLedBrightness(int brightness) {
  // Clamp brightness to valid range
  if (brightness < 0) brightness = 0;
  if (brightness > 255) brightness = 255;
  
  currentBrightness = brightness;
  
  // Set LED brightness using PWM
  ledcWrite(LED_PIN, brightness);
}

/**
 * Update LED brightness - gradually brightens when idle
 */
static void updateBrightness() {
  unsigned long currentTime = millis();
  
  // Update at specified interval
  if (currentTime - lastUpdate >= updateInterval) {
    // Always try to brighten when idle (up to maximum 255)
    if (currentBrightness < 255) {
      currentBrightness += brightenSpeed;
      if (currentBrightness > 255) {
        currentBrightness = 255;
      }
      setLedBrightness(currentBrightness);
    }
    
    lastUpdate = currentTime;
  }
}

// ============================================================================
// ESP-NOW Receive Callback
// ============================================================================

/**
 * Callback function called when ESP-NOW data is received
 * Parses the received message and controls LED brightness accordingly
 * @param info ESP-NOW receive information (contains sender MAC address)
 * @param data Received data buffer
 * @param len Length of received data
 */
static void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len <= 0) return;
  
  // Print sender MAC address
  Serial.print("[esp-now] received from ");
  printMac12(info->src_addr);
  Serial.print(": ");
  
  // Extract printable characters from received data
  String message;
  for (int i = 0; i < len; i++) {
    char c = (char)data[i];
    if (isprint(c)) message += c;
  }
  message.trim();
  message.toUpperCase();
  Serial.println(message);

  // Process commands
  if (message == "DIM") {
    // Button pressed: decrease brightness by dimStep
    currentBrightness -= dimStep;
    if (currentBrightness < 0) {
      currentBrightness = 0;
    }
    setLedBrightness(currentBrightness);
    Serial.print("[LIGHT] Dimmed -> Brightness: ");
    Serial.println(currentBrightness);
  }
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

  // Initialize ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed! Rebooting...");
    delay(500);
    ESP.restart();
    return;
  }
  esp_now_register_recv_cb(onDataRecv);

  // Print own MAC address for peer configuration
  uint8_t selfMac[6];
  WiFi.macAddress(selfMac);
  Serial.print("Self MAC12: ");
  printMac12(selfMac);
  Serial.println();
  Serial.println("ESP-NOW LIGHT ready.");

  // Initialize PWM for LED (ESP32-C6 uses ledcAttach)
  ledcAttach(LED_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  setLedBrightness(255);  // Start at full brightness
}

// ============================================================================
// Main Loop
// ============================================================================

/**
 * Main program loop
 * Continuously updates LED brightness for smooth fade effect
 */
void loop() {
  // Update brightness gradually
  updateBrightness();
  
  delay(10);
}
