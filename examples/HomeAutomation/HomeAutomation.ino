// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------
//
// HomeAutomation — multi-room lights/scenes/HVAC dashboard. Drives 4
// relays + 1 PWM dimmer + 1 RGB strip. Replace the digitalWrite() /
// ledcWrite() stubs with your wiring. Out of the box it just logs the
// intent so the screenshot still shows a populated UI.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <JouleDash.h>

AsyncWebServer server(80);
using joule::DashCard;
using joule::DashType;
using joule::DashColor;

constexpr int PIN_R1=4, PIN_R2=5, PIN_R3=18, PIN_R4=19, PIN_PWM=23, PIN_R=25, PIN_G=26, PIN_B=27;

DashCard hero    (DashType::Custom, "hero",   "Home");
DashCard cLiving (DashType::Switch, "living", "Living-room lights");
DashCard cKit    (DashType::Switch, "kit",    "Kitchen lights");
DashCard cBed    (DashType::Switch, "bed",    "Bedroom lights");
DashCard cPorch  (DashType::Switch, "porch",  "Porch lights");
DashCard cDimmer (DashType::Slider, "dim",    "Dining dimmer", "%");
DashCard cRgb    (DashType::Color,  "rgb",    "RGB strip");
DashCard cAc     (DashType::Slider, "ac",     "AC setpoint",   "°C");
DashCard cAcMode (DashType::Status, "acmd",   "AC mode");
DashCard cTemp   (DashType::Temperature,"t",  "Indoor temp",   "°C");
DashCard cHum    (DashType::Humidity,   "h",  "Indoor humidity","%");
DashCard cPower  (DashType::Number, "p",      "Whole-home load","W");

DashCard sceneMovie(DashType::Button,"sm",    "🎬 Movie");
DashCard sceneSleep(DashType::Button,"ss",    "🌙 Sleep");
DashCard sceneWake (DashType::Button,"sw",    "☀ Wake");
DashCard sceneAway (DashType::Button,"sa",    "🚪 Away");

void setup() {
  Serial.begin(115200);
  pinMode(PIN_R1,OUTPUT); pinMode(PIN_R2,OUTPUT); pinMode(PIN_R3,OUTPUT); pinMode(PIN_R4,OUTPUT);

  WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status()!=WL_CONNECTED) delay(200);

  JouleDash.setTitle("Apartment 12B");
  JouleDash.setBrandColor("#6366f1");           // indigo (JouleSuite default)
  JouleDash.setTheme("auto");
  JouleDash.addTab("Rooms");
  JouleDash.addTab("Climate");
  JouleDash.addTab("Scenes");

  hero.setWidth(12);
  hero.setCustomHtml(
    "<div style='display:flex;align-items:center;gap:18px;flex-wrap:wrap'>"
      "<div style='width:60px;height:60px;border-radius:16px;display:grid;place-items:center;"
        "background:var(--grad);font-size:32px'>🏠</div>"
      "<div style='flex:1;min-width:240px'>"
        "<div style='font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);font-weight:700'>"
          "Welcome home</div>"
        "<div style='font-size:22px;font-weight:800;background:var(--grad);"
          "-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent'>"
          "<span id='dash-hero-out'>—</span></div>"
        "<div style='font-size:12px;color:var(--muted)'>4 lights, 1 dimmer, 1 RGB, HVAC online</div>"
      "</div></div>");

  cLiving.setTab("Rooms"); cLiving.setWidth(3); cLiving.onChange([](const String &v){digitalWrite(PIN_R1,v=="1");});
  cKit   .setTab("Rooms"); cKit   .setWidth(3); cKit   .onChange([](const String &v){digitalWrite(PIN_R2,v=="1");});
  cBed   .setTab("Rooms"); cBed   .setWidth(3); cBed   .onChange([](const String &v){digitalWrite(PIN_R3,v=="1");});
  cPorch .setTab("Rooms"); cPorch .setWidth(3); cPorch .onChange([](const String &v){digitalWrite(PIN_R4,v=="1");});
  cDimmer.setTab("Rooms"); cDimmer.setWidth(6); cDimmer.setRange(0,100);
  cRgb   .setTab("Rooms"); cRgb   .setWidth(6); cRgb.setValue("#6366f1");

  cAc    .setTab("Climate"); cAc    .setWidth(6); cAc.setRange(16,30); cAc.setStep(1); cAc.setValue(22);
  cAcMode.setTab("Climate"); cAcMode.setWidth(3); cAcMode.setValue("ok");
  cTemp  .setTab("Climate"); cTemp  .setWidth(3);
  cHum   .setTab("Climate"); cHum   .setWidth(3);
  cPower .setTab("Climate"); cPower .setWidth(3); cPower.setColor(DashColor::Warning);

  sceneMovie.setTab("Scenes"); sceneMovie.setWidth(3);
  sceneSleep.setTab("Scenes"); sceneSleep.setWidth(3);
  sceneWake .setTab("Scenes"); sceneWake .setWidth(3);
  sceneAway .setTab("Scenes"); sceneAway .setWidth(3); sceneAway.setColor(DashColor::Danger);

  sceneMovie.onChange([](const String&){JouleDash.notify(joule::NotifyLevel::Success,"🎬 Movie scene activated",2500);});
  sceneSleep.onChange([](const String&){JouleDash.notify(joule::NotifyLevel::Info,"🌙 Sleeping — lights off",2500);});
  sceneWake .onChange([](const String&){JouleDash.notify(joule::NotifyLevel::Success,"☀ Good morning",2500);});
  sceneAway .onChange([](const String&){JouleDash.notify(joule::NotifyLevel::Warn,"🚪 Away — house secured",2500);});

  JouleDash.add(&hero);
  JouleDash.add(&cLiving); JouleDash.add(&cKit); JouleDash.add(&cBed); JouleDash.add(&cPorch);
  JouleDash.add(&cDimmer); JouleDash.add(&cRgb);
  JouleDash.add(&cAc); JouleDash.add(&cAcMode); JouleDash.add(&cTemp); JouleDash.add(&cHum); JouleDash.add(&cPower);
  JouleDash.add(&sceneMovie); JouleDash.add(&sceneSleep); JouleDash.add(&sceneWake); JouleDash.add(&sceneAway);
  JouleDash.begin(&server, "", "", true);
  server.begin();
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last > 2000) {
    last = now;
    cTemp .setValue(22.5f + 0.4f * sin(now/9000.0), 1);
    cHum  .setValue(46.0f + 3.0f * cos(now/12000.0), 1);
    cPower.setValue((int)(620 + 120 * sin(now/4000.0)));
    int onCount = 0;
    onCount += digitalRead(PIN_R1) + digitalRead(PIN_R2) + digitalRead(PIN_R3) + digitalRead(PIN_R4);
    hero.setValue(String(onCount) + " of 4 lights on · " + String((int)cTemp.value().toFloat()) + " °C indoors");
  }
  JouleDash.tick();
}
