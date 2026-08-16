// ---------------------------------------------------------------------------
// VectiSuite for ESP32 — VectiOTA · VectiSerial · VectiNet · VectiDash
// Author: Chinmoy Bhuyan
// Email:  chinmoy@joulepoint.com
// (c) 2026 VectiVolt — Apache-2.0 License
// ---------------------------------------------------------------------------

// VectiDash — real-time IoT dashboard, all widgets free, all source Apache-2.0.
//
// Why this exists: ESP-DASH paywalls every interesting widget behind Pro,
// runs on Server-Sent Events (broken on Safari over long sessions), and
// gives you no escape hatch for a custom UI snippet. VectiDash includes
// every widget in the box, transports over a single WebSocket, and adds:
//
//   * 50 widgets — the whole DashType enum below renders in the bundled UI:
//     readouts, meters, charts, controls, layout, plus CUSTOM (raw HTML /
//     DOM ID the host sketch updates).
//   * Tabs — multi-page dashboards, drag-reorder cards.
//   * Themes: dark / light / auto, with brand-color override.
//   * Notifications — push toast or persistent banners from the firmware.
//   * Auth (Basic) and an optional anonymous-read-only mode.
//   * Multi-client sync — every interaction is rebroadcast so all open tabs
//     stay consistent.
//
// Usage:
//
//     #include <VectiDash.h>
//     vecti::DashCard temp(vecti::DashType::Number, "temp", "Temperature","°C", 0, 50);
//     void setup(){ VectiDash.add(&temp); VectiDash.begin(&server,"admin","vecti"); }
//     void loop(){ temp.setValue(readTemp()); VectiDash.tick(); }
#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace vecti {

// Widget kinds. Enumerator VALUES are part of nothing persisted — the wire
// format sends the lowercase name from typeName() — but keep appending rather
// than reordering so a sketch built against an older header still means what
// it said.
//
// Every widget here is free. The comparable commercial suites gate tabs, the
// joystick and the input cards behind a paid tier; there is no such split in
// this library and there is not going to be one.
enum class DashType : uint8_t {
  // --- readouts ---
  Number=0, Text, Status, Badge, Led, Temperature, Humidity,
  Battery, Signal, Uptime, Table, LogView, Image, Sparkline,
  // --- meters ---
  Gauge, Dial, Donut, Progress, Bar, Level, Compass, Thermo,
  // --- charts ---
  Chart, MultiChart, Histogram, Scatter, Heatmap,
  // --- controls ---
  Button, ConfirmButton, Momentary, Switch, Slider, RangeSlider,
  Stepper, Dropdown, Radio, Checklist, Input, Textarea, Password,
  Keypad, Joystick, XYPad, Knob, Color, DateTime, QrCode,
  // --- layout / escape hatch ---
  Header, Divider, Custom
};

enum class DashColor : uint8_t {
  Default=0, Success, Warning, Danger, Info, Primary
};

enum class ChartType : uint8_t { Line=0, Bar, Area };

class DashCardBase;

using DashChangeCb = std::function<void(const String &payload)>;

// THREADING: every member of a card is touched from the loop() task only —
// the sketch's setValue()/chartPushXY() calls, and ingest() which tick() runs
// for queued `cmd` frames. The AsyncTCP task never reads or writes a card, so
// none of this needs a lock. Keep it that way: serialising a card from a
// WebSocket callback would race the String reassignment in setValueStr().
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

  // The callback runs on the loop() task, from inside tick(): inbound `cmd`
  // frames are queued by the AsyncTCP task and applied here. That keeps every
  // card field single-task (see the class comment) and means a slow callback
  // delays your own loop rather than stalling the whole server — but it still
  // delays it, so no delay() longer than your push interval.
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
  // x is transmitted alongside y and the bundled renderer plots against it, so
  // an irregular sample cadence draws with the right spacing — no need to push
  // at a fixed interval. chartSetSeries keeps the newest maxPoints samples.
  void chartPushXY(float x, float y);   // appends a point; rolls at maxPoints
  void chartSetSeries(const float *xs, const float *ys, size_t n);
  void chartSetMaxPoints(size_t n) { _chartMax = n; }
  // Carried in the layout frame for custom front-ends. The bundled UI draws the
  // same gradient area+line for all three.
  void chartSetType(ChartType t)   { _chartType = t; _dirty = true; }

  // ---- custom widget escape hatch ----
  // Set an HTML snippet to render inside the card body. The snippet may
  // include a <span id="dash-<id>-out"></span> the firmware updates via
  // setValue() — the runtime injects the value into that span on every
  // broadcast. Anything beyond that span is inert: the snippet is injected as
  // markup, so a <script> in it never executes.
  void setCustomHtml(const String &html) { _custom = html; _dirty = true; }

  // ---- choices (Dropdown / Radio / Checklist) ----
  // Pipe-separated, matching VectiNet's parameter convention so the two
  // libraries don't disagree about how a list of choices is spelled:
  //     mode.setOptions("Eco|Standard|Boost");
  // A Checklist reports its selection back the same way ("Eco|Boost").
  void setOptions(const String &pipeSeparated) { _opts = pipeSeparated; _dirty = true; }
  String options() const { return _opts; }

  // ---- meta ----
  DashType type() const  { return _type; }
  String   id()   const  { return _id; }
  bool     dirty() const { return _dirty; }
  void     clean()       { _dirty = false; }

  void describe(class JsonObject &o) const;
  String value() const { return _val; }
  void   ingest(const String &payload);

private:
  void _encodeSeries();

  DashType  _type;
  String    _id, _label, _unit, _tab, _val, _custom, _opts;
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

class VectiDashClass {
public:
  VectiDashClass();

  // Routes: `/` (302), `/dash`, `/dash/login`, `/dash/ws`.
  //
  // With credentials set, `/`, `/dash` and the `/dash/ws` upgrade all require
  // them. allowAnonymousRead drops the requirement for the page and for the
  // read half of the socket: such a client still receives layout and value
  // frames, but its `cmd` frames are ignored until it logs in via
  // `/dash/login`, which is the only route that challenges in that mode.
  void begin(AsyncWebServer *server,
             const String &username = "",
             const String &password = "",
             bool allowAnonymousRead = false);

  // Register a card. The card pointer must outlive VectiDash (typical
  // pattern: static / global in the host sketch).
  void add(DashCardBase *card);

  // Force a fresh layout push (useful after add/remove at runtime).
  void refreshLayout();

  // Push pending updates to all connected clients, and apply the `cmd` frames
  // the AsyncTCP task queued since the last call (this is where onChange
  // callbacks run). Call from loop(). Both halves are rate-limited to one pass
  // per `setMinPushIntervalMs()` ms (default 100 — i.e. up to 10 Hz), so an
  // interaction lands within one interval of the user's tap.
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
  String _updatesJson();
  bool   _auth(AsyncWebServerRequest *req, bool requireWrite=true) const;
  bool   _canWrite(uint32_t clientId) const;
  void   _forget(uint32_t clientId);

  struct PendingCmd { String id, value; };

  AsyncWebServer *_server = nullptr;
  AsyncWebSocket *_ws = nullptr;
  // --- set in begin(), read-only afterwards from both tasks ---
  String _user, _pass;
  bool   _anonRead = false;
  String _ticket;                        // handshake ticket; see begin()

  // --- shared: written by the AsyncTCP task, drained by loop() in tick().
  // Everything below _mx is touched from both tasks and must be accessed with
  // _mx held. Nothing that blocks (client->text(), req->send(), a host
  // callback) may run while it is held. ---
  mutable std::mutex _mx;
  std::vector<uint32_t> _writers;        // sockets that proved credentials
  std::map<uint32_t, String> _rx;        // partial inbound messages, by client
  std::vector<PendingCmd> _pending;      // inbound cmds awaiting tick()
  bool _needSnapshot = false;            // a client connected / said hello

  // --- loop() task only ---
  std::vector<DashCardBase*> _cards;
  std::vector<String> _tabs;
  String _title = "VectiDash";
  String _brand = "#7c5cff";
  String _theme = "auto";
  uint32_t _pushMs = 100;
  uint32_t _lastPush = 0;
  bool _layoutDirty = true;
};

} // namespace vecti

extern vecti::VectiDashClass VectiDash;
