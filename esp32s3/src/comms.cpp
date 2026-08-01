#include "comms.h"
#include "state.h"
#include "storage.h"
#include "config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>

Comms g_comms;

// ============================================================================
//  BLE
// ============================================================================

static BLEServer*       s_bleServer = nullptr;
static BLECharacteristic* s_txChar  = nullptr;

class BleServerCb : public BLEServerCallbacks {
    void onConnect(BLEServer* s) override {
        g_state.bleConnected = true;
    }
    void onDisconnect(BLEServer* s) override {
        g_state.bleConnected = false;
        s->getAdvertising()->start();
    }
};

class BleRxCb : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* chr) override {
        std::string val = chr->getValue();
        String msg(val.c_str(), val.length());
        msg.trim();
        if (msg.length() > 0) {
            g_comms.handleIncoming(msg);
        }
    }
};

// ============================================================================
//  WiFi TCP
// ============================================================================

static WiFiServer* s_tcpServer = nullptr;
static WiFiClient  s_tcpClient;
static String      s_tcpBuffer;

// ============================================================================
//  Comms 实现
// ============================================================================

void Comms::begin() {
    bleStart();
}

void Comms::bleStart() {
    BLEDevice::init(BLE_DEVICE_NAME);
    s_bleServer = BLEDevice::createServer();
    s_bleServer->setCallbacks(new BleServerCb());

    BLEService* svc = s_bleServer->createService(BLE_SERVICE_UUID);

    BLECharacteristic* rxChar = svc->createCharacteristic(
        BLE_RX_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    rxChar->setCallbacks(new BleRxCb());

    s_txChar = svc->createCharacteristic(
        BLE_TX_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    s_txChar->addDescriptor(new BLE2902());

    svc->start();
    s_bleServer->getAdvertising()->start();
    _bleInit = true;

    Serial.printf("[BLE] 广播: %s\n", BLE_DEVICE_NAME);
}

bool Comms::isBleConnected() const {
    return g_state.bleConnected;
}

// --- WiFi ---

void Comms::wifiTryReconnect() {
    String ssid, pass;
    g_storage.loadWiFi(ssid, pass);
    if (ssid.length() == 0) {
        Serial.println("[WiFi] 无已保存凭据，跳过重连");
        return;
    }

    Serial.printf("[WiFi] 尝试连接: %s\n", ssid.c_str());
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
        g_state.wifiConnected = true;
        g_state.wifiIP   = WiFi.localIP().toString();
        g_state.wifiSSID = ssid;
        Serial.printf("[WiFi] 已连接, IP: %s\n", g_state.wifiIP.c_str());
        if (!s_tcpServer) {
            s_tcpServer = new WiFiServer(WIFI_TCP_PORT);
            s_tcpServer->begin();
            Serial.printf("[WiFi] TCP 服务启动, 端口 %d\n", WIFI_TCP_PORT);
        }
    } else {
        Serial.println("[WiFi] 连接失败/超时");
        g_state.wifiConnected = false;
    }
}

void Comms::wifiConfigure(const String &ssid, const String &password) {
    Serial.printf("[WiFi] 收到配网请求: %s\n", ssid.c_str());
    g_storage.saveWiFi(ssid, password);

    if (s_tcpClient) s_tcpClient.stop();
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
        g_state.wifiIP   = WiFi.localIP().toString();
        g_state.wifiSSID = ssid;
        if (!s_tcpServer) {
            s_tcpServer = new WiFiServer(WIFI_TCP_PORT);
            s_tcpServer->begin();
        }
        Serial.printf("[WiFi] 配网成功, IP: %s\n", g_state.wifiIP.c_str());
    } else {
        g_state.wifiConnected = false;
        Serial.println("[WiFi] 配网失败");
    }
}

bool Comms::isWifiConnected() const {
    return g_state.wifiConnected && WiFi.status() == WL_CONNECTED;
}

String Comms::getWifiIP() const {
    return g_state.wifiIP;
}

String Comms::getWifiSSID() const {
    return g_state.wifiSSID;
}

// --- 发送 ---

void Comms::send(const String &msg) {
    String line = msg + "\n";
    // BLE
    if (_bleInit && s_txChar && g_state.bleConnected) {
        s_txChar->setValue(line.c_str());
        s_txChar->notify();
    }
    // WiFi TCP
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

// --- 接收/更新 ---

void Comms::update() {
    // WiFi TCP
    if (s_tcpServer) {
        WiFiClient newClient = s_tcpServer->accept();
        if (newClient) {
            s_tcpClient = newClient;
            g_state.wifiConnected = true;
            Serial.println("[WiFi] TCP 客户端已连接");
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
        } else if (g_state.wifiConnected) {
            // TCP 断开
            g_state.wifiConnected = false;
            Serial.println("[WiFi] TCP 客户端断开");
        }
    }
}

CommLink Comms::activeLink() {
    bool ble  = g_state.bleConnected;
    bool wifi = isWifiConnected() && s_tcpClient && s_tcpClient.connected();
    if (ble && wifi) return LINK_BOTH;
    if (ble)         return LINK_BLE;
    if (wifi)        return LINK_WIFI;
    return LINK_NONE;
}
