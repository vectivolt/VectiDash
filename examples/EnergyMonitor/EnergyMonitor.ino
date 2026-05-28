// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------
//
// EnergyMonitor — production-ready single-phase home energy meter
// dashboard. Pair with a CT-clamp + ADS1115 (or any current/voltage
// sensor) and replace the `readPower()` stub with your real readout.
// Out of the box it streams a plausible household load curve so the UI
// is meaningful immediately.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <JouleDash.h>
#include <math.h>

AsyncWebServer server(80);
using joule::DashCard;
using joule::DashType;
using joule::DashColor;

DashCard hero    (DashType::Custom,   "hero",  "Energy Monitor");
DashCard cNow    (DashType::Number,   "now",   "Current power", "W");
DashCard cToday  (DashType::Number,   "today", "Today",         "kWh");
DashCard cMonth  (DashType::Number,   "mo",    "This month",    "kWh");
DashCard cCost   (DashType::Number,   "cost",  "Today's cost",  "₹");
DashCard cBudget (DashType::Progress, "bud",   "Monthly budget","%");
DashCard cVolt   (DashType::Number,   "v",     "Voltage",       "V");
DashCard cCur    (DashType::Number,   "i",     "Current",       "A");
DashCard cPF     (DashType::Number,   "pf",    "Power factor",  "");
DashCard cFreq   (DashType::Number,   "f",     "Frequency",     "Hz");
DashCard cTrend  (DashType::Chart,    "ch",    "Load over time");
DashCard cMix    (DashType::Donut,    "mix",   "Renewable",     "%");
DashCard cPeak   (DashType::Status,   "peak",  "Peak hours");

float readPower() {
  // Replace with HLW8032 / SCT-013 / ADS1115 reading. The stub fakes a
  // realistic morning-to-evening household load shape.
  float h = ((millis()/1000) % (24*60)) / 60.0f;             // 0..24
  float base = 400.0f;
  base += 800.0f * exp(-pow(h - 8, 2) / 4);                  // breakfast
  base += 600.0f * exp(-pow(h - 13, 2) / 8);                 // midday
  base += 1400.0f * exp(-pow(h - 20, 2) / 6);                // dinner
  return base + 80.0f * sin(millis()/1700.0f);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status() != WL_CONNECTED) delay(200);

  JouleDash.setTitle("Energy Monitor");
  JouleDash.setBrandColor("#10b981");          // green for energy
  JouleDash.setTheme("auto");
  JouleDash.addTab("Live");
  JouleDash.addTab("Quality");
  JouleDash.addTab("Insight");

  hero.setWidth(12);
  hero.setCustomHtml(
    "<div style='display:flex;align-items:center;gap:18px;flex-wrap:wrap'>"
      "<div style='width:60px;height:60px;border-radius:16px;display:grid;place-items:center;"
        "background:var(--grad);font-size:30px'>🔌</div>"
      "<div style='flex:1;min-width:240px'>"
        "<div style='font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);font-weight:700'>"
          "Whole-home meter</div>"
        "<div style='font-size:22px;font-weight:800;background:var(--grad);"
          "-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent'>"
          "<span id='dash-hero-out'>—</span></div>"
        "<div style='font-size:12px;color:var(--muted)'>Tariff: ₹ 8.50/kWh · slab 1</div>"
      "</div></div>");

  cNow   .setTab("Live"); cNow   .setWidth(3); cNow   .setColor(DashColor::Success);
  cToday .setTab("Live"); cToday .setWidth(3);
  cMonth .setTab("Live"); cMonth .setWidth(3);
  cCost  .setTab("Live"); cCost  .setWidth(3); cCost  .setColor(DashColor::Warning);
  cBudget.setTab("Live"); cBudget.setWidth(6); cBudget.setRange(0,100);
  cMix   .setTab("Live"); cMix   .setWidth(3); cMix   .setRange(0,100); cMix.setColor(DashColor::Success);
  cPeak  .setTab("Live"); cPeak  .setWidth(3);

  cVolt  .setTab("Quality"); cVolt  .setWidth(3); cVolt.setColor(DashColor::Info);
  cCur   .setTab("Quality"); cCur   .setWidth(3); cCur .setColor(DashColor::Warning);
  cPF    .setTab("Quality"); cPF    .setWidth(3);
  cFreq  .setTab("Quality"); cFreq  .setWidth(3);
  cTrend .setTab("Insight"); cTrend .setWidth(12);

  cPeak.setValue("ok");
  for (int i = 0; i < 30; i++) cTrend.chartPushXY(i, 600 + 400*sin(i/5.0));

  JouleDash.add(&hero); JouleDash.add(&cNow); JouleDash.add(&cToday); JouleDash.add(&cMonth);
  JouleDash.add(&cCost); JouleDash.add(&cBudget); JouleDash.add(&cMix); JouleDash.add(&cPeak);
  JouleDash.add(&cVolt); JouleDash.add(&cCur); JouleDash.add(&cPF); JouleDash.add(&cFreq);
  JouleDash.add(&cTrend);
  JouleDash.begin(&server, "", "", true);
  server.begin();
}

void loop() {
  static uint32_t last = 0, chartT = 0, dayStart = millis();
  static float totalKwh = 12.4f, monthKwh = 245.0f;
  uint32_t now = millis();
  if (now - last > 1000) {
    last = now;
    float w = readPower();
    totalKwh += w / 3600.0f / 1000.0f;        // W·s → kWh
    monthKwh += w / 3600.0f / 1000.0f;

    cNow  .setValue((int)w);
    cToday.setValue(totalKwh, 2);
    cMonth.setValue(monthKwh, 1);
    cCost .setValue((int)(totalKwh * 8.5f));
    cBudget.setValue((int)constrain(monthKwh / 300.0f * 100.0f, 0.0f, 100.0f));

    cVolt.setValue(230.0f + 1.2f * sin(now/9000.0f), 1);
    cCur .setValue(w / 230.0f, 2);
    cPF  .setValue(0.97f + 0.02f * sin(now/7000.0f), 3);
    cFreq.setValue(50.0f + 0.05f * sin(now/11000.0f), 2);

    cMix .setValue((int)(50 + 30 * sin(now/13000.0f)));

    // 16:00–22:00 marks peak in many tariffs — flip status pill
    int h = ((millis()/1000) % (24*60)) / 60;
    cPeak.setValue(h >= 16 && h < 22 ? "warn" : "ok");

    hero.setValue(String((int)w) + " W now · " + String(totalKwh,1) + " kWh today");
  }
  if (now - chartT > 2000) {
    chartT = now;
    cTrend.chartPushXY((now - dayStart)/1000.0f, readPower());
  }
  JouleDash.tick();
}
