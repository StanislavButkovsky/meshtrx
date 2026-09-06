#include "wifi_monitor.h"
#include "repeater.h"
#include "lora_radio.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

static WebServer server(80);
static uint32_t startTime = 0;

// Текущий канал — доступ из main.cpp
extern uint8_t currentChannel;

static void handleRoot() {
  RepeaterStats stats = repeaterGetStats();
  uint32_t uptime = (millis() - startTime) / 1000;
  uint32_t hours = uptime / 3600;
  uint32_t mins = (uptime % 3600) / 60;
  uint32_t secs = uptime % 60;
  uint8_t ch = loraGetChannel();
  float freq = loraGetFrequency(ch);
  int8_t txPower = loraGetTxPower();

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>MeshTRX Repeater</title>";
  html += "<style>"
          "body{font-family:monospace;background:#141414;color:#e8e8e8;padding:16px;}"
          "h1{color:#4ade80;font-size:20px;}"
          "table{border-collapse:collapse;width:100%;}"
          "td{padding:4px 8px;border-bottom:1px solid #333;}"
          "td:first-child{color:#888;}"
          ".val{color:#4ade80;font-weight:bold;}"
          "select,button{font-family:monospace;font-size:14px;padding:6px 12px;"
          "background:#222;color:#e8e8e8;border:1px solid #444;border-radius:4px;}"
          "button{background:#1e3a1e;color:#4ade80;cursor:pointer;margin-left:8px;}"
          "button:hover{background:#2a4a2a;}"
          ".ch-form{margin:12px 0;display:flex;align-items:center;}"
          "</style></head><body>";
  html += "<h1>MeshTRX Repeater</h1>";

  // Форма выбора канала
  html += "<p><a href='/map' style='color:#4ade80'>Кого слышит ретранслятор →</a></p>";
  html += "<div class='ch-form'><form method='GET' action='/channel'>";
  html += "<select name='ch'>";
  for (int i = 0; i < 23; i++) {
    float f = 863.15f + i * 0.3f;
    html += "<option value='" + String(i) + "'";
    if (i == ch) html += " selected";
    html += ">CH " + String(i) + " &mdash; " + String(f, 2) + " MHz</option>";
  }
  html += "</select>";
  html += "<button type='submit'>Set</button></form></div>";

  html += "<table>";
  html += "<tr><td>Uptime</td><td class='val'>" + String(hours) + "h " + String(mins) + "m " + String(secs) + "s</td></tr>";
  html += "<tr><td>Channel</td><td class='val'>CH " + String(ch) + " &mdash; " + String(freq, 2) + " MHz</td></tr>";
  html += "<tr><td>TX Power</td><td class='val'>" + String(txPower) + " dBm</td></tr>";
  html += "<tr><td>Forwarded</td><td class='val'>" + String(stats.fwd_count) + "</td></tr>";
  html += "<tr><td>Dropped</td><td class='val'>" + String(stats.drop_count) + "</td></tr>";
  html += "<tr><td>Audio</td><td>" + String(stats.audio_fwd) + "</td></tr>";
  html += "<tr><td>Text</td><td>" + String(stats.text_fwd) + "</td></tr>";
  html += "<tr><td>File</td><td>" + String(stats.file_fwd) + "</td></tr>";
  html += "<tr><td>Beacon</td><td>" + String(stats.beacon_fwd) + "</td></tr>";

  if (stats.fwd_count > 0) {
    html += "<tr><td>RSSI range</td><td>" + String(stats.min_rssi) + " .. " + String(stats.max_rssi) + " dBm</td></tr>";
  }

  // IP адрес
  String ip = WiFi.getMode() == WIFI_STA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  html += "<tr><td>IP</td><td class='val'>" + ip + "</td></tr>";
  html += "</table>";
  html += "<p style='color:#555;font-size:11px;margin-top:16px;'>Auto-refresh 5s</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

static void handleSetChannel() {
  if (server.hasArg("ch")) {
    int ch = server.arg("ch").toInt();
    if (ch >= 0 && ch < 23) {
      currentChannel = ch;
      loraSetChannel(ch);
      loraStartReceive();
      // Сохранить в NVS
      Preferences prefs;
      prefs.begin("settings", false);
      prefs.putUChar("channel", ch);
      prefs.end();
      Serial.printf("[WiFi] Channel set to %d (%.2f MHz)\n", ch, loraGetFrequency(ch));
    }
  }
  // Redirect обратно на главную
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Redirecting...");
}


// Карта станций. Тайлы тянет браузер того, кто смотрит страницу, — сам ESP32
// карту хранить не может, да и не должен. Если интернета у смотрящего нет
// (например, он подключился к точке доступа самого ретранслятора), карта не
// загрузится, поэтому рядом всегда есть радар: он рисуется на месте и
// показывает то же самое — кто где относительно ретранслятора.
static void handleMap() {
  String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>MeshTRX — станции</title>"
    "<style>"
    "body{background:#111;color:#eee;font-family:system-ui,sans-serif;margin:0;padding:12px}"
    "h1{font-size:18px;margin:0 0 10px}"
    "a{color:#4ade80}"
    ".tabs{display:flex;gap:8px;margin-bottom:10px}"
    ".tabs button{background:#222;color:#eee;border:1px solid #444;border-radius:6px;"
    "padding:6px 14px;font-size:14px}"
    ".tabs button.on{background:#4ade80;color:#111;border-color:#4ade80}"
    "#map{height:60vh;border-radius:8px}"
    "#radar{background:#000;border-radius:8px;width:100%;max-width:520px;display:block}"
    "table{width:100%;border-collapse:collapse;margin-top:12px;font-size:13px}"
    "th,td{padding:5px 6px;text-align:left;border-bottom:1px solid #333}"
    "th{color:#888;font-weight:normal}"
    ".old{opacity:.45}"
    ".muted{color:#888;font-size:13px;margin:6px 0}"
    ".netbox{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:12px;margin-top:8px}"
    ".netbox select,.netbox input{background:#222;color:#eee;border:1px solid #444;"
    "border-radius:6px;padding:6px;font-size:14px;margin:4px 6px 4px 0;min-width:180px}"
    ".netbox button{background:#222;color:#eee;border:1px solid #444;border-radius:6px;"
    "padding:6px 12px;font-size:14px;margin:4px 6px 4px 0}"
    ".netbox button:disabled{opacity:.4}"
    ".ok{color:#4ade80}.bad{color:#f87171}"
    "</style></head><body>"
    "<h1>MeshTRX — кого слышит ретранслятор</h1>"
    "<div class='tabs'><button id='bR' class='on'>Радар</button>"
    "<button id='bM'>Карта</button>"
    "<button id='bFit' style='display:none'>Показать всех</button>"
    "<a href='/' style='margin-left:auto;align-self:center'>назад</a></div>"
    "<canvas id='radar' width='520' height='520'></canvas>"
    "<div id='map' style='display:none'></div>"
    "<table id='t'><thead><tr><th>Позывной</th><th>Сигнал</th><th>Батарея</th>"
    "<th>Слышали</th></tr></thead><tbody></tbody></table>"
    "<h2 style='font-size:16px;margin:18px 0 8px'>Сеть</h2>"
    "<div id='netstate' class='muted'>проверяем…</div>"
    "<div class='netbox'>"
    "<select id='ssid'><option value=\"\">— нажмите «Обновить список» —</option></select>"
    "<button id='bScan'>Обновить список</button><br>"
    "<input id='pass' type='password' placeholder='пароль сети'>"
    "<button id='bTest'>Проверить</button>"
    "<button id='bSave' disabled>Подключить</button>"
    "<button id='bForget'>Забыть сеть</button>"
    "<div id='netmsg' class='muted'></div></div>"
    "<script>");
  html += F(
    "let map,layer,nodes=[];"
    "const R=document.getElementById('radar'),C=R.getContext('2d');"
    "function age(s){return s<60?s+' с назад':(s<3600?Math.round(s/60)+' мин назад':Math.round(s/3600)+' ч назад')}"
    // расстояние по прямой между двумя точками, метры
    "function dist(a,b,c,d){const R2=6371000,p=Math.PI/180;"
    "const x=(c-a)*p,y=(d-b)*p,h=Math.sin(x/2)**2+Math.cos(a*p)*Math.cos(c*p)*Math.sin(y/2)**2;"
    "return 2*R2*Math.asin(Math.sqrt(h))}"
    "function bearing(a,b,c,d){const p=Math.PI/180;const y=Math.sin((d-b)*p)*Math.cos(c*p);"
    "const x=Math.cos(a*p)*Math.sin(c*p)-Math.sin(a*p)*Math.cos(c*p)*Math.cos((d-b)*p);"
    "return (Math.atan2(y,x)*180/Math.PI+360)%360}"
    "function drawRadar(){"
    "const w=R.width,h=R.height,cx=w/2,cy=h/2;C.clearRect(0,0,w,h);"
    "const gps=nodes.filter(n=>n.gps);"
    // центр — первая станция с координатами: у самого ретранслятора GPS нет
    "const c0=gps[0];"
    "let max=1000;"
    "if(c0)gps.forEach(n=>{const d=dist(c0.lat,c0.lon,n.lat,n.lon);if(d>max)max=d});"
    "C.strokeStyle='#1b5e20';C.fillStyle='#4ade80';C.font='12px system-ui';"
    "for(let i=1;i<=4;i++){C.beginPath();C.arc(cx,cy,i*cy/4.4,0,7);C.stroke();"
    "C.fillStyle='#2e7d32';C.fillText(Math.round(max*i/4)+' м',cx+4,cy-i*cy/4.4+12)}"
    "if(!c0){C.fillStyle='#888';C.fillText('ни одна станция не передала координаты',20,cy);return}"
    "gps.forEach(n=>{const d=dist(c0.lat,c0.lon,n.lat,n.lon),b=bearing(c0.lat,c0.lon,n.lat,n.lon);"
    "const r=(d/max)*(cy/1.1),a=(b-90)*Math.PI/180;"
    "const x=cx+r*Math.cos(a),y=cy+r*Math.sin(a);"
    "C.fillStyle=n.age>600?'#33691e':'#4ade80';C.beginPath();C.arc(x,y,6,0,7);C.fill();"
    "C.fillStyle='#eee';C.fillText(n.cs+' '+Math.round(d)+' м',x+9,y+4)})}"
    // Карта обновляется каждые пять секунд, и раньше при каждом обновлении
    // подгонялась под все станции — то есть отменяла то, что человек только
    // что сделал руками: приблизил, сдвинул, посмотрел. Теперь подгоняем один
    // раз, при первом показе, и больше вид не трогаем. Кнопка «Показать всех»
    // возвращает общий охват, когда он снова нужен.
    "let userMoved=false;"
    "function drawMap(){"
    "const gps=nodes.filter(n=>n.gps);if(!gps.length)return;"
    "if(!map){map=L.map('map').setView([gps[0].lat,gps[0].lon],13);"
    "L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',"
    "{maxZoom:19,attribution:'OpenStreetMap'}).addTo(map);"
    "map.on('movestart zoomstart',e=>{if(e.hard!==true)userMoved=true});"
    "fitAll()}"
    "if(layer)layer.remove();layer=L.layerGroup().addTo(map);"
    "gps.forEach(n=>L.marker([n.lat,n.lon]).addTo(layer)"
    ".bindPopup(n.cs+'<br>'+n.rssi+' dBm, '+age(n.age)))}"
    "function fitAll(){const gps=nodes.filter(n=>n.gps);if(!map||!gps.length)return;"
    "userMoved=false;"
    "map.fitBounds(gps.map(n=>[n.lat,n.lon]),{maxZoom:15,padding:[30,30]})}"
    "function fill(){"
    "const tb=document.querySelector('#t tbody');tb.innerHTML='';"
    "nodes.forEach(n=>{const tr=document.createElement('tr');if(n.age>600)tr.className='old';"
    "const bat=n.bat===255?'USB':(n.bat+'%');"
    "tr.innerHTML='<td>'+n.cs+'</td><td>'+n.rssi+' dBm / '+n.snr+'</td><td>'+bat+"
    "'</td><td>'+age(n.age)+'</td>';tb.appendChild(tr)})}"
    "async function tick(){try{const r=await fetch('/api/nodes');nodes=(await r.json()).nodes;"
    "fill();drawRadar();if(map)drawMap()}catch(e){}}"
    "document.getElementById('bR').onclick=()=>{R.style.display='block';"
    "document.getElementById('map').style.display='none';"
    "bR.classList.add('on');bM.classList.remove('on');"
    "document.getElementById('bFit').style.display='none';drawRadar()};"
    // Leaflet тянется из интернета, а его у ретранслятора может не быть —
    // в режиме точки доступа его нет наверняка. Поэтому грузим карту только
    // когда её попросили, и не держим из-за неё всю страницу: радар и список
    // должны появляться сразу.
    "let leafletLoading=false;"
    "function loadLeaflet(cb){if(window.L)return cb();if(leafletLoading)return;leafletLoading=true;"
    "const css=document.createElement('link');css.rel='stylesheet';"
    "css.href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';document.head.appendChild(css);"
    "const js=document.createElement('script');js.src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';"
    "js.onload=()=>{leafletLoading=false;cb()};"
    "js.onerror=()=>{leafletLoading=false;"
    "document.getElementById('map').innerHTML='<div style=\"padding:20px;color:#888\">"
    "Карта не загрузилась: у ретранслятора нет доступа в интернет. "
    "Подключите его к домашней сети ниже — или пользуйтесь радаром, он работает всегда.</div>'};"
    "document.head.appendChild(js);"
    "setTimeout(()=>{if(!window.L&&leafletLoading)js.onerror()},8000)}"
    "document.getElementById('bM').onclick=()=>{R.style.display='none';"
    "document.getElementById('map').style.display='block';"
    "bM.classList.add('on');bR.classList.remove('on');"
    "document.getElementById('bFit').style.display='inline-block';"
    "loadLeaflet(()=>setTimeout(()=>{drawMap();if(map)map.invalidateSize()},50))};"
    "document.getElementById('bFit').onclick=fitAll;"
    "tick();setInterval(tick,5000);"
    // Пароль сначала проверяем и только потом сохраняем: ретранслятор может
    // стоять на мачте, и опечатка оставила бы его без сети совсем.
    "const $=id=>document.getElementById(id);"
    "async function netstate(){const r=await fetch('/api/wifi/state');const s=await r.json();"
    "$('netstate').innerHTML=(s.connected?'<span class=ok>в сети '+s.saved+'</span>, адрес '+s.ip"
    "+' · точка доступа '+s.ap:(s.saved?'<span class=bad>сеть '+s.saved+' недоступна</span> ('+s.status+'), ':'')"
    "+'работает точкой доступа '+s.ap)}"
    "async function scan(){$('netmsg').textContent='ищем сети…';"
    "const r=await fetch('/api/scan');const d=await r.json();"
    "const sel=$('ssid');sel.innerHTML='<option value=\"\">— выберите сеть —</option>';"
    "d.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{const o=document.createElement('option');"
    "o.value=n.ssid;o.textContent=n.ssid+'  '+n.rssi+' dBm'+(n.lock?' 🔒':'');sel.appendChild(o)});"
    "$('netmsg').textContent='найдено сетей: '+d.networks.length}"
    "$('bScan').onclick=scan;"
    "$('bTest').onclick=async()=>{const q='ssid='+encodeURIComponent($('ssid').value)+"
    "'&pass='+encodeURIComponent($('pass').value);"
    "$('netmsg').textContent='проверяем подключение, это до 15 секунд…';$('bSave').disabled=true;"
    "const r=await fetch('/api/wifi/test?'+q);const d=await r.json();"
    "$('netmsg').innerHTML=d.ok?'<span class=ok>подключение удалось, адрес '+d.ip+"
    "'. Теперь можно нажать «Подключить»</span>':'<span class=bad>не вышло: '+d.why+'</span>';"
    "$('bSave').disabled=!d.ok};"
    "$('bSave').onclick=async()=>{const q='ssid='+encodeURIComponent($('ssid').value)+"
    "'&pass='+encodeURIComponent($('pass').value);"
    "$('netmsg').textContent='подключаемся…';"
    "const r=await fetch('/api/wifi/save?'+q);const d=await r.json();"
    "$('netmsg').innerHTML=d.ok?'<span class=ok>готово: '+d.ip+'. Точка доступа продолжает работать</span>':"
    "'<span class=bad>не вышло: '+d.why+'</span>';netstate()};"
    "$('bForget').onclick=async()=>{await fetch('/api/wifi/forget');"
    "$('netmsg').textContent='сеть забыта, работаем точкой доступа';netstate()};"
    // Сканирование эфира занимает несколько секунд, и всё это время веб-сервер
    // на ESP32 не отвечает — при загрузке страницы это выглядело так, будто
    // не работает ничего: ни радар, ни список станций. Поэтому ищем сети
    // только по кнопке.
    "netstate();setInterval(netstate,10000);"
    "</script></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

// === Подключение к домашней сети ===
//
// Точка доступа самого ретранслятора удобна тем, что есть всегда, но интернета
// за ней нет — и карта станций на странице остаётся пустой: тайлы браузеру
// взять неоткуда. Поэтому даём подключиться к обычной сети прямо со страницы.
//
// Два правила, которые здесь важнее удобства. Первое: пароль сначала
// проверяется, и только потом сохраняется — иначе опечатка оставила бы
// ретранслятор без связи вовсе, а он может стоять на мачте. Второе: своя точка
// доступа никуда не девается, работаем в режиме «и точка, и клиент». Если
// домашняя сеть пропадёт, попасть на страницу всё равно можно.

static String staStatusText() {
  switch (WiFi.status()) {
    case WL_CONNECTED:       return "подключено";
    case WL_NO_SSID_AVAIL:   return "сеть не найдена";
    case WL_CONNECT_FAILED:  return "не подошёл пароль";
    case WL_IDLE_STATUS:     return "ожидание";
    case WL_DISCONNECTED:    return "отключено";
    default:                 return "нет связи";
  }
}

// Подключиться и подождать результата. Возвращает true, если получилось.
static bool staTry(const String& ssid, const String& pass, uint32_t waitMs = 12000) {
  WiFi.mode(WIFI_AP_STA);              // точку доступа не гасим ни на секунду
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  while (millis() - start < waitMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    if (WiFi.status() == WL_CONNECT_FAILED) return false;
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

static void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "{\"networks\":[";
  for (int i = 0; i < n && i < 24; i++) {
    if (i) json += ",";
    String ssid = WiFi.SSID(i);
    ssid.replace("\"", "'");
    json += "{\"ssid\":\"" + ssid + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI(i));
    json += ",\"lock\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true");
    json += "}";
  }
  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

// Проверка без последствий: подключились, посмотрели, отключились. Настройки
// не трогаем — то, что сохранено, остаётся сохранённым.
static void handleWifiTest() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (!ssid.length()) { server.send(400, "application/json", "{\"ok\":false,\"why\":\"не выбрана сеть\"}"); return; }

  bool ok = staTry(ssid, pass);
  String ip = ok ? WiFi.localIP().toString() : "";
  String why = ok ? "" : staStatusText();
  WiFi.disconnect(false, true);        // забыть попытку, точку доступа оставить

  server.send(200, "application/json",
    "{\"ok\":" + String(ok ? "true" : "false") +
    ",\"ip\":\"" + ip + "\",\"why\":\"" + why + "\"}");
}

static void handleWifiSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (!ssid.length()) { server.send(400, "application/json", "{\"ok\":false}"); return; }

  bool ok = staTry(ssid, pass);
  if (ok) {
    Preferences prefs;
    prefs.begin("repeater", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.end();
    Serial.printf("[WiFi] Сеть сохранена: %s, адрес %s\n",
                  ssid.c_str(), WiFi.localIP().toString().c_str());
  }
  server.send(200, "application/json",
    "{\"ok\":" + String(ok ? "true" : "false") +
    ",\"ip\":\"" + (ok ? WiFi.localIP().toString() : "") + "\"" +
    ",\"why\":\"" + (ok ? "" : staStatusText()) + "\"}");
}

// Забыть сеть и остаться только точкой доступа — «по команде из настроек»,
// как и просили: возврат в исходное состояние без кабеля и перепрошивки.
static void handleWifiForget() {
  Preferences prefs;
  prefs.begin("repeater", false);
  prefs.remove("wifi_ssid");
  prefs.remove("wifi_pass");
  prefs.end();
  WiFi.disconnect(false, true);
  WiFi.mode(WIFI_AP);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleWifiState() {
  Preferences prefs;
  prefs.begin("repeater", true);
  String saved = prefs.getString("wifi_ssid", "");
  prefs.end();
  bool connected = WiFi.status() == WL_CONNECTED;
  String json = "{\"saved\":\"" + saved + "\"";
  json += ",\"connected\":" + String(connected ? "true" : "false");
  json += ",\"status\":\"" + staStatusText() + "\"";
  json += ",\"ip\":\"" + (connected ? WiFi.localIP().toString() : String("")) + "\"";
  json += ",\"ap\":\"" + WiFi.softAPIP().toString() + "\"}";
  server.send(200, "application/json", json);
}

// Станции в виде JSON: страница рисует по ним карту и радар, а заодно этим же
// адресом удобно пользоваться из своих скриптов.
static void handleNodes() {
  const RepeaterNode* list = nullptr;
  uint8_t n = repeaterGetNodes(&list);
  uint32_t now = millis();
  String json = "{\"nodes\":[";
  for (uint8_t i = 0; i < n; i++) {
    const RepeaterNode& d = list[i];
    if (i) json += ",";
    char id[9];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X", d.id[0], d.id[1], d.id[2], d.id[3]);
    json += "{\"id\":\"" + String(id) + "\"";
    json += ",\"cs\":\"" + String(d.call_sign) + "\"";
    json += ",\"gps\":" + String(d.has_gps ? "true" : "false");
    if (d.has_gps) {
      json += ",\"lat\":" + String(d.lat_e7 / 1e7, 6);
      json += ",\"lon\":" + String(d.lon_e7 / 1e7, 6);
      json += ",\"alt\":" + String(d.altitude_m);
    }
    json += ",\"rssi\":" + String(d.rssi);
    json += ",\"snr\":" + String(d.snr);
    json += ",\"bat\":" + String(d.battery);
    json += ",\"age\":" + String((now - d.last_seen_ms) / 1000);
    json += ",\"beacons\":" + String(d.beacons);
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void wifiMonitorInit() {
  startTime = millis();

  // Загрузить WiFi credentials и IP из NVS
  Preferences prefs;
  prefs.begin("repeater", true);
  String ssid = prefs.getString("wifi_ssid", "");
  String pass = prefs.getString("wifi_pass", "");
  String ipStr = prefs.getString("static_ip", "");
  prefs.end();

  if (ssid.length() > 0) {
    // И точка доступа, и клиент одновременно. Раньше при сохранённой сети
    // ретранслятор уходил в чистого клиента — и если сеть не поднялась (роутер
    // перезагружается, сменили пароль), попасть на его страницу было уже
    // нечем. Своя точка стоит копейки и остаётся всегда.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("MeshTRX-Repeater", "meshtrx123");

    // Статический IP если задан
    if (ipStr.length() > 0) {
      IPAddress ip, gw, sn;
      if (ip.fromString(ipStr)) {
        gw = IPAddress(ip[0], ip[1], ip[2], 1);
        sn = IPAddress(255, 255, 255, 0);
        WiFi.config(ip, gw, sn);
        Serial.printf("[WiFi] Static IP: %s\n", ipStr.c_str());
      }
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("[WiFi] Connecting to %s...\n", ssid.c_str());
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      timeout++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected: %s (точка доступа тоже работает: %s)\n",
                    WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str());
      // Сеть может пропасть и вернуться — переподключаемся сами, без участия
      // человека: до ретранслятора на мачте не дотянешься.
      WiFi.setAutoReconnect(true);
    } else {
      Serial.printf("[WiFi] сеть %s недоступна, работаем точкой доступа: %s\n",
                    ssid.c_str(), WiFi.softAPIP().toString().c_str());
    }
  } else {
    // SoftAP по умолчанию
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MeshTRX-Repeater", "meshtrx123");
    Serial.printf("[WiFi] AP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  server.on("/", handleRoot);
  server.on("/channel", handleSetChannel);
  server.on("/api/nodes", handleNodes);
  server.on("/api/scan", handleScan);
  server.on("/api/wifi/state", handleWifiState);
  server.on("/api/wifi/test", handleWifiTest);
  server.on("/api/wifi/save", handleWifiSave);
  server.on("/api/wifi/forget", handleWifiForget);
  server.on("/map", handleMap);
  server.begin();
  Serial.println("[WiFi] Web server started");
}

void wifiMonitorTask(void* param) {
  while (true) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
