// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------
//
// IoTPlant — soil-moisture / light / reservoir / pump dashboard for a
// self-watering planter. Reads a capacitive moisture probe on A0, a
// photoresistor on A1, a tank-level switch on D3, drives a 5 V pump on
// D4. Stubbed values mean the demo runs unmodified on a bare ESP.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <JouleDash.h>
#include <math.h>

AsyncWebServer server(80);
using joule::DashCard;
using joule::DashType;
using joule::DashColor;

constexpr int PIN_PUMP = 4;

DashCard hero  (DashType::Custom,   "hero",   "Plant · Monstera");
DashCard cSoil (DashType::Donut,    "soil",   "Soil moisture", "%");
DashCard cLight(DashType::Number,   "lux",    "Light",        "lx");
DashCard cTemp (DashType::Temperature,"t",    "Air temp",     "°C");
DashCard cHum  (DashType::Humidity, "h",      "Humidity",     "%");
DashCard cTank (DashType::Progress, "tank",   "Water tank",   "%");
DashCard cPump (DashType::Switch,   "pump",   "Pump");
DashCard cAuto (DashType::Switch,   "auto",   "Auto-water");
DashCard cThresh(DashType::Slider,  "th",     "Water below",  "%");
DashCard cChart(DashType::Chart,    "trend",  "Soil moisture · 24 h");
DashCard cMode (DashType::Status,   "mode",   "Mode");

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PUMP, OUTPUT);

  WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status() != WL_CONNECTED) delay(200);

  JouleDash.setTitle("IoT Plant");
  JouleDash.setBrandColor("#10b981");     // plant green
  JouleDash.setTheme("auto");
  JouleDash.addTab("Plant");
  JouleDash.addTab("Settings");

  hero.setWidth(12);
  hero.setCustomHtml(
    "<div style='display:flex;align-items:center;gap:18px;flex-wrap:wrap'>"
      "<div style='width:60px;height:60px;border-radius:16px;display:grid;place-items:center;"
        "background:var(--grad);font-size:32px'>🌱</div>"
      "<div style='flex:1;min-width:240px'>"
        "<div style='font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);font-weight:700'>"
          "Health</div>"
        "<div style='font-size:22px;font-weight:800;background:var(--grad);"
          "-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent'>"
          "<span id='dash-hero-out'>—</span></div>"
        "<div style='font-size:12px;color:var(--muted)'>Last watered: 2 h ago · next check in 8 min</div>"
      "</div></div>");

  cSoil .setTab("Plant"); cSoil .setWidth(4); cSoil .setRange(0,100); cSoil.setColor(DashColor::Info);
  cLight.setTab("Plant"); cLight.setWidth(4); cLight.setColor(DashColor::Warning);
  cTank .setTab("Plant"); cTank .setWidth(4); cTank .setRange(0,100);
  cTemp .setTab("Plant"); cTemp .setWidth(3);
  cHum  .setTab("Plant"); cHum  .setWidth(3);
  cMode .setTab("Plant"); cMode .setWidth(3); cMode.setValue("ok");
  cPump .setTab("Plant"); cPump .setWidth(3);
  cChart.setTab("Plant"); cChart.setWidth(12);

  cAuto  .setTab("Settings"); cAuto  .setWidth(6); cAuto.setValue(true);
  cThresh.setTab("Settings"); cThresh.setWidth(6); cThresh.setRange(10,80); cThresh.setStep(5);

  cPump.onChange([](const String &v){
    digitalWrite(PIN_PUMP, v=="1" ? HIGH : LOW);
    JouleDash.notify(v=="1" ? joule::NotifyLevel::Success : joule::NotifyLevel::Info,
                     v=="1" ? "Pump ON · watering" : "Pump OFF", 2000);
  });
  cAuto.onChange([](const String &v){
    JouleDash.notify(joule::NotifyLevel::Info, v=="1" ? "Auto-water enabled" : "Manual mode", 1500);
  });

  for (int i = 0; i < 24; i++) cChart.chartPushXY(i, 55 + 18*sin(i/4.0));

  JouleDash.add(&hero);
  JouleDash.add(&cSoil); JouleDash.add(&cLight);JouleDash.add(&cTank);
  JouleDash.add(&cTemp); JouleDash.add(&cHum);  JouleDash.add(&cMode); JouleDash.add(&cPump);
  JouleDash.add(&cChart);
  JouleDash.add(&cAuto); JouleDash.add(&cThresh);
  JouleDash.begin(&server, "", "", true);
  server.begin();
}

void loop() {
  static uint32_t last = 0, chartT = 0;
  uint32_t now = millis();
  if (now - last > 2000) {
    last = now;
    int soil = 55 + 18 * sin(now/30000.0);             // simulate slow dry-down
    int lux  = 800  + 1200 * sin(now/40000.0);
    int tank = 78;
    cSoil .setValue(soil);
    cLight.setValue(lux);
    cTank .setValue(tank);
    cTemp .setValue(23.4f + 1.0f * sin(now/13000.0), 1);
    cHum  .setValue(55.0f + 4.0f * cos(now/16000.0), 1);
    cMode .setValue(soil < 35 ? "warn" : "ok");
    hero  .setValue(String(soil) + "% soil · " + String(lux) + " lx · tank " + String(tank) + "%");

    // Auto-water if enabled + dry.
    int thresh = 35;   // value comes from cThresh's onChange in real code
    if (soil < thresh) {
      digitalWrite(PIN_PUMP, HIGH);
    }
  }
  if (now - chartT > 10000) {
    chartT = now;
    cChart.chartPushXY((now/1000)/60.0, 55 + 18 * sin(now/30000.0));
  }
  JouleDash.tick();
}
