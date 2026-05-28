// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------

// JouleDash — real-time IoT dashboard, all widgets free, all source MIT.
//
// Why this exists: ESP-DASH paywalls every interesting widget behind Pro,
// runs on Server-Sent Events (broken on Safari over long sessions), and
// gives you no escape hatch for a custom UI snippet. JouleDash includes
// every widget in the box, transports over a single WebSocket, and adds:
//
//   * Widgets: NUMBER, BUTTON, SWITCH, SLIDER, GAUGE, PROGRESS, STATUS,
//     TEMP, HUMIDITY, IMAGE, INPUT, JOYSTICK, COLOR/RGB, CHART (line, bar,
//     area), CUSTOM (raw HTML / DOM ID the host sketch updates).
//   * Tabs — multi-page dashboards, drag-reorder cards.
//   * Themes: dark / light / auto, with brand-color override.
//   * Notifications — push toast or persistent banners from the firmware.
//   * Auth (Basic) and an optional anonymous-read-only mode.
//   * Multi-client sync — every interaction is rebroadcast so all open tabs
//     stay consistent.
//
// Usage:
//
//     #include <JouleDash.h>
//     joule::DashCard temp(joule::DashType::Number, "temp", "Temperature","°C", 0, 50);
//     void setup(){ JouleDash.add(&temp); JouleDash.begin(&server,"admin","joule"); }
//     void loop(){ temp.setValue(readTemp()); JouleDash.tick(); }
#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>
#include <vector>

namespace joule {

enum class DashType : uint8_t {
  Number=0, Button, Switch, Slider, Gauge, Progress,
  Status, Temperature, Humidity, Image, Input, Joystick,
  Color, Chart, Custom, Donut
};

enum class DashColor : uint8_t {
  Default=0, Success, Warning, Danger, Info, Primary
};

enum class ChartType : uint8_t { Line=0, Bar, Area };

class DashCardBase;

using DashChangeCb = std::function<void(const String &payload)>;

class DashCardBase {
public:
  DashCardBase(DashType t, const String &id, const String &label)
    : _type(t), _id(id), _label(label) {}
  // 4-arg overload: convenience for `DashCard(type, "id", "Label", "unit")`.
  // The unit is the small grey suffix shown next to the value on number-
  // like widgets (gauge, slider, progress, number, temperature, humidity).
  DashCardBase(DashType t, const String &id, const String &label, const String &unit)
    : _type(t), _id(id), _label(label), _unit(unit) {}
  // 6-arg overload mirrors ESPDash's signature for easier migration:
  // `DashCard(type, "id", "Label", "unit", min, max)`.
  DashCardBase(DashType t, const String &id, const String &label, const String &unit, float lo, float hi)
    : _type(t), _id(id), _label(label), _unit(unit), _rmin(lo), _rmax(hi) {}
  virtual ~DashCardBase() = default;

  void setLabel(const String &s) { _label = s; _dirty = true; }
  void setUnit(const String &s)  { _unit  = s; _dirty = true; }
  void setColor(DashColor c)     { _color = c; _dirty = true; }
  void setTab(const String &t)   { _tab   = t; _dirty = true; }
  void setHidden(bool h)         { _hidden=h;  _dirty = true; }
  // Size in 12-column grid units (1..12). 0 = auto.
  void setWidth(uint8_t w)       { _width = w; _dirty = true; }

  void onChange(DashChangeCb cb) { _onChange = std::move(cb); }

  // ---- value setters (numeric overloads for ergonomic call sites) ----
  void setValue(int v)         { setValueStr(String(v)); }
  void setValue(unsigned v)    { setValueStr(String(v)); }
  void setValue(long v)        { setValueStr(String(v)); }
  void setValue(float v, int digits=2) { setValueStr(String(v, digits)); }
  void setValue(double v, int digits=2){ setValueStr(String((float)v, digits)); }
  void setValue(bool v)        { setValueStr(v?"1":"0"); }
  void setValue(const char *s) { setValueStr(String(s)); }
  void setValue(const String &s){ setValueStr(s); }
  void setValueStr(const String &s);

  // ---- range (for Slider / Gauge / Number / Progress) ----
  void setRange(float lo, float hi) { _rmin = lo; _rmax = hi; _dirty = true; }
  void setStep (float st)            { _rstep = st; _dirty = true; }

  // ---- chart helpers ----
  void chartPushXY(float x, float y);   // appends a point; rolls at maxPoints
  void chartSetSeries(const float *xs, const float *ys, size_t n);
  void chartSetMaxPoints(size_t n) { _chartMax = n; }
  void chartSetType(ChartType t)   { _chartType = t; _dirty = true; }

  // ---- custom widget escape hatch ----
  // Set an HTML snippet to render inside the card body. The snippet may
  // include a <span id="dash-<id>-out"></span> the firmware updates via
  // setValue() — the runtime injects the value into that span on every
  // broadcast. For more bespoke widgets, listen for "joulecmd" events on
  // window in your snippet and respond via /dash/cmd.
  void setCustomHtml(const String &html) { _custom = html; _dirty = true; }

  // ---- meta ----
  DashType type() const  { return _type; }
  String   id()   const  { return _id; }
  bool     dirty() const { return _dirty; }
  void     clean()       { _dirty = false; }

  void describe(class JsonObject &o) const;
  String value() const { return _val; }
  void   ingest(const String &payload);

private:
  DashType  _type;
  String    _id, _label, _unit, _tab, _val, _custom;
  DashColor _color = DashColor::Default;
  bool      _hidden = false;
  bool      _dirty  = true;
  uint8_t   _width  = 0;

  float _rmin=0, _rmax=100, _rstep=1;

  ChartType _chartType = ChartType::Line;
  size_t    _chartMax  = 50;
  std::vector<float> _chartX, _chartY;

  DashChangeCb _onChange;
};

// Convenience alias for the common usage pattern.
using DashCard = DashCardBase;

// ---- Notifications --------------------------------------------------------

enum class NotifyLevel : uint8_t { Info=0, Success, Warn, Error };

class JouleDashClass {
public:
  JouleDashClass();

  void begin(AsyncWebServer *server,
             const String &username = "",
             const String &password = "",
             bool allowAnonymousRead = false);

  // Register a card. The card pointer must outlive JouleDash (typical
  // pattern: static / global in the host sketch).
  void add(DashCardBase *card);

  // Force a fresh layout push (useful after add/remove at runtime).
  void refreshLayout();

  // Push pending updates to all connected clients. Call from loop(). The
  // call coalesces dirty cards into a single WS frame at most every
  // `setMinPushIntervalMs()` ms (default 100 — i.e. up to 10 Hz).
  void tick();

  // Tabs.
  void addTab(const String &name)         { _tabs.push_back(name); _layoutDirty = true; }

  // Theme / branding.
  void setTitle(const String &t)          { _title = t; _layoutDirty = true; }
  void setBrandColor(const String &css)   { _brand = css; _layoutDirty = true; }
  void setTheme(const String &t)          { _theme = t; _layoutDirty = true; } // "dark" / "light" / "auto"
  void setMinPushIntervalMs(uint32_t ms)  { _pushMs = ms; }

  // Push a notification to all clients.
  void notify(NotifyLevel lvl, const String &message, uint32_t ttlMs = 5000);

private:
  void _attachWs();
  String _layoutJson() const;
  String _updatesJson(bool full);
  bool   _auth(AsyncWebServerRequest *req, bool requireWrite=true) const;

  AsyncWebServer *_server = nullptr;
  AsyncWebSocket *_ws = nullptr;
  String _user, _pass;
  bool   _anonRead = false;
  std::vector<DashCardBase*> _cards;
  std::vector<String> _tabs;
  String _title = "JouleDash";
  String _brand = "#7c5cff";
  String _theme = "auto";
  uint32_t _pushMs = 100;
  uint32_t _lastPush = 0;
  bool _layoutDirty = true;
};

} // namespace joule

extern joule::JouleDashClass JouleDash;
