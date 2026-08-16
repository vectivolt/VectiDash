// ---------------------------------------------------------------------------
// VectiSuite for ESP32 — VectiOTA · VectiSerial · VectiNet · VectiDash
// Author: Chinmoy Bhuyan
// Email:  chinmoy@joulepoint.com
// (c) 2026 VectiVolt — Apache-2.0 License
// ---------------------------------------------------------------------------

// VectiDash minimal example.
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <VectiDash.h>

AsyncWebServer server(80);
vecti::DashCard temp (vecti::DashType::Number,"temp","Temperature","°C");
vecti::DashCard led  (vecti::DashType::Switch,"led","Onboard LED");

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status()!=WL_CONNECTED) delay(200);
  pinMode(LED_BUILTIN,OUTPUT);
  led.onChange([](const String &v){ digitalWrite(LED_BUILTIN, v=="1"?HIGH:LOW); });
  VectiDash.add(&temp);
  VectiDash.add(&led);
  VectiDash.setTitle("Demo");
  VectiDash.begin(&server, "admin","vecti");
  server.begin();
}

void loop(){
  static uint32_t t=0;
  if (millis()-t>1000){ t=millis(); temp.setValue(20.0f + (millis()%20)*0.1f); }
  VectiDash.tick();
}
