// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------

// JouleDash minimal example.
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <JouleDash.h>

AsyncWebServer server(80);
joule::DashCard temp (joule::DashType::Number,"temp","Temperature","°C");
joule::DashCard led  (joule::DashType::Switch,"led","Onboard LED");

void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status()!=WL_CONNECTED) delay(200);
  pinMode(LED_BUILTIN,OUTPUT);
  led.onChange([](const String &v){ digitalWrite(LED_BUILTIN, v=="1"?HIGH:LOW); });
  JouleDash.add(&temp);
  JouleDash.add(&led);
  JouleDash.setTitle("Demo");
  JouleDash.begin(&server, "admin","joule");
  server.begin();
}

void loop(){
  static uint32_t t=0;
  if (millis()-t>1000){ t=millis(); temp.setValue(20.0f + (millis()%20)*0.1f); }
  JouleDash.tick();
}
