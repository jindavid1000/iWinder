#include "comms.h"
#include "state.h"
#include "storage.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>

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
// 多客户端: APP/Web 调试工具可同时在线，互不顶掉（曾为单客户端，
// 新连接静默覆盖旧 socket，导致 APP 假连接、状态停止更新）
#define TCP_MAX_CLIENTS 4
static WiFiClient  s_tcpClients[TCP_MAX_CLIENTS];
static String      s_tcpBuffers[TCP_MAX_CLIENTS];
static bool        s_clientConnected = false;
static WiFiUDP      s_broadcastUdp;
static unsigned long s_lastBroadcastMs = 0;

void Comms::begin() {
    WiFi.onEvent(onWiFiEvent);
    WiFi.setSleep(false);
    bool staOk = wifiConnectSTA();
    if (!staOk) {
        Serial.println("[WiFi] 无凭据或 STA 失败，进入 AP 模式");
        wifiStartAP();
    }
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
    WiFi.mode(WIFI_AP_STA);  // APSTA：STA 连家庭 WiFi，AP 做兜底
    delay(100);
    WiFi.softAP("ESP-Winder");
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
        s_broadcastUdp.begin(8888);

        // mDNS: 家庭 WiFi 内用 http://iwinder.local 访问（iOS/macOS 浏览器原生支持）
        if (MDNS.begin("iwinder")) {
            MDNS.addService("http", "tcp", 80);
            Serial.println("[WiFi] mDNS 已启动: http://iwinder.local");
        }
    } else {
        Serial.println("[WiFi] STA 失败，保持 AP 模式");
    }

    // 无论 STA 是否成功，都启动 TCP 服务器（AP 一定在运行）
    if (!s_tcpServer) {
        s_tcpServer = new WiFiServer(WIFI_TCP_PORT);
        s_tcpServer->begin();
    }
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WiFi] AP IP: %s, TCP: %d\n", apIP.toString().c_str(), WIFI_TCP_PORT);
    return WiFi.status() == WL_CONNECTED;
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
        // AP 模式也开 UDP 发现（连热点的手机能搜到设备，
        // 且 parsePacket 不会因套接字未初始化而狂报错）
        s_broadcastUdp.begin(8888);
    }
}

void Comms::wifiConfigure(const String &ssid, const String &password) {
    Serial.printf("[WiFi] 配网: %s\n", ssid.c_str());
    g_storage.saveWiFi(ssid, password);

    // APSTA 模式：保持 AP 连接不断，同时连家庭 WiFi
    // 手机通过 AP (192.168.4.1) 的 TCP 连接不会断
    // ESP 连上家庭 WiFi 后立即回报局域网 IP
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
        WiFi.softAP("ESP-Winder");  // 确保 AP 在运行
    }
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - start) < (uint32_t)(WIFI_CONNECT_TIMEOUT_S * 1000)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        String staIP = WiFi.localIP().toString();
        g_state.wifiConnected = true;
        g_state.wifiIP = staIP;
        g_state.wifiSSID = ssid;
        Serial.printf("[WiFi] 配网成功: 局域网 IP=%s\n", staIP.c_str());

        // 立即通过 AP 的 TCP 连接回报局域网 IP
        String msg = "{\"type\":\"wifi_status\",\"connected\":true,\"ip\":\"" +
                     staIP + "\",\"ssid\":\"" + ssid + "\"}\n";
        for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
            if (s_tcpClients[i] && s_tcpClients[i].connected()) {
                s_tcpClients[i].print(msg);
            }
        }
        Serial.println("[WiFi] 已回报局域网 IP 给 APP");

        // 启动 UDP 广播，让局域网内其他设备也能发现
        s_broadcastUdp.begin(8888);
    } else {
        Serial.println("[WiFi] 配网失败，保持 AP+STA 模式");
    }
}

bool Comms::isWifiConnected() const {
    if (WiFi.getMode() & WIFI_MODE_AP) {
        if (WiFi.getMode() & WIFI_MODE_STA) {
            // APSTA：STA 连上了就算在线
            return WiFi.status() == WL_CONNECTED || WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
        }
        return WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
    }
    return WiFi.status() == WL_CONNECTED;
}

String Comms::getWifiIP() const { return g_state.wifiIP; }
String Comms::getWifiSSID() const { return g_state.wifiSSID; }

void Comms::send(const String &msg) {
    String line = msg + "\n";
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (s_tcpClients[i] && s_tcpClients[i].connected()) {
            s_tcpClients[i].print(line);
        }
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
        int slot = -1;
        for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
            if (!s_tcpClients[i] || !s_tcpClients[i].connected()) { slot = i; break; }
        }
        if (slot >= 0) {
            s_tcpClients[slot] = newClient;
            s_tcpBuffers[slot] = "";
            s_clientConnected = true;
            Serial.printf("[WiFi] TCP 客户端已连接: %s (槽位 %d)\n",
                          newClient.remoteIP().toString().c_str(), slot);
            s_tcpClients[slot].print("{\"type\":\"response\",\"cmd\":\"connect\",\"ok\":true,\"msg\":\"ESP-Winder ready\"}\n");
        } else {
            // 满了: 显式关闭，客户端能感知断开并重连
            newClient.stop();
            Serial.println("[WiFi] TCP 客户端满，拒绝新连接");
        }
    }

    bool anyConnected = false;
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (s_tcpClients[i] && s_tcpClients[i].connected()) {
            anyConnected = true;
            while (s_tcpClients[i].available()) {
                char c = s_tcpClients[i].read();
                if (c == '\n') {
                    s_tcpBuffers[i].trim();
                    if (s_tcpBuffers[i].length() > 0 && _msgCb) {
                        _msgCb(s_tcpBuffers[i]);
                    }
                    s_tcpBuffers[i] = "";
                } else if (c != '\r') {
                    s_tcpBuffers[i] += c;
                    if (s_tcpBuffers[i].length() > 1024) s_tcpBuffers[i] = "";
                }
            }
        } else if (s_tcpClients[i]) {
            s_tcpClients[i].stop();
            s_tcpClients[i] = WiFiClient();
            s_tcpBuffers[i] = "";
        }
    }
    if (!anyConnected && s_clientConnected) {
        s_clientConnected = false;
        Serial.println("[WiFi] TCP 客户端全部断开");
    }

    // 周期 UDP 广播设备信息（仅 STA 连上家庭 WiFi 时）
    if (WiFi.status() == WL_CONNECTED && (millis() - s_lastBroadcastMs > 2000)) {
        s_lastBroadcastMs = millis();
        String staIP = WiFi.localIP().toString();
        String msg = "WINDER:" + staIP + ":" + String(WIFI_TCP_PORT);
        s_broadcastUdp.beginPacket(IPAddress(255, 255, 255, 255), 8888);
        s_broadcastUdp.print(msg);
        s_broadcastUdp.endPacket();
    }

    // 响应 APP 的主动发现请求（单播回复，避免 Android 丢弃广播包）
    int pktLen = s_broadcastUdp.parsePacket();
    if (pktLen > 0) {
        char buf[64] = {0};
        s_broadcastUdp.read(buf, sizeof(buf) - 1);
        String req = String(buf);
        req.trim();
        if (req == "DISCOVER_WINDER" && WiFi.status() == WL_CONNECTED) {
            IPAddress remoteIP = s_broadcastUdp.remoteIP();
            uint16_t remotePort = s_broadcastUdp.remotePort();
            String staIP = WiFi.localIP().toString();
            String reply = "WINDER:" + staIP + ":" + String(WIFI_TCP_PORT);
            s_broadcastUdp.beginPacket(remoteIP, remotePort);
            s_broadcastUdp.print(reply);
            s_broadcastUdp.endPacket();
            Serial.printf("[WiFi] 发现请求来自 %s:%d -> 单播回复 %s\n",
                          remoteIP.toString().c_str(), remotePort, reply.c_str());
        }
    }
}

CommLink Comms::activeLink() {
    bool anyCli = false;
    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
        if (s_tcpClients[i] && s_tcpClients[i].connected()) { anyCli = true; break; }
    bool wifi = isWifiConnected() && anyCli;
    if (wifi) return LINK_WIFI;
    return LINK_NONE;
}
