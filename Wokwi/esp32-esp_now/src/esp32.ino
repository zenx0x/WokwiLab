#include <WiFi.h>
#include <esp_now.h>

// MAC receiver
uint8_t broadcastAddress[] = {0x8C, 0xAA, 0xB5, 0x84, 0xFB, 0x90};

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Ok" : "Error");
}

//Receiving Callback
void OnDataRecv(const uint8_t * mac, const uint8_t *data, int len) {
  Serial.print("Message received: ");
  char receivedString[len];
  memcpy(receivedString, data, len);
  Serial.println(String(receivedString));
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_now_init();
  //send callback
  esp_now_register_send_cb(OnDataSent);
  //receive callback
  esp_now_register_recv_cb(OnDataRecv);
  // Peer Registration
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  String pp = "Hello!!!Hello!!";
  WiFi.setTxPower(WIFI_POWER_2dBm);
  esp_now_send(broadcastAddress, (uint8_t*)pp.c_str(), pp.length() + 1);

  delay(500);

  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  esp_now_send(broadcastAddress, (uint8_t*)pp.c_str(), pp.length() + 1);

  delay(1000);
}



