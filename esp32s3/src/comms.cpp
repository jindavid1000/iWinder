#include "comms.h"
#include "state.h"
#include "storage.h"
#include "config.h"
#include <WiFi.h>

Comms g_comms;

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.println("[WiFi-EVT] AP 已启动");
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
            uint8_t mac[6];
            memcpy(mac, info.wifi_ap_staconnected.mac, 6);
            Serial.printf("[WiFi-EVT] 设备已关联 MAC=%02X:%02X:%02X:%02X:%02X:%02X AID=%d\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                          info.wifi_ap_staconnected.aid);
            break;
        }
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("[WiFi-EVT] 设备已断开关联");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.printf("[WiFi-EVT] DHCP 已分配 IP=%s\n",
                          IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
            break;
        default:
            break;
    }
}

static WiFiServer* s_tcpServer = nullptr;
static WiFiClient  s_tcpClient;
static String      s_tcpBuffer;
static bool        s_clientConnected = false;

void Comms::begin() {
    WiFi.onEvent(onWiFiEvent);
    bool staOk = wifiConnectSTA();
    if (!staOk) {
        Serial.println("[WiFi] 无凭据或 STA 失败，进入 AP 模式");
        wifiStartAP();
    }
    WiFi.setSleep(false);
}

bool Comms::wifiConnectSTA() {
    Serial.println("[WiFi] 检查凭据...");
    String ssid, pass;
    g_storage.loadWiFi(ssid, pass);
    if (ssid.length() == 0) {
        ssid = WIFI_STA_SSID;
        pass = WIFI_STA_PASSWORD;
    }
    if (ssid == "YOUR_WIFI_NAME" || ssid == "DISABLED_STA_TEST" || ssid.length() == 0) {
        Serial.println("[WiFi] 无凭据，直接 AP");
        return false;
    }

    Serial.printf("[WiFi] STA 连接: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - start) < (uint32_t)(WIFI_CONNECT_TIMEOUT_S * 1000)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        Serial.printf("[WiFi] STA 已连接! IP: %s\n", ip.toString().c_str());
        g_state.wifiConnected = true;
        g_state.wifiIP = ip.toString();
        g_state.wifiSSID = ssid;
        if (!s_tcpServer) {
            s_tcpServer = new WiFiServer(WIFI_TCP_PORT);
            s_tcpServer->begin();
            Serial.printf("[WiFi] TCP: %s:%d\n", ip.toString().c_str(), WIFI_TCP_PORT);
        }
        return true;
    }
    Serial.println("[WiFi] STA 失败");
    return false;
}

void Comms::wifiStartAP() {
    Serial.println("[WiFi] 启动 AP 模式");
    WiFi.mode(WIFI_AP);
    delay(100);
    bool ok = WiFi.softAP("ESP-Winder");
    delay(200);
    Serial.printf("[WiFi] softAP: %s\n", ok ? "OK" : "FAIL");
    if (ok) {
        IPAddress ip = WiFi.softAPIP();
        Serial.printf("[WiFi] AP IP: %s\n", ip.toString().c_str());
        g_state.wifiConnected = true;
        g_state.wifiIP = ip.toString();
        g_state.wifiSSID = "ESP-Winder";
        if (!s_tcpServer) {
            s_tcpServer = new WiFiServer(WIFI_TCP_PORT);
            s_tcpServer->begin();
            Serial.printf("[WiFi] TCP: %s:%d\n", ip.toString().c_str(), WIFI_TCP_PORT);
        }
    } else {
        Serial.println("[WiFi] AP 启动失败!");
    }
}

void Comms::wifiConfigure(const String &ssid, const String &password) {
    Serial.printf("[WiFi] 配网: %s\n", ssid.c_str());
    g_storage.saveWiFi(ssid, password);
    if (s_tcpClient) s_tcpClient.stop();
    s_clientConnected = false;
    WiFi.disconnect(true, true);
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - start) < (uint32_t)(WIFI_CONNECT_TIMEOUT_S * 1000)) {
        delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) {
        g_state.wifiConnected = true;
        g_state.wifiIP = WiFi.localIP().toString();
        g_state.wifiSSID = ssid;
        if (!s_tcpServer) {
            s_tcpServer = new WiFiServer(WIFI_TCP_PORT);
            s_tcpServer->begin();
        }
        Serial.printf("[WiFi] 配网成功: %s\n", g_state.wifiIP.c_str());
    } else {
        g_state.wifiConnected = false;
        Serial.println("[WiFi] 配网失败，回退 AP 模式");
        wifiStartAP();
    }
}

bool Comms::isWifiConnected() const {
    if (WiFi.getMode() == WIFI_AP) {
        return WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
    }
    return WiFi.status() == WL_CONNECTED;
}

String Comms::getWifiIP() const { return g_state.wifiIP; }
String Comms::getWifiSSID() const { return g_state.wifiSSID; }

void Comms::send(const String &msg) {
    String line = msg + "\n";
    if (s_tcpClient && s_tcpClient.connected()) {
        s_tcpClient.print(line);
    }
}

void Comms::send(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    send(String(buf));
}

void Comms::update() {
    if (!s_tcpServer) return;

    WiFiClient newClient = s_tcpServer->accept();
    if (newClient) {
        s_tcpClient = newClient;
        s_clientConnected = true;
        Serial.printf("[WiFi] TCP 客户端已连接: %s\n", newClient.remoteIP().toString().c_str());
        s_tcpClient.print("{\"type\":\"response\",\"cmd\":\"connect\",\"ok\":true,\"msg\":\"ESP-Winder ready\"}\n");
    }

    if (s_tcpClient && s_tcpClient.connected()) {
        while (s_tcpClient.available()) {
            char c = s_tcpClient.read();
            if (c == '\n') {
                s_tcpBuffer.trim();
                if (s_tcpBuffer.length() > 0 && _msgCb) {
                    _msgCb(s_tcpBuffer);
                }
                s_tcpBuffer = "";
            } else if (c != '\r') {
                s_tcpBuffer += c;
                if (s_tcpBuffer.length() > 1024) s_tcpBuffer = "";
            }
        }
    } else {
        if (s_clientConnected) {
            s_clientConnected = false;
            Serial.println("[WiFi] TCP 客户端断开");
        }
    }
}

CommLink Comms::activeLink() {
    bool wifi = isWifiConnected() && s_tcpClient && s_tcpClient.connected();
    if (wifi) return LINK_WIFI;
    return LINK_NONE;
}
