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
    // Подключиться к существующей сети
    WiFi.mode(WIFI_STA);

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
      Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WiFi] STA failed, starting AP");
      WiFi.mode(WIFI_AP);
      WiFi.softAP("MeshTRX-Repeater", "meshtrx123");
      Serial.printf("[WiFi] AP: %s\n", WiFi.softAPIP().toString().c_str());
    }
  } else {
    // SoftAP по умолчанию
    WiFi.mode(WIFI_AP);
    WiFi.softAP("MeshTRX-Repeater", "meshtrx123");
    Serial.printf("[WiFi] AP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  server.on("/", handleRoot);
  server.on("/channel", handleSetChannel);
  server.begin();
  Serial.println("[WiFi] Web server started");
}

void wifiMonitorTask(void* param) {
  while (true) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
