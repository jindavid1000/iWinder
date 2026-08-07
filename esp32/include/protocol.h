#ifndef PROTOCOL_H
#define PROTOCOL_H
//============================================================================
//  protocol.h — JSON 通信协议解析/构建
//============================================================================
#include <Arduino.h>

#include "state.h"
#include "config.h"

class Protocol {
public:
    // 处理收到的 JSON 命令行
    void handle(const String &line);

    // 发送状态上报
    void sendStatus();

    // 发送错误
    void sendError(ErrorCode code, const String &msg);

    // 发送 WiFi 状态
    void sendWifiStatus();

    // 发送通用响应
    void sendResponse(const String &type, bool ok, const String &msg = "");

    // 发送参数列表
    void sendParams();

    // 发送预设列表
    void sendPresetList();

private:
    // 命令处理
    void cmdStart(int speed);
    void cmdStop();
    void cmdPause();
    void cmdResume();
    void cmdHome();
    void cmdSetSpeed(int speed);
    void cmdSetParam(const String &key, float value);
    void cmdSetParams(const String &jsonStr);
    void cmdSavePreset(const String &name);
    void cmdLoadPreset(const String &name);
    void cmdDeletePreset(const String &name);
    void cmdSetWifi(const String &ssid, const String &password);
    void cmdClearError();
    void cmdFactoryReset();
    void cmdCalibrateServo();
};

extern Protocol g_protocol;

// 参数 key <-> DeviceConfig 字段的映射辅助
bool setConfigValue(DeviceConfig &cfg, const String &key, float value);
String getConfigJson(const DeviceConfig &cfg);

#endif // PROTOCOL_H
