// ---------------------------------------------------------------------------
// JouleSuite for ESP32 / ESP8266 — JouleOTA · JouleSerial · JouleNet · JouleDash
// Author: Chinmoy Bhuyan
// Email:  dikibhuyan@gmail.com
// (c) 2026 — MIT License
// ---------------------------------------------------------------------------

// JouleDash implementation. Wire format:
//
//   client -> server:
//     {type:"hello"}                              — request initial layout + values
//     {type:"cmd", id:"<cardId>", value:"<str>"}  — interaction (slider, button…)
//
//   server -> client:
//     {type:"layout", title, brand, theme, tabs:[], cards:[ {id,type,...} ]}
//     {type:"upd",    cards:[ {id,value} ]}
//     {type:"notify", level, message, ttl}
//
// All values are strings on the wire; the UI casts as needed.

#include "JouleDash.h"
#include "JouleDash_ui.h"
#include "JouleDash_ui_gz.h"
#include <ArduinoJson.h>
#include <memory>

namespace {
// Serve the pre-compressed UI blob with `Content-Encoding: gzip`. Browsers
// transparently inflate. Cuts the ~14 KB SPA to ~5 KB on the wire, which
// is what makes it survive weak Wi-Fi links where the uncompressed
// chunked variant stalls partway through.
static void sendGzippedUi(AsyncWebServerRequest *req, const uint8_t *gz, size_t len) {
  AsyncWebServerResponse *res = req->beginResponse(200, "text/html; charset=utf-8", gz, len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("Cache-Control", "public, max-age=3600");
  req->send(res);
}
}

namespace joule {

// -------------------- DashCardBase ----------------------------------------

void DashCardBase::setValueStr(const String &s) {
  if (_val == s) return;
  _val = s; _dirty = true;
}

void DashCardBase::chartPushXY(float x, float y) {
  _chartX.push_back(x); _chartY.push_back(y);
  while (_chartX.size() > _chartMax) { _chartX.erase(_chartX.begin()); _chartY.erase(_chartY.begin()); }
  // Encode the series into _val as compact JSON so the standard "upd" path
  // ships it. The UI parses {x:[],y:[]} directly.
  String j = "{\"x\":["; for (size_t i=0;i<_chartX.size();i++){ if(i)j+=','; j+=String(_chartX[i],3);} j+="],\"y\":[";
  for (size_t i=0;i<_chartY.size();i++){ if(i)j+=','; j+=String(_chartY[i],3);} j+="]}";
  _val = j; _dirty = true;
}

void DashCardBase::chartSetSeries(const float *xs, const float *ys, size_t n) {
  _chartX.assign(xs, xs+n); _chartY.assign(ys, ys+n);
  chartPushXY(0,0); // shortcut: re-encode without adding
  _chartX.pop_back(); _chartY.pop_back();
}

static const char *typeName(DashType t) {
  switch (t) {
    case DashType::Number:      return "number";
    case DashType::Button:      return "button";
    case DashType::Switch:      return "switch";
    case DashType::Slider:      return "slider";
    case DashType::Gauge:       return "gauge";
    case DashType::Progress:    return "progress";
    case DashType::Status:      return "status";
    case DashType::Temperature: return "temperature";
    case DashType::Humidity:    return "humidity";
    case DashType::Image:       return "image";
    case DashType::Input:       return "input";
    case DashType::Joystick:    return "joystick";
    case DashType::Color:       return "color";
    case DashType::Chart:       return "chart";
    case DashType::Custom:      return "custom";
    case DashType::Donut:       return "donut";
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
  o["value"] = _val;
}

void DashCardBase::ingest(const String &payload) {
  // Push-from-UI: update the locally stored value too so a subsequent
  // broadcast reflects the user input, then hand off to the callback.
  setValueStr(payload);
  if (_onChange) _onChange(payload);
}

// -------------------- JouleDashClass --------------------------------------

JouleDashClass::JouleDashClass() {}

void JouleDashClass::add(DashCardBase *c) {
  _cards.push_back(c);
  _layoutDirty = true;
}

void JouleDashClass::refreshLayout() { _layoutDirty = true; }

String JouleDashClass::_layoutJson() const {
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

String JouleDashClass::_updatesJson(bool full) {
  JsonDocument d;
  d["type"] = "upd";
  auto arr = d["cards"].to<JsonArray>();
  for (auto *c : _cards) {
    if (!full && !c->dirty()) continue;
    auto o = arr.add<JsonObject>();
    o["id"] = c->id();
    o["value"] = c->value();
    c->clean();
  }
  String s; serializeJson(d, s); return s;
}

static String renderHtml(const String &title, const String &brand) {
  String s = FPSTR(DASH_UI_HTML);
  s.replace("__TITLE__", title);
  s.replace("__BRAND__", brand);
  return s;
}

bool JouleDashClass::_auth(AsyncWebServerRequest *req, bool requireWrite) const {
  if (_user.length() == 0) return true;
  if (_anonRead && !requireWrite) return true;
  return req->authenticate(_user.c_str(), _pass.c_str());
}

void JouleDashClass::begin(AsyncWebServer *server, const String &username, const String &password, bool allowAnonymousRead) {
  _server = server; _user = username; _pass = password; _anonRead = allowAnonymousRead;

  // exact() so `/` and `/dash` don't accidentally swallow other libraries'
  // sub-paths via the 3.x default BackwardCompatible matcher.
  //
  // `/` redirects to `/dash` instead of serving the HTML directly. We hit
  // an AsyncTCP quirk where a ~15KB string response on the literal "/"
  // path consistently truncated around 5.6KB ("IncompleteRead" on the
  // client). The same handler body served via `/dash` completes cleanly,
  // so this is a path-specific TCP-send issue, not a route or handler bug.
  // A 302 sidesteps it entirely — first request gets a tiny redirect, the
  // browser follows to `/dash` and receives the full page from the working
  // path.
  // `/` redirects to `/dash`. The same handler body served on the literal
  // "/" path consistently truncates 8-12 KB into a 22 KB chunked response
  // on this hardware (AsyncTCP single-segment issue when the path is one
  // char). A 302 sidesteps it: the browser does the second GET against
  // `/dash` which serves cleanly.
  _server->on(AsyncURIMatcher::exact("/"), HTTP_GET, [this](AsyncWebServerRequest *req){
    if (!_auth(req, false)) return req->requestAuthentication();
    AsyncWebServerResponse *r = req->beginResponse(302, "text/plain", "");
    r->addHeader("Location", "/dash");
    req->send(r);
  });
  _server->on(AsyncURIMatcher::exact("/dash"), HTTP_GET, [this](AsyncWebServerRequest *req){
    if (!_auth(req, false)) return req->requestAuthentication();
    sendGzippedUi(req, DASH_UI_HTML_GZ, DASH_UI_HTML_GZ_LEN);
  });

  _attachWs();
}

void JouleDashClass::_attachWs() {
  _ws = new AsyncWebSocket("/dash/ws");
  _ws->onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                       AwsEventType type, void *arg, uint8_t *data, size_t len){
    if (type == WS_EVT_CONNECT) {
      client->text(_layoutJson());
      client->text(_updatesJson(true));
      return;
    }
    if (type != WS_EVT_DATA) return;
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (!info->final || info->opcode != WS_TEXT) return;
    String s; s.reserve(len); for (size_t i=0;i<len;i++) s += (char)data[i];
    JsonDocument d;
    if (deserializeJson(d, s) != DeserializationError::Ok) return;
    const char *t = d["type"] | "";
    if (strcmp(t, "cmd") == 0) {
      String id  = d["id"]    | "";
      String val = d["value"] | "";
      for (auto *c : _cards) if (c->id() == id) {
        c->ingest(val);
        // rebroadcast so all other tabs see the same value immediately.
        JsonDocument up; up["type"]="upd"; auto arr = up["cards"].to<JsonArray>();
        auto o = arr.add<JsonObject>(); o["id"]=id; o["value"]=val;
        String packet; serializeJson(up, packet);
        server->textAll(packet);
        c->clean();
        break;
      }
    } else if (strcmp(t, "hello") == 0) {
      client->text(_layoutJson());
      client->text(_updatesJson(true));
    }
  });
  _server->addHandler(_ws);
}

void JouleDashClass::tick() {
  if (!_ws) return;
  uint32_t now = millis();
  if ((now - _lastPush) < _pushMs) return;
  _lastPush = now;
  if (_layoutDirty) {
    _ws->textAll(_layoutJson());
    _layoutDirty = false;
  }
  if (_ws->count() == 0) return;
  bool anyDirty=false; for (auto *c:_cards) if (c->dirty()) { anyDirty=true; break; }
  if (!anyDirty) return;
  _ws->textAll(_updatesJson(false));
}

void JouleDashClass::notify(NotifyLevel lvl, const String &message, uint32_t ttlMs) {
  if (!_ws) return;
  const char *lvls[] = {"info","success","warn","error"};
  JsonDocument d;
  d["type"]="notify"; d["level"]=lvls[(int)lvl];
  d["message"]=message; d["ttl"]=ttlMs;
  String s; serializeJson(d, s);
  _ws->textAll(s);
}

} // namespace joule

joule::JouleDashClass JouleDash;
