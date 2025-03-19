/*
  IoT Challenge #1 - Parking Occupancy Detection Node
  
  This code implements a low-power parking occupancy sensor that:
  1. Wakes up periodically from deep sleep
  2. Uses HC-SR04 ultrasonic sensor to detect car presence (optimized for 50cm threshold)
  3. Sends "FREE" or "OCCUPIED" status via ESP-NOW
  4. Implements precise timing for energy consumption analysis
  5. Returns to deep sleep to conserve power
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <HCSR04.h>

// HC-SR04 sensor pins
#define TRIG_PIN 5
#define ECHO_PIN 18

// Parking occupancy threshold
#define MAX_DISTANCE 50  // cm

// Calculate timeout for 50cm detection
// Sound travels at ~343m/s, or 0.0343cm/μs
// For 50cm distance, round trip is 100cm, taking ~2915μs
// Add some margin for safe detection
#define ECHO_TIMEOUT_US 3000  // Timeout for 50cm + margin

// Deep sleep configuration
// X = (last two digits of leader_personcode) mod 50 + 5
// Example: For personcode ending with XX, adjust this value
#define TIME_TO_SLEEP 37  // seconds (modify based on your personcode)
#define uS_TO_S_FACTOR 1000000ULL

// Debug mode enables detailed timing information via Serial
#define DEBUG true

// Sink node MAC address (fixed in the challenge requirements)
uint8_t broadcastAddress[] = {0x8C, 0xAA, 0xB5, 0x84, 0xFB, 0x90};
esp_now_peer_info_t peerInfo;

// Global variables for timing measurements
unsigned long startTime;
unsigned long bootTime;
unsigned long sensorIdleTime;
unsigned long sensorActiveTime;
unsigned long wifiSetupTime;
unsigned long txTime;
unsigned long wifiOffTime;

// Function forward declarations
void printTimestamp(String message);
void setupESPNOW();
bool isOccupied();
void sendParkingStatus(const char* status);
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 1000); // Wait for serial or timeout after 1s
  
  startTime = micros();
  printTimestamp("BOOT START");
  
  // 1. BOOT PERIOD MEASUREMENT
  bootTime = micros();
  
  // 2. SENSOR IDLE PERIOD - Configure HC-SR04 sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  sensorIdleTime = micros();
  printTimestamp("SENSOR IDLE COMPLETE");
  
  // 3. SENSOR ACTIVE PERIOD - Check for occupancy using optimized detection
  bool occupied = isOccupied();
  sensorActiveTime = micros();
  printTimestamp("SENSOR ACTIVE COMPLETE - Occupancy check done");
  
  // Determine parking status based on occupancy
  const char* parkingStatus = occupied ? "OCCUPIED" : "FREE";
  printTimestamp("Parking status: " + String(parkingStatus));
  
  // 4. WIFI SETUP PERIOD - Initialize ESP-NOW
  setupESPNOW();
  wifiSetupTime = micros();
  printTimestamp("WIFI SETUP COMPLETE");
  
  // 5. TX PERIOD - Send parking status
  sendParkingStatus(parkingStatus);
  // Small delay to ensure transmission completes
  delay(20);
  txTime = micros();
  printTimestamp("TX COMPLETE");
  
  // 6. WIFI OFF PERIOD - Disable WiFi to save power
  WiFi.mode(WIFI_OFF);
  wifiOffTime = micros();
  printTimestamp("WIFI OFF COMPLETE");
  
  // Print timing summary for energy analysis
  printEnergyTimingSummary();
  
  // Configure deep sleep wakeup timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  printTimestamp("Going to deep sleep for " + String(TIME_TO_SLEEP) + " seconds");
  
  // Flush serial output before sleep
  Serial.flush();
  
  // Enter deep sleep mode
  esp_deep_sleep_start();
}

void loop() {
  // This is never executed due to deep sleep
}

// Print timestamp and message for debugging
void printTimestamp(String message) {
  if (DEBUG) {
    unsigned long elapsed = micros() - startTime;
    Serial.print(elapsed);
    Serial.print(" us: ");
    Serial.println(message);
  }
}

// Print summary of timing measurements for energy analysis
void printEnergyTimingSummary() {
  Serial.println("\n--- ENERGY TIMING SUMMARY ---");
  Serial.println("Boot period: " + String((sensorIdleTime - bootTime) / 1000.0) + " ms");
  Serial.println("Sensor idle period: " + String((sensorActiveTime - sensorIdleTime) / 1000.0) + " ms");
  Serial.println("Sensor active period: " + String((wifiSetupTime - sensorActiveTime) / 1000.0) + " ms");
  Serial.println("WiFi setup period: " + String((txTime - wifiSetupTime) / 1000.0) + " ms");
  Serial.println("Transmission period: " + String((wifiOffTime - txTime) / 1000.0) + " ms");
  Serial.println("Total active time: " + String((wifiOffTime - bootTime) / 1000.0) + " ms");
  Serial.println("Deep sleep period: " + String(TIME_TO_SLEEP * 1000) + " ms");
  Serial.println("------------------------------\n");
}

// Setup ESP-NOW communication
void setupESPNOW() {
  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);
  
  // Set WiFi power to minimum required level
  WiFi.setTxPower(WIFI_POWER_2dBm);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    printTimestamp("Error initializing ESP-NOW");
    return;
  }
  
  // Register callback for when data is sent
  esp_now_register_send_cb(onDataSent);
  
  // Register peer device
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    printTimestamp("Failed to add peer");
    return;
  }
}

// Check if the parking spot is occupied (optimized for 50cm threshold)
bool isOccupied() {
  // Send trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo pulse duration with optimized timeout
  // If pulseIn returns 0, it means timeout occurred (no echo within ECHO_TIMEOUT_US)
  // which indicates no object detected within MAX_DISTANCE
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  
  if (duration == 0) {
    // No echo received within timeout, parking spot is free
    printTimestamp("No object detected within 50cm");
    return false;
  } else {
    // Echo received before timeout, something is within 50cm
    // Calculate approximate distance for debugging
    float distance = duration * 0.0343 / 2;
    printTimestamp("Object detected at approximately " + String(distance) + " cm");
    return true;
  }
}

// Send parking status via ESP-NOW
void sendParkingStatus(const char* status) {
  printTimestamp("Sending message: " + String(status));
  
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)status, strlen(status) + 1);
  
  if (result != ESP_OK) {
    printTimestamp("Error sending the message");
  }
}

// Callback function when data is sent
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  printTimestamp("Message send status: " + String(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed"));
}