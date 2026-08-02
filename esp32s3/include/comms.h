#ifndef COMMS_H
#define COMMS_H
//============================================================================
//  comms.h — 统一通信层（WiFi TCP）
//  对外提供 commSend() 发送消息，commSetCallback() 注册接收回调。
//============================================================================
#include <Arduino.h>

#include "state.h"

typedef void (*MsgCallback)(const String &msg);

class Comms {
public:
    void begin();

    // WiFi
    void wifiTryReconnect();              // 上电时尝试重连
    void wifiStartAP();                   // 开启 AP 热点（手机直连模式）
    bool wifiConnectSTA();                // 连家庭 WiFi（STA 模式）
    void wifiConfigure(const String &ssid, const String &password);
    bool isWifiConnected() const;
    String getWifiIP() const;
    String getWifiSSID() const;

    // 统一发送（推送到 WiFi TCP）
    void send(const String &msg);
    void send(const char *fmt, ...);

    // 接收回调
    void onMessage(MsgCallback cb) { _msgCb = cb; }
    void handleIncoming(const String &msg) { if (_msgCb) _msgCb(msg); }

    // 主循环更新（处理 TCP 收发）
    void update();

    // 当前活跃链路
    CommLink activeLink();

private:
    bool _wifiInit = false;
    MsgCallback _msgCb = nullptr;
};

extern Comms g_comms;

#endif // COMMS_H
