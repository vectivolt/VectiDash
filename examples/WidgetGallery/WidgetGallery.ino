// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------
//
// WidgetGallery — every JouleDash widget type rendered in one beautiful
// dashboard, with simulated values that exercise the full visual range.
// Use this sketch as a living style guide: drop into a fresh board, look
// at every component side-by-side, copy what you need into your own sketch.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <JouleDash.h>
#include <math.h>

AsyncWebServer server(80);
using joule::DashCard;
using joule::DashType;
using joule::DashColor;

// One card per widget type — keep them in declaration order so the layout
// reads top-to-bottom matching the JouleDash README's widget catalogue.
DashCard hero    (DashType::Custom,      "hero",  "JouleDash Widget Gallery");

DashCard cNumber (DashType::Number,      "num",   "Number card",      "kW");
DashCard cTemp   (DashType::Temperature, "tmp",   "Temperature",      "°C");
DashCard cHumid  (DashType::Humidity,    "hum",   "Humidity",         "%");
DashCard cStatus (DashType::Status,      "st",    "Online status");
DashCard cInput  (DashType::Input,       "in",    "Free text input");

DashCard cButton (DashType::Button,      "btn",   "Button (click me)");
DashCard cSwitch (DashType::Switch,      "sw",    "Toggle switch");
DashCard cSlider (DashType::Slider,      "sld",   "Slider 0-100",     "%");
DashCard cJoy    (DashType::Joystick,    "joy",   "Joystick (drag)");
DashCard cColor  (DashType::Color,       "col",   "Colour picker");

DashCard cProg   (DashType::Progress,    "prg",   "Progress bar",     "%");
DashCard cGauge  (DashType::Gauge,       "gau",   "Gauge",            "%");
DashCard cDonut  (DashType::Donut,       "don",   "Donut ring",       "%");
DashCard cImage  (DashType::Image,       "img",   "Image card");
DashCard cChart  (DashType::Chart,       "ch",    "Line / area chart");

void setup() {
  Serial.begin(115200);
  WiFi.begin("YOUR_SSID", "YOUR_PASS");
  while (WiFi.status() != WL_CONNECTED) delay(200);

  JouleDash.setTitle("JouleDash · Widget Gallery");
  JouleDash.setBrandColor("#6366f1");        // indigo — JouleSuite default
  JouleDash.setTheme("auto");
  JouleDash.addTab("Display");
  JouleDash.addTab("Interactive");
  JouleDash.addTab("Indicators");

  // ---- Hero ---------------------------------------------------------------
  hero.setWidth(12);
  hero.setCustomHtml(
    "<div style='display:flex;align-items:center;gap:18px;flex-wrap:wrap'>"
      "<div style='width:60px;height:60px;border-radius:16px;display:grid;place-items:center;"
        "background:var(--grad);font-size:30px'>🎨</div>"
      "<div style='flex:1;min-width:220px'>"
        "<div style='font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);font-weight:700'>"
          "Live demo</div>"
        "<div style='font-size:22px;font-weight:800;background:var(--grad);"
          "-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent'>"
          "Every widget JouleDash ships</div>"
        "<div style='font-size:12px;color:var(--muted);margin-top:2px'>"
          "Tick: <span id='dash-hero-out'>—</span></div>"
      "</div></div>");

  // ---- Display tab ---
  cNumber.setTab("Display"); cNumber.setWidth(3); cNumber.setColor(DashColor::Info);
  cTemp  .setTab("Display"); cTemp  .setWidth(3); cTemp  .setColor(DashColor::Warning);
  cHumid .setTab("Display"); cHumid .setWidth(3); cHumid .setColor(DashColor::Info);
  cStatus.setTab("Display"); cStatus.setWidth(3); cStatus.setValue("ok");
  cImage .setTab("Display"); cImage .setWidth(6);
  cInput .setTab("Display"); cInput .setWidth(6);

  // 1x1 PNG placeholder (transparent purple) so the Image card has something
  // pleasing to show without needing an external URL. Replace at runtime
  // with setValue("https://…") to switch to a real source.
  cImage.setValue("https://dummyimage.com/600x180/6366f1/ffffff&text=JouleDash");

  // ---- Interactive tab ---
  cButton.setTab("Interactive"); cButton.setWidth(3); cButton.setColor(DashColor::Primary);
  cSwitch.setTab("Interactive"); cSwitch.setWidth(3);
  cSlider.setTab("Interactive"); cSlider.setWidth(6); cSlider.setRange(0,100);
  cJoy   .setTab("Interactive"); cJoy   .setWidth(6);
  cColor .setTab("Interactive"); cColor .setWidth(6); cColor.setValue("#6366f1");

  cButton.onChange([](const String &){
    JouleDash.notify(joule::NotifyLevel::Success, "Button clicked!", 2000);
  });
  cSwitch.onChange([](const String &v){
    JouleDash.notify(joule::NotifyLevel::Info, String("Switch ") + (v=="1" ? "on":"off"), 1500);
  });
  cSlider.onChange([](const String &v){
    Serial.printf("slider → %s\n", v.c_str());
  });
  cColor.onChange([](const String &v){
    JouleDash.setBrandColor(v);
    JouleDash.refreshLayout();
    JouleDash.notify(joule::NotifyLevel::Info, "Brand: " + v, 2000);
  });

  // ---- Indicators tab ---
  cProg .setTab("Indicators"); cProg .setWidth(6); cProg .setRange(0,100);
  cGauge.setTab("Indicators"); cGauge.setWidth(3); cGauge.setRange(0,100);
  cDonut.setTab("Indicators"); cDonut.setWidth(3); cDonut.setRange(0,100); cDonut.setColor(DashColor::Success);
  cChart.setTab("Indicators"); cChart.setWidth(12);

  // Pre-fill chart so the first viewer sees a populated trace.
  for (int i = 0; i < 30; i++) cChart.chartPushXY(i, 50 + 25*sin(i/4.0));

  JouleDash.add(&hero);
  JouleDash.add(&cNumber); JouleDash.add(&cTemp);  JouleDash.add(&cHumid);  JouleDash.add(&cStatus);
  JouleDash.add(&cImage);  JouleDash.add(&cInput);
  JouleDash.add(&cButton); JouleDash.add(&cSwitch);JouleDash.add(&cSlider); JouleDash.add(&cJoy); JouleDash.add(&cColor);
  JouleDash.add(&cProg);   JouleDash.add(&cGauge); JouleDash.add(&cDonut);  JouleDash.add(&cChart);

  JouleDash.begin(&server, "", "", true);
  server.begin();
  Serial.println("Open http://" + WiFi.localIP().toString() + "/");
}

void loop() {
  static uint32_t last = 0, chartT = 0, startMs = millis();
  uint32_t now = millis();
  if (now - last > 1000) {
    last = now;
    float t = (now - startMs) / 1000.0f;
    cNumber.setValue(7.0f + 0.6f * sin(t/5.0f), 2);
    cTemp  .setValue(23.5f + 1.8f * sin(t/7.0f), 1);
    cHumid .setValue(48.0f + 4.0f * cos(t/9.0f), 1);
    cProg  .setValue((int)((sin(t/11.0f)*0.5f+0.5f)*100));
    cGauge .setValue((int)((cos(t/13.0f)*0.5f+0.5f)*100));
    cDonut .setValue((int)((sin(t/17.0f)*0.5f+0.5f)*100));
    hero   .setValue(String("t = ") + String(t,1) + "s · " + millis() + " ms uptime");
  }
  if (now - chartT > 2000) {
    chartT = now;
    cChart.chartPushXY((now - startMs)/1000.0f, 50 + 25*sin(now/4000.0));
  }
  JouleDash.tick();
}
