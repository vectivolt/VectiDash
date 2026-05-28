// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------
//
// WeatherStation — temperature, humidity, pressure, wind direction +
// speed, UV index, rain. Pair with BME280 (temp/hum/press), wind vane
// (analog), anemometer (pulse), VEML7700 (UV). The stub generates a
// believable diurnal curve so the dashboard is meaningful immediately.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <JouleDash.h>
#include <math.h>

AsyncWebServer server(80);
using joule::DashCard;
using joule::DashType;
using joule::DashColor;

DashCard hero  (DashType::Custom,      "hero",  "Weather Station");
DashCard cTemp (DashType::Temperature, "t",     "Temperature",  "°C");
DashCard cFeel (DashType::Temperature, "fl",    "Feels like",   "°C");
DashCard cHum  (DashType::Humidity,    "h",     "Humidity",     "%");
DashCard cPres (DashType::Number,      "p",     "Pressure",     "hPa");
DashCard cUv   (DashType::Gauge,       "uv",    "UV index",     "");
DashCard cWind (DashType::Number,      "ws",    "Wind speed",   "km/h");
DashCard cDir  (DashType::Number,      "wd",    "Wind dir",     "°");
DashCard cRain (DashType::Number,      "r",     "Rain (24h)",   "mm");
DashCard cStat (DashType::Status,      "st",    "Sky");
DashCard cTrend(DashType::Chart,       "trend", "Temp · 24 h");
DashCard cHumCh(DashType::Chart,       "hch",   "Humidity · 24 h");

void setup() {
  Serial.begin(115200);
  WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status() != WL_CONNECTED) delay(200);

  JouleDash.setTitle("Sky · WeatherStation");
  JouleDash.setBrandColor("#0ea5e9");        // sky blue
  JouleDash.setTheme("auto");
  JouleDash.addTab("Now");
  JouleDash.addTab("Trends");

  hero.setWidth(12);
  hero.setCustomHtml(
    "<div style='display:flex;align-items:center;gap:18px;flex-wrap:wrap'>"
      "<div style='width:60px;height:60px;border-radius:16px;display:grid;place-items:center;"
        "background:var(--grad);font-size:32px'>🌤</div>"
      "<div style='flex:1;min-width:240px'>"
        "<div style='font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);font-weight:700'>"
          "Outdoor</div>"
        "<div style='font-size:24px;font-weight:800;background:var(--grad);"
          "-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent'>"
          "<span id='dash-hero-out'>—</span></div>"
        "<div style='font-size:12px;color:var(--muted)'>Updated every 2 s · BME280 / pluviometer / anemometer</div>"
      "</div></div>");

  cTemp.setTab("Now"); cTemp.setWidth(3); cTemp.setColor(DashColor::Warning);
  cFeel.setTab("Now"); cFeel.setWidth(3);
  cHum .setTab("Now"); cHum .setWidth(3); cHum.setColor(DashColor::Info);
  cPres.setTab("Now"); cPres.setWidth(3);
  cUv  .setTab("Now"); cUv  .setWidth(4); cUv.setRange(0,11);
  cWind.setTab("Now"); cWind.setWidth(2);
  cDir .setTab("Now"); cDir .setWidth(2);
  cRain.setTab("Now"); cRain.setWidth(2); cRain.setColor(DashColor::Info);
  cStat.setTab("Now"); cStat.setWidth(2); cStat.setValue("ok");

  cTrend.setTab("Trends"); cTrend.setWidth(12);
  cHumCh.setTab("Trends"); cHumCh.setWidth(12);

  // Pre-seed 24 hourly samples so the chart is meaningful on first load.
  for (int i = 0; i < 24; i++) {
    cTrend .chartPushXY(i, 22 + 6*sin((i-6)/24.0*6.28));
    cHumCh .chartPushXY(i, 55 + 12*cos((i-6)/24.0*6.28));
  }

  JouleDash.add(&hero);
  JouleDash.add(&cTemp); JouleDash.add(&cFeel); JouleDash.add(&cHum); JouleDash.add(&cPres);
  JouleDash.add(&cUv);   JouleDash.add(&cWind); JouleDash.add(&cDir); JouleDash.add(&cRain); JouleDash.add(&cStat);
  JouleDash.add(&cTrend);JouleDash.add(&cHumCh);
  JouleDash.begin(&server, "", "", true);
  server.begin();
}

void loop() {
  static uint32_t last = 0, chartT = 0;
  uint32_t now = millis();
  if (now - last > 2000) {
    last = now;
    float h = ((millis()/1000) % (24*60)) / 60.0f;            // 0..24 simulated hour
    float t = 22 + 6 * sin((h - 6) / 24.0 * 6.28);
    float hum = 55 + 12 * cos((h - 6) / 24.0 * 6.28);
    cTemp.setValue(t, 1);
    cFeel.setValue(t - 2.0 + 0.3 * sin(now/1700.0), 1);
    cHum .setValue(hum, 0);
    cPres.setValue(1013 + 4 * sin(now/9000.0), 1);
    cUv  .setValue(constrain((int)(10 * sin(h/12.0 * 3.14)), 0, 11));
    cWind.setValue(6 + 2 * sin(now/3000.0), 1);
    cDir .setValue((int)((now/100) % 360));
    cRain.setValue(1.4f, 1);
    cStat.setValue(h >= 6 && h <= 19 ? "ok" : "warn");

    String label = (t > 28 ? "Hot" : t > 18 ? "Warm" : "Cool");
    char buf[80]; snprintf(buf, sizeof(buf), "%s · %.1f °C · %.0f %% hum", label.c_str(), t, hum);
    hero.setValue(buf);
  }
  if (now - chartT > 5000) {
    chartT = now;
    float h = ((now/1000) % (24*60)) / 60.0f;
    cTrend .chartPushXY(h, 22 + 6*sin((h-6)/24.0*6.28));
    cHumCh .chartPushXY(h, 55 + 12*cos((h-6)/24.0*6.28));
  }
  JouleDash.tick();
}
