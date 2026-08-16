// ---------------------------------------------------------------------------
// VectiSuite for ESP32 — VectiOTA · VectiSerial · VectiNet · VectiDash
// Author: Chinmoy Bhuyan
// Email:  chinmoy@joulepoint.com
// (c) 2026 VectiVolt — Apache-2.0 License
// ---------------------------------------------------------------------------
//
// HomeAutomation — multi-room lights/scenes/HVAC dashboard. Drives 4
// relays (GPIO 4/5/18/19), a PWM dimmer (GPIO 23) and an RGB strip
// (GPIO 25/26/27). The AC setpoint has no actuator here; it only drives
// the mode pill. Sensor values are simulated so the demo runs on a bare
// ESP with nothing wired.

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <VectiDash.h>

AsyncWebServer server(80);
using vecti::DashCard;
using vecti::DashType;
using vecti::DashColor;

constexpr int PIN_R1=4, PIN_R2=5, PIN_R3=18, PIN_R4=19, PIN_PWM=23, PIN_R=25, PIN_G=26, PIN_B=27;
constexpr int CH_PWM=0, CH_R=1, CH_G=2, CH_B=3;   // LEDC channels

// LEDC is channel-addressed on arduino-esp32 2.x (ledcSetup + ledcAttachPin,
// then ledcWrite(channel, ...)) and pin-addressed on 3.x, which dropped those
// two functions. This repo builds against 2.0.17; keep both cores working.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  #define PWM_BEGIN(pin, ch)        ledcAttachChannel((pin), 5000, 8, (ch))
  #define PWM_WRITE(pin, ch, duty)  ledcWrite((pin), (duty))
#else
  #define PWM_BEGIN(pin, ch)        (ledcSetup((ch), 5000, 8), ledcAttachPin((pin), (ch)))
  #define PWM_WRITE(pin, ch, duty)  ledcWrite((ch), (duty))
#endif

int acSetpoint = 22;    // °C, driven by cAc

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
  // 8-bit PWM so the slider's 0..100 and the picker's 0x00..0xff map straight on.
  PWM_BEGIN(PIN_PWM, CH_PWM);
  PWM_BEGIN(PIN_R, CH_R); PWM_BEGIN(PIN_G, CH_G); PWM_BEGIN(PIN_B, CH_B);

  WiFi.begin("YOUR_SSID","YOUR_PASS");
  while (WiFi.status()!=WL_CONNECTED) delay(200);

  VectiDash.setTitle("Apartment 12B");
  VectiDash.setBrandColor("#6366f1");           // indigo (VectiSuite default)
  VectiDash.setTheme("auto");
  VectiDash.addTab("Rooms");
  VectiDash.addTab("Climate");
  VectiDash.addTab("Scenes");

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
  cDimmer.onChange([](const String &v){ PWM_WRITE(PIN_PWM, CH_PWM, v.toInt()*255/100); });
  cRgb   .setTab("Rooms"); cRgb   .setWidth(6); cRgb.setValue("#6366f1");
  cRgb   .onChange([](const String &v){
    // The colour card sends "#rrggbb".
    long rgb = strtol(v.c_str() + (v.startsWith("#") ? 1 : 0), nullptr, 16);
    PWM_WRITE(PIN_R, CH_R, (rgb>>16)&0xff);
    PWM_WRITE(PIN_G, CH_G, (rgb>>8)&0xff);
    PWM_WRITE(PIN_B, CH_B, rgb&0xff);
  });

  cAc    .setTab("Climate"); cAc    .setWidth(6); cAc.setRange(16,30); cAc.setStep(1);
  cAc.setValue(acSetpoint);
  cAc.onChange([](const String &v){ acSetpoint = v.toInt(); });
  cAcMode.setTab("Climate"); cAcMode.setWidth(3); cAcMode.setValue("ok");
  cTemp  .setTab("Climate"); cTemp  .setWidth(3);
  cHum   .setTab("Climate"); cHum   .setWidth(3);
  cPower .setTab("Climate"); cPower .setWidth(3); cPower.setColor(DashColor::Warning);

  sceneMovie.setTab("Scenes"); sceneMovie.setWidth(3);
  sceneSleep.setTab("Scenes"); sceneSleep.setWidth(3);
  sceneWake .setTab("Scenes"); sceneWake .setWidth(3);
  sceneAway .setTab("Scenes"); sceneAway .setWidth(3); sceneAway.setColor(DashColor::Danger);

  sceneMovie.onChange([](const String&){VectiDash.notify(vecti::NotifyLevel::Success,"🎬 Movie scene activated",2500);});
  sceneSleep.onChange([](const String&){VectiDash.notify(vecti::NotifyLevel::Info,"🌙 Sleeping — lights off",2500);});
  sceneWake .onChange([](const String&){VectiDash.notify(vecti::NotifyLevel::Success,"☀ Good morning",2500);});
  sceneAway .onChange([](const String&){VectiDash.notify(vecti::NotifyLevel::Warn,"🚪 Away — house secured",2500);});

  VectiDash.add(&hero);
  VectiDash.add(&cLiving); VectiDash.add(&cKit); VectiDash.add(&cBed); VectiDash.add(&cPorch);
  VectiDash.add(&cDimmer); VectiDash.add(&cRgb);
  VectiDash.add(&cAc); VectiDash.add(&cAcMode); VectiDash.add(&cTemp); VectiDash.add(&cHum); VectiDash.add(&cPower);
  VectiDash.add(&sceneMovie); VectiDash.add(&sceneSleep); VectiDash.add(&sceneWake); VectiDash.add(&sceneAway);
  VectiDash.begin(&server, "", "", true);
  server.begin();
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last > 2000) {
    last = now;
    float indoor = 22.5f + 0.4f * sin(now/9000.0);
    cTemp .setValue(indoor, 1);
    cHum  .setValue(46.0f + 3.0f * cos(now/12000.0), 1);
    cPower.setValue((int)(620 + 120 * sin(now/4000.0)));
    cAcMode.setValue(indoor > acSetpoint + 0.5f ? "cooling" : "ok");
    int onCount = 0;
    onCount += digitalRead(PIN_R1) + digitalRead(PIN_R2) + digitalRead(PIN_R3) + digitalRead(PIN_R4);
    hero.setValue(String(onCount) + " of 4 lights on · " + String((int)cTemp.value().toFloat()) + " °C indoors");
  }
  VectiDash.tick();
}
