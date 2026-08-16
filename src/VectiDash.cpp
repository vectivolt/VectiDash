// ---------------------------------------------------------------------------
// VectiSuite for ESP32 — VectiOTA · VectiSerial · VectiNet · VectiDash
// Author: VectiVolt team
// (c) 2026 VectiVolt — Apache-2.0 License
// ---------------------------------------------------------------------------

// VectiDash implementation. Wire format:
//
//   client -> server:
//     {type:"hello"}                              — request initial layout + values
//     {type:"cmd", id:"<cardId>", value:"<str>"}  — interaction (slider, button…)
//                                                   ignored unless the socket
//                                                   authenticated at handshake
//
//   server -> client:
//     {type:"layout", title, brand, theme, tabs:[], cards:[ {id,type,...} ]}
//     {type:"upd",    cards:[ {id,value} ]}
//     {type:"notify", level, message, ttl}
//
// All values are strings on the wire; the UI casts as needed.
//
// Two tasks touch this file. The AsyncTCP task runs the HTTP handlers and every
// WebSocket callback; the Arduino loop() task runs tick(), the sketch's
// setValue() calls and the onChange callbacks. The AsyncTCP side only ever
// touches the small block of state guarded by _mx (see VectiDash.h) and hands
// work to loop() through it — it never reads a card and never serialises a
// frame, so card state needs no lock at all.

#include "VectiDash.h"
#include "VectiDash_ui_gz.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <memory>

namespace {
// Serve the pre-compressed UI blob with `Content-Encoding: gzip`. Browsers
// transparently inflate. Measured off the blob in VectiDash_ui_gz.h: 155,079
// bytes of HTML go out as 46,812 bytes on the wire, which is what makes it
// survive weak Wi-Fi links where the uncompressed chunked variant stalls
// partway through.
//
// `cache` is no-store once credentials are configured: the response carries the
// handshake ticket, and a shared cache holding a copy would hand both the page
// and the ticket to the next unauthenticated requester.
static void sendGzippedUi(AsyncWebServerRequest *req, const uint8_t *gz, size_t len,
                          const char *cache, const String &setCookie) {
  AsyncWebServerResponse *res = req->beginResponse(200, "text/html; charset=utf-8", gz, len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("Cache-Control", cache);
  if (setCookie.length()) res->addHeader("Set-Cookie", setCookie);
  req->send(res);
}

// Ceiling on a single inbound WS message. Enough for an Input card carrying a
// config blob; the point is that a peer cannot grow a per-client String without
// bound before the JSON ever parses.
constexpr size_t kMaxRxBytes = 4096;

// Ceiling on `cmd` frames queued for one tick(). A dashboard has one pair of
// hands on it; anything past this is a peer spamming the socket, and dropping
// the excess is correct — the queue is last-write-wins anyway.
constexpr size_t kMaxPendingCmds = 32;
}

namespace vecti {

// -------------------- DashCardBase ----------------------------------------

void DashCardBase::setValueStr(const String &s) {
  if (_val == s) return;
  _val = s; _dirty = true;
}

// Encode the series into _val as compact JSON so the standard "upd" path ships
// it. The UI parses {x:[],y:[]} directly. Reserve first: at 120 points the
// string runs to ~3 KB and every unreserved append can realloc-and-copy all of
// it.
void DashCardBase::_encodeSeries() {
  String j = "{\"x\":[";
  j.reserve(_chartX.size() * 26 + 16);
  for (size_t i=0;i<_chartX.size();i++){ if(i)j+=','; j+=String(_chartX[i],3);} j+="],\"y\":[";
  for (size_t i=0;i<_chartY.size();i++){ if(i)j+=','; j+=String(_chartY[i],3);} j+="]}";
  _val = std::move(j); _dirty = true;
}

void DashCardBase::chartPushXY(float x, float y) {
  _chartX.push_back(x); _chartY.push_back(y);
  // One range erase rather than one per dropped point, which matters when a
  // caller lowers _chartMax mid-run. Still shifts the tail — a ring buffer is
  // the upgrade if a chart ever gets long enough for that to show.
  if (_chartX.size() > _chartMax) {
    size_t drop = _chartX.size() - _chartMax;
    _chartX.erase(_chartX.begin(), _chartX.begin() + drop);
    _chartY.erase(_chartY.begin(), _chartY.begin() + drop);
  }
  _encodeSeries();
}

void DashCardBase::chartSetSeries(const float *xs, const float *ys, size_t n) {
  // Keep the newest maxPoints samples — handing over a longer series has to
  // mean the same thing as pushing them one at a time.
  if (n > _chartMax) { xs += n - _chartMax; ys += n - _chartMax; n = _chartMax; }
  _chartX.assign(xs, xs+n); _chartY.assign(ys, ys+n);
  _encodeSeries();
}

// The string here is the contract with the UI: it must equal the registry key
// in ui/shared/widgets/index.js. No `default:` case on purpose — adding a
// DashType without a name is then a compile warning, not a widget that
// silently renders as "unknown" on a customer's bench.
static const char *typeName(DashType t) {
  switch (t) {
    // readouts
    case DashType::Number:        return "number";
    case DashType::Text:          return "text";
    case DashType::Status:        return "status";
    case DashType::Badge:         return "badge";
    case DashType::Led:           return "led";
    case DashType::Temperature:   return "temperature";
    case DashType::Humidity:      return "humidity";
    case DashType::Battery:       return "battery";
    case DashType::Signal:        return "signal";
    case DashType::Uptime:        return "uptime";
    case DashType::Table:         return "table";
    case DashType::LogView:       return "logview";
    case DashType::Image:         return "image";
    case DashType::Sparkline:     return "sparkline";
    // meters
    case DashType::Gauge:         return "gauge";
    case DashType::Dial:          return "dial";
    case DashType::Donut:         return "donut";
    case DashType::Progress:      return "progress";
    case DashType::Bar:           return "bar";
    case DashType::Level:         return "level";
    case DashType::Compass:       return "compass";
    case DashType::Thermo:        return "thermo";
    // charts
    case DashType::Chart:         return "chart";
    case DashType::MultiChart:    return "multichart";
    case DashType::Histogram:     return "histogram";
    case DashType::Scatter:       return "scatter";
    case DashType::Heatmap:       return "heatmap";
    // controls
    case DashType::Button:        return "button";
    case DashType::ConfirmButton: return "confirm";
    case DashType::Momentary:     return "momentary";
    case DashType::Switch:        return "switch";
    case DashType::Slider:        return "slider";
    case DashType::RangeSlider:   return "range";
    case DashType::Stepper:       return "stepper";
    case DashType::Dropdown:      return "dropdown";
    case DashType::Radio:         return "radio";
    case DashType::Checklist:     return "checklist";
    case DashType::Input:         return "input";
    case DashType::Textarea:      return "textarea";
    case DashType::Password:      return "password";
    case DashType::Keypad:        return "keypad";
    case DashType::Joystick:      return "joystick";
    case DashType::XYPad:         return "xypad";
    case DashType::Knob:          return "knob";
    case DashType::Color:         return "color";
    case DashType::DateTime:      return "datetime";
    case DashType::QrCode:        return "qrcode";
    // layout
    case DashType::Header:        return "header";
    case DashType::Divider:       return "divider";
    case DashType::Custom:        return "custom";
  }
  return "unknown";
}
static const char *colorName(DashColor c) {
  switch(c){case DashColor::Success:return "success";case DashColor::Warning:return "warning";
    case DashColor::Danger:return "danger";case DashColor::Info:return "info";
    case DashColor::Primary:return "primary";default:return "default";}
}
static const char *chartName(ChartType t){switch(t){case ChartType::Bar:return "bar";case ChartType::Area:return "area";default:return "line";}}

void DashCardBase::describe(JsonObject &o) const {
  o["id"]    = _id;
  o["type"]  = typeName(_type);
  o["label"] = _label;
  o["unit"]  = _unit;
  o["color"] = colorName(_color);
  o["tab"]   = _tab;
  o["min"]   = _rmin;
  o["max"]   = _rmax;
  o["step"]  = _rstep;
  o["width"] = _width;
  o["hidden"]= _hidden;
  o["chartType"] = chartName(_chartType);
  if (_custom.length()) o["custom"] = _custom;
  // Only emitted when set — an empty key on every one of 30 cards is
  // pure waste on a link this constrained.
  if (_opts.length())   o["opts"]   = _opts;
  o["value"] = _val;
}

void DashCardBase::ingest(const String &payload) {
  // Push-from-UI: update the locally stored value too so a subsequent
  // broadcast reflects the user input, then hand off to the callback.
  setValueStr(payload);
  if (_onChange) _onChange(payload);
}

// -------------------- VectiDashClass --------------------------------------

VectiDashClass::VectiDashClass() {}

void VectiDashClass::add(DashCardBase *c) {
  _cards.push_back(c);
  _layoutDirty = true;
}

void VectiDashClass::refreshLayout() { _layoutDirty = true; }

String VectiDashClass::_layoutJson() const {
  JsonDocument d;
  d["type"]  = "layout";
  d["title"] = _title;
  d["brand"] = _brand;
  d["theme"] = _theme;
  auto tabs = d["tabs"].to<JsonArray>();
  for (auto &t : _tabs) tabs.add(t);
  auto cards = d["cards"].to<JsonArray>();
  for (auto *c : _cards) {
    auto o = cards.add<JsonObject>();
    c->describe(o);
  }
  String s; serializeJson(d, s); return s;
}

// loop() task only — reads every card. The frame is always a broadcast, so
// clearing the dirty flags here is safe: no client is left owed an update.
// (Nothing needs a full dump: the layout frame carries every card's value, so
// that is what a fresh client is bootstrapped with.)
String VectiDashClass::_updatesJson() {
  JsonDocument d;
  d["type"] = "upd";
  auto arr = d["cards"].to<JsonArray>();
  for (auto *c : _cards) {
    if (!c->dirty()) continue;
    auto o = arr.add<JsonObject>();
    o["id"] = c->id();
    o["value"] = c->value();
    c->clean();
  }
  String s; serializeJson(d, s); return s;
}

bool VectiDashClass::_auth(AsyncWebServerRequest *req, bool requireWrite) const {
  if (_user.length() == 0) return true;
  if (_anonRead && !requireWrite) return true;
  if (!req) return false;
  // Browsers never attach Basic credentials to a WebSocket upgrade, so the
  // ticket cookie planted by the page GET is the only identity an upgrade can
  // carry. Basic is still accepted for non-browser clients.
  if (_ticket.length() && req->hasHeader("Cookie") &&
      req->header("Cookie").indexOf("jdash=" + _ticket) >= 0) return true;
  return req->authenticate(_user.c_str(), _pass.c_str());
}

// Both of these read/write _writers and _rx. Call with _mx held.
bool VectiDashClass::_canWrite(uint32_t clientId) const {
  if (_user.length() == 0) return true;
  return std::find(_writers.begin(), _writers.end(), clientId) != _writers.end();
}

void VectiDashClass::_forget(uint32_t clientId) {
  _writers.erase(std::remove(_writers.begin(), _writers.end(), clientId), _writers.end());
  _rx.erase(clientId);
}

void VectiDashClass::begin(AsyncWebServer *server, const String &username, const String &password, bool allowAnonymousRead) {
  _server = server; _user = username; _pass = password; _anonRead = allowAnonymousRead;

  if (_user.length()) {
    // One ticket per boot, handed out with the page and presented by the
    // WebSocket upgrade. Ceiling: it never rotates and, like the Basic
    // credentials it stands in for, it is plaintext on the wire — put the
    // device behind TLS or a trusted LAN, not on the open internet.
    _ticket = String((uint32_t)random(0x7fffffff), HEX) + String((uint32_t)random(0x7fffffff), HEX);
  }

  // exact() so `/` and `/dash` don't accidentally swallow other libraries'
  // sub-paths via the 3.x default BackwardCompatible matcher.
  //
  // `/` redirects to `/dash` instead of serving the HTML directly. The same
  // handler body served on the literal "/" path consistently truncates 8-12 KB
  // into the response on this hardware (an AsyncTCP single-segment issue when
  // the path is one char). A 302 sidesteps it: the browser does the second GET
  // against `/dash`, which serves cleanly.
  _server->on(AsyncURIMatcher::exact("/"), HTTP_GET, [this](AsyncWebServerRequest *req){
    if (!_auth(req, false)) return req->requestAuthentication();
    AsyncWebServerResponse *r = req->beginResponse(302, "text/plain", "");
    r->addHeader("Location", "/dash");
    req->send(r);
  });
  // With allowAnonymousRead nothing else ever challenges, so this is the only
  // door a browser can be prompted at. It hands back the write ticket and
  // bounces to the dashboard.
  _server->on(AsyncURIMatcher::exact("/dash/login"), HTTP_GET, [this](AsyncWebServerRequest *req){
    if (!_auth(req, true)) return req->requestAuthentication();
    AsyncWebServerResponse *r = req->beginResponse(302, "text/plain", "");
    r->addHeader("Location", "/dash");
    r->addHeader("Cache-Control", "no-store");
    if (_ticket.length()) r->addHeader("Set-Cookie", "jdash=" + _ticket + "; Path=/; HttpOnly; SameSite=Strict");
    req->send(r);
  });
  _server->on(AsyncURIMatcher::exact("/dash"), HTTP_GET, [this](AsyncWebServerRequest *req){
    if (!_auth(req, false)) return req->requestAuthentication();
    bool writer = _auth(req, true);
    sendGzippedUi(req, DASH_UI_HTML_GZ, DASH_UI_HTML_GZ_LEN,
                  _ticket.length() ? "no-store" : "public, max-age=3600",
                  writer && _ticket.length() ? "jdash=" + _ticket + "; Path=/; HttpOnly; SameSite=Strict" : String());
  });

  _attachWs();
}

void VectiDashClass::_attachWs() {
  _ws = new AsyncWebSocket("/dash/ws");
  // The socket is the control channel: gating only the page would leave every
  // relay one `wscat` away. Anonymous-read installs still complete the upgrade
  // — they are turned away later, per `cmd` frame.
  _ws->handleHandshake([this](AsyncWebServerRequest *req){ return _auth(req, false); });
  // Everything in this lambda runs on the AsyncTCP task. It touches _mx-guarded
  // state and nothing else — no card is read here and no frame is built here.
  // Both would race loop(): a card's String value is reassigned by setValue()
  // on the other task, and reading it mid-realloc is a use-after-free. The work
  // is handed to tick() instead.
  _ws->onEvent([this](AsyncWebSocket *, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len){
    if (type == WS_EVT_CONNECT) {
      // arg is the upgrade request, and it is deleted the moment this returns —
      // this is the only place the ticket can be checked, so decide write
      // authority now and remember it by client id.
      bool writer = _auth((AsyncWebServerRequest*)arg, true);   // hashing; keep it off the lock
      std::lock_guard<std::mutex> lk(_mx);
      if (writer) _writers.push_back(client->id());
      // The layout frame carries each card's current value, so one broadcast
      // from the next tick() bootstraps the new client. Cheaper than a private
      // send path, and it keeps card serialisation on loop().
      _needSnapshot = true;
      return;
    }
    // Fired from the client destructor, so it covers cleanupClients() evictions
    // too — including the ones tick() triggers from loop(), which is why this
    // needs the lock. Without it both containers grow by an entry (and a
    // partial-message String) for every socket the device has ever seen.
    if (type == WS_EVT_DISCONNECT) {
      std::lock_guard<std::mutex> lk(_mx);
      _forget(client->id());
      return;
    }
    if (type != WS_EVT_DATA) return;
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    // message_opcode, not opcode: continuation frames carry 0 and would
    // otherwise look like a protocol we don't speak.
    if (info->message_opcode != WS_TEXT) return;
    // A message arrives in as many callbacks as it took TCP segments, and the
    // peer may also have split it across frames. Parsing the first chunk as a
    // whole document silently loses anything past one segment.
    String msg;
    {
      std::lock_guard<std::mutex> lk(_mx);
      String &buf = _rx[client->id()];
      if (info->num == 0 && info->index == 0) buf = "";
      if (buf.length() + len > kMaxRxBytes) { _rx.erase(client->id()); return; }
      buf.concat((const char*)data, len);
      if (!info->final || info->index + len < info->len) return;
      msg = buf;                    // own it before the map entry goes away
      _rx.erase(client->id());
    }
    JsonDocument d;
    if (deserializeJson(d, msg) != DeserializationError::Ok) return;
    const char *t = d["type"] | "";
    if (strcmp(t, "cmd") == 0) {
      // Queued, not applied: ingest() runs the host callback, which may block
      // and must not run on this task. tick() applies it. No rebroadcast
      // either — the callback may clamp or reject the value, and echoing the
      // request would pin every tab to something the firmware refused.
      std::lock_guard<std::mutex> lk(_mx);
      if (!_canWrite(client->id())) return;
      if (_pending.size() >= kMaxPendingCmds) return;
      _pending.push_back({ d["id"] | "", d["value"] | "" });
    } else if (strcmp(t, "hello") == 0) {
      std::lock_guard<std::mutex> lk(_mx);
      _needSnapshot = true;
    }
  });
  _server->addHandler(_ws);
}

void VectiDashClass::tick() {
  if (!_ws) return;
  uint32_t now = millis();
  if ((now - _lastPush) < _pushMs) return;
  _lastPush = now;

  // Take everything the AsyncTCP task left us, then drop the lock before
  // touching a card: ingest() runs the host callback, which is allowed to be
  // slow and to call back into VectiDash.
  std::vector<PendingCmd> cmds;
  {
    std::lock_guard<std::mutex> lk(_mx);
    cmds.swap(_pending);
  }
  for (auto &p : cmds)
    for (auto *c : _cards) if (c->id() == p.id) { c->ingest(p.value); break; }

  // Reaps sockets left behind by slept or reloaded tabs and enforces the client
  // cap. Without it every stale entry still gets a copy of each fan-out. The
  // eviction path can run WS_EVT_DISCONNECT synchronously on this task, which
  // is why _forget() takes the lock.
  _ws->cleanupClients();
  if (_ws->count() == 0) return;

  // A client whose send queue is full has its frame dropped silently, and
  // serializing has already cleared the dirty flags, so there is nothing left
  // to retransmit. Sit this tick out and let the changes coalesce instead.
  //
  // Ceiling worth knowing: availableForWriteAll() is all-or-nothing, so one
  // backgrounded phone with a full queue pauses updates for every viewer until
  // cleanupClients() reaps it. That is the price of never dropping a delta.
  if (!_ws->availableForWriteAll()) return;

  bool snapshot;
  {
    std::lock_guard<std::mutex> lk(_mx);
    snapshot = _needSnapshot; _needSnapshot = false;
  }
  if (_layoutDirty || snapshot) {
    _ws->textAll(_layoutJson());
    _layoutDirty = false;
    // The layout frame is the big one (~1.5 KB with a dozen cards) and it may
    // have taken the last free slot in someone's queue. Re-check before the
    // value frame, or _updatesJson() clears the dirty flags for an update that
    // gets dropped on the floor. Every card's value rode along in the layout
    // anyway, so a new client is already current.
    if (!_ws->availableForWriteAll()) return;
  }
  bool anyDirty=false; for (auto *c:_cards) if (c->dirty()) { anyDirty=true; break; }
  if (!anyDirty) return;
  _ws->textAll(_updatesJson());
}

void VectiDashClass::notify(NotifyLevel lvl, const String &message, uint32_t ttlMs) {
  if (!_ws) return;
  const char *lvls[] = {"info","success","warn","error"};
  uint8_t li = (uint8_t)lvl;
  JsonDocument d;
  d["type"]="notify"; d["level"]=lvls[li < 4 ? li : 0];
  d["message"]=message; d["ttl"]=ttlMs;
  String s; serializeJson(d, s);
  _ws->textAll(s);
}

} // namespace vecti

vecti::VectiDashClass VectiDash;
