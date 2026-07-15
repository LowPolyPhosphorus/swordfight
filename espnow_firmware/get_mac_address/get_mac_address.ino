// get_mac_address.ino
// Flash this to any board first to find its MAC address.
// You need the hub's MAC address to put into each sword_sender.ino.

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {}
