#include "protocol.h"
#include "comms.h"
#include "storage.h"
#include "winder.h"
#include "state.h"
#include "config.h"
#include <WiFi.h>

#include <ArduinoJson.h>

Protocol g_protocol;

//============================================================================
//  参数 key <-> DeviceConfig 字段映射
//============================================================================
bool setConfigValue(DeviceConfig &c, const String &key, float value) {
    // 引脚
    if (key == "pinMotorPwm")         { c.pinMotorPwm = (uint8_t)value;           return true; }
    if (key == "pinServoPwm")         { c.pinServoPwm = (uint8_t)value;           return true; }
    if (key == "pinEndstop")          { c.pinEndstop = (uint8_t)value;            return true; }
    if (key == "pinHallIdler")        { c.pinHallIdler = (uint8_t)value;          return true; }
    if (key == "pinHallSpool")        { c.pinHallSpool = (uint8_t)value;          return true; }

    // 传感器
    if (key == "hallIdlerMagnets")    { c.hallIdlerMagnets = (uint8_t)value;      return true; }
    if (key == "hallSpoolMagnets")    { c.hallSpoolMagnets = (uint8_t)value;      return true; }
    if (key == "idlerDiameter")       { c.idlerDiameter = value;                  return true; }
    if (key == "hallDebounceUs")      { c.hallDebounceUs = (uint32_t)value;       return true; }
    if (key == "endstopDebounceUs")   { c.endstopDebounceUs = (uint32_t)value;    return true; }

    // 料盘
    if (key == "spoolOuterDiameter")  { c.spoolOuterDiameter = value;             return true; }
    if (key == "spoolWidth")          { c.spoolWidth = value;                     return true; }
    if (key == "spoolCoreDiaWithCard"){ c.spoolCoreDiaWithCard = value;           return true; }
    if (key == "spoolCoreDiaNoCard")  { c.spoolCoreDiaNoCard = value;             return true; }
    if (key == "spoolHasCardboard")   { c.spoolHasCardboard = (value != 0);       return true; }
    if (key == "filamentDiameter")    { c.filamentDiameter = value;               return true; }

    // 运动
    if (key == "traverseLeftStart")   { c.traverseLeftStart = value;              return true; }
    if (key == "traverseRightEnd")    { c.traverseRightEnd = value;               return true; }
    if (key == "traverseDistPerRev")  { c.traverseDistPerRev = value;             return true; }
    if (key == "leadScrewPitch")      { c.leadScrewPitch = value;                 return true; }
    if (key == "calIntervalRounds")   { c.calIntervalRounds = (uint8_t)value;     return true; }

    // 舵机
    if (key == "servoStopPulse")      { c.servoStopPulse = (uint16_t)value;       return true; }
    if (key == "servoLeftPulse")      { c.servoLeftPulse = (uint16_t)value;       return true; }
    if (key == "servoRightPulse")     { c.servoRightPulse = (uint16_t)value;      return true; }
    if (key == "servoHomePulse")      { c.servoHomePulse = (uint16_t)value;       return true; }
    if (key == "servoPulseMin")       { c.servoPulseMin = (uint16_t)value;        return true; }
    if (key == "servoPulseMax")       { c.servoPulseMax = (uint16_t)value;        return true; }
    if (key == "servoTraverseSpeedRight") { c.servoTraverseSpeedRight = value;    return true; }
    if (key == "servoTraverseSpeedLeft")  { c.servoTraverseSpeedLeft = value;     return true; }

    // 电机
    if (key == "motorMinSpeed")       { c.motorMinSpeed = (uint16_t)value;        return true; }
    if (key == "motorDefaultSpeed")   { c.motorDefaultSpeed = (uint16_t)value;    return true; }
    if (key == "motorMaxSpeed")       { c.motorMaxSpeed = (uint16_t)value;        return true; }
    if (key == "motorSoftStartMs")    { c.motorSoftStartMs = (uint16_t)value;     return true; }

    // 打滑
    if (key == "slipTolerance")       { c.slipTolerance = value;                  return true; }
    if (key == "stallTimeoutS")       { c.stallTimeoutS = value;                  return true; }

    // 任务完成
    if (key == "autoStopMode")        { c.autoStopMode = (uint8_t)value;          return true; }
    if (key == "targetLengthM")       { c.targetLengthM = value;                  return true; }
    if (key == "targetTurns")         { c.targetTurns = (uint32_t)value;          return true; }
    if (key == "fullLoadWarnPct")     { c.fullLoadWarnPct = (uint8_t)value;       return true; }

    // 通信
    if (key == "statusReportIntervalMs"){ c.statusReportIntervalMs = (uint16_t)value; return true; }
    if (key == "bleDisconnectStop")   { c.bleDisconnectStop = (value != 0);       return true; }
    if (key == "wifiDisconnectStop")  { c.wifiDisconnectStop = (value != 0);      return true; }

    return false;
}

String getConfigJson(const DeviceConfig &c) {
    JsonDocument doc;
    doc["pinMotorPwm"]              = c.pinMotorPwm;
    doc["pinServoPwm"]              = c.pinServoPwm;
    doc["pinEndstop"]               = c.pinEndstop;
    doc["pinHallIdler"]             = c.pinHallIdler;
    doc["pinHallSpool"]             = c.pinHallSpool;
    doc["hallIdlerMagnets"]         = c.hallIdlerMagnets;
    doc["hallSpoolMagnets"]         = c.hallSpoolMagnets;
    doc["idlerDiameter"]            = c.idlerDiameter;
    doc["hallDebounceUs"]           = c.hallDebounceUs;
    doc["endstopDebounceUs"]        = c.endstopDebounceUs;
    doc["spoolOuterDiameter"]       = c.spoolOuterDiameter;
    doc["spoolWidth"]               = c.spoolWidth;
    doc["spoolCoreDiaWithCard"]     = c.spoolCoreDiaWithCard;
    doc["spoolCoreDiaNoCard"]       = c.spoolCoreDiaNoCard;
    doc["spoolHasCardboard"]        = (int)c.spoolHasCardboard;
    doc["filamentDiameter"]         = c.filamentDiameter;
    doc["traverseLeftStart"]        = c.traverseLeftStart;
    doc["traverseRightEnd"]         = c.traverseRightEnd;
    doc["traverseDistPerRev"]       = c.traverseDistPerRev;
    doc["leadScrewPitch"]           = c.leadScrewPitch;
    doc["calIntervalRounds"]        = c.calIntervalRounds;
    doc["servoStopPulse"]           = c.servoStopPulse;
    doc["servoLeftPulse"]           = c.servoLeftPulse;
    doc["servoRightPulse"]          = c.servoRightPulse;
    doc["servoHomePulse"]           = c.servoHomePulse;
    doc["servoPulseMin"]            = c.servoPulseMin;
    doc["servoPulseMax"]            = c.servoPulseMax;
    doc["servoTraverseSpeedRight"]  = c.servoTraverseSpeedRight;
    doc["servoTraverseSpeedLeft"]   = c.servoTraverseSpeedLeft;
    doc["motorMinSpeed"]            = c.motorMinSpeed;
    doc["motorDefaultSpeed"]        = c.motorDefaultSpeed;
    doc["motorMaxSpeed"]            = c.motorMaxSpeed;
    doc["motorSoftStartMs"]         = c.motorSoftStartMs;
    doc["slipTolerance"]            = c.slipTolerance;
    doc["stallTimeoutS"]            = c.stallTimeoutS;
    doc["autoStopMode"]             = c.autoStopMode;
    doc["targetLengthM"]            = c.targetLengthM;
    doc["targetTurns"]              = c.targetTurns;
    doc["fullLoadWarnPct"]          = c.fullLoadWarnPct;
    doc["statusReportIntervalMs"]   = c.statusReportIntervalMs;
    doc["bleDisconnectStop"]        = (int)c.bleDisconnectStop;
    doc["wifiDisconnectStop"]       = (int)c.wifiDisconnectStop;

    String out;
    serializeJson(doc, out);
    return out;
}

//============================================================================
//  命令处理
//============================================================================

void Protocol::handle(const String &line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        sendResponse("error", false, "JSON 解析失败");
        return;
    }

    String cmd = doc["cmd"] | "";
    if (cmd.length() == 0) {
        sendResponse("error", false, "缺少 cmd 字段");
        return;
    }

    if (cmd == "start") {
        int speed = doc["speed"] | (int)g_config.motorDefaultSpeed;
        cmdStart(speed);
    } else if (cmd == "stop") {
        cmdStop();
    } else if (cmd == "pause") {
        cmdPause();
    } else if (cmd == "resume") {
        cmdResume();
    } else if (cmd == "home") {
        cmdHome();
    } else if (cmd == "set_speed") {
        int speed = doc["speed"] | 0;
        cmdSetSpeed(speed);
    } else if (cmd == "get_status") {
        sendStatus();
    } else if (cmd == "set_param") {
        String key = doc["key"] | "";
        float val  = doc["value"] | 0.0f;
        cmdSetParam(key, val);
    } else if (cmd == "set_params") {
        // 序列化 params 子对象为字符串再处理
        String paramsStr;
        serializeJson(doc["params"], paramsStr);
        cmdSetParams(paramsStr);
    } else if (cmd == "get_params") {
        sendParams();
    } else if (cmd == "save_preset") {
        String name = doc["name"] | "";
        cmdSavePreset(name);
    } else if (cmd == "load_preset") {
        String name = doc["name"] | "";
        cmdLoadPreset(name);
    } else if (cmd == "delete_preset") {
        String name = doc["name"] | "";
        cmdDeletePreset(name);
    } else if (cmd == "list_presets") {
        sendPresetList();
    } else if (cmd == "set_wifi") {
        String ssid = doc["ssid"] | "";
        String pass = doc["password"] | "";
        cmdSetWifi(ssid, pass);
    } else if (cmd == "get_wifi_status") {
        sendWifiStatus();
    } else if (cmd == "clear_error") {
        cmdClearError();
    } else if (cmd == "factory_reset") {
        cmdFactoryReset();
    } else {
        sendResponse("error", false, "未知命令: " + cmd);
    }
}

// --- 各命令 ---

void Protocol::cmdStart(int speed) {
    g_winder.startTask(speed);
    sendResponse("start", true);
}

void Protocol::cmdStop() {
    g_winder.stopTask();
    sendResponse("stop", true);
}

void Protocol::cmdPause() {
    g_winder.pauseTask();
    sendResponse("pause", true);
}

void Protocol::cmdResume() {
    g_winder.resumeTask();
    sendResponse("resume", true);
}

void Protocol::cmdHome() {
    g_winder.goHome();
    sendResponse("home", true);
}

void Protocol::cmdSetSpeed(int speed) {
    g_winder.setSpeed(speed);
    sendResponse("set_speed", true);
}

void Protocol::cmdSetParam(const String &key, float value) {
    if (setConfigValue(g_config, key, value)) {
        g_storage.saveConfig(g_config);
        g_winder.applyConfig();
        sendResponse("set_param", true);
    } else {
        sendResponse("set_param", false, "未知参数: " + key);
    }
}

void Protocol::cmdSetParams(const String &jsonStr) {
    JsonDocument params;
    DeserializationError err = deserializeJson(params, jsonStr);
    if (err) {
        sendResponse("set_params", false, "params 解析失败");
        return;
    }
    int count = 0;
    for (JsonPair kv : params.as<JsonObject>()) {
        if (setConfigValue(g_config, kv.key().c_str(), kv.value().as<float>())) {
            count++;
        }
    }
    g_storage.saveConfig(g_config);
    g_winder.applyConfig();
    sendResponse("set_params", true, "已更新 " + String(count) + " 个参数");
}

// --- 状态上报 ---

void Protocol::sendStatus() {
    JsonDocument doc;
    doc["type"]               = "status";
    doc["state"]              = stateName(g_state.state);
    doc["speed"]              = (int)g_state.currentSpeedPct;
    doc["spool_rpm"]          = roundf(g_state.spoolRpm * 10) / 10.0;
    doc["spool_turns"]        = roundf(g_state.spoolTurns * 10) / 10.0;
    doc["idler_turns"]        = roundf(g_state.idlerTurns * 10) / 10.0;
    doc["length_measured"]    = roundf(g_state.lengthMeasured * 100) / 100.0;
    doc["length_theoretical"] = roundf(g_state.lengthTheoretical * 100) / 100.0;
    doc["effective_diameter"] = roundf(g_state.effectiveDiameter * 10) / 10.0;
    doc["current_layer"]      = g_state.currentLayer;
    doc["traverse_pos"]       = roundf(g_state.traversePos * 10) / 10.0;
    doc["traverse_dir"]       = (g_state.traverseDir == DIR_LEFT)  ? "left"  :
                                (g_state.traverseDir == DIR_RIGHT) ? "right" : "none";
    doc["round_trips"]        = g_state.roundTrips;
    doc["calib_countdown"]    = g_state.calibCountdown;
    doc["link"]               = linkName(g_comms.activeLink());
    doc["uptime"]             = g_state.uptimeSec;

    if (g_state.state == STATE_ERROR) {
        doc["error_code"] = errorName(g_state.errorCode);
        doc["error_msg"]  = g_state.errorMsg;
    }

    String out;
    serializeJson(doc, out);
    g_comms.send(out);
}

void Protocol::sendError(ErrorCode code, const String &msg) {
    JsonDocument doc;
    doc["type"]    = "error";
    doc["code"]    = errorName(code);
    doc["msg"]     = msg;
    doc["length_measured"]    = roundf(g_state.lengthMeasured * 100) / 100.0;
    doc["length_theoretical"] = roundf(g_state.lengthTheoretical * 100) / 100.0;
    String out;
    serializeJson(doc, out);
    g_comms.send(out);
}

void Protocol::sendWifiStatus() {
    JsonDocument doc;
    doc["type"]      = "wifi_status";
    // 直接查 STA 是否连上家庭路由器（不受 AP 影响）
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    doc["connected"] = staConnected;
    doc["ip"]        = staConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["ssid"]      = staConnected ? g_state.wifiSSID : String("ESP-Winder");
    String out;
    serializeJson(doc, out);
    g_comms.send(out);
}

void Protocol::sendResponse(const String &type, bool ok, const String &msg) {
    JsonDocument doc;
    doc["type"]   = "response";
    doc["cmd"]    = type;
    doc["ok"]     = ok;
    if (msg.length() > 0) doc["msg"] = msg;
    String out;
    serializeJson(doc, out);
    g_comms.send(out);
}

void Protocol::sendParams() {
    JsonDocument doc;
    doc["type"]   = "params";
    doc["params"] = serialized(getConfigJson(g_config));
    String out;
    serializeJson(doc, out);
    g_comms.send(out);
}

void Protocol::sendPresetList() {
    JsonDocument doc;
    doc["type"] = "preset_list";
    JsonArray arr = doc["presets"].to<JsonArray>();
    for (int i = 0; i < MAX_PRESETS; i++) {
        Preset p;
        if (g_storage.loadPreset(i, p) && p.valid) {
            arr.add(p.name);
        }
    }
    String out;
    serializeJson(doc, out);
    g_comms.send(out);
}

void Protocol::cmdSavePreset(const String &name) {
    if (name.length() == 0) {
        sendResponse("save_preset", false, "名称不能为空");
        return;
    }
    // 先查重名，覆盖
    int idx = g_storage.findPresetByName(name.c_str());
    if (idx < 0) idx = g_storage.findFreeSlot();
    if (idx < 0) {
        sendResponse("save_preset", false, "预设已满");
        return;
    }
    g_storage.savePreset(idx, name.c_str(), g_config);
    sendResponse("save_preset", true);
    sendPresetList();
}

void Protocol::cmdLoadPreset(const String &name) {
    int idx = g_storage.findPresetByName(name.c_str());
    if (idx < 0) {
        sendResponse("load_preset", false, "预设不存在");
        return;
    }
    Preset p;
    g_storage.loadPreset(idx, p);
    g_config = p.config;
    g_storage.saveConfig(g_config);
    g_winder.applyConfig();
    sendResponse("load_preset", true);
    sendParams();
}

void Protocol::cmdDeletePreset(const String &name) {
    int idx = g_storage.findPresetByName(name.c_str());
    if (idx < 0) {
        sendResponse("delete_preset", false, "预设不存在");
        return;
    }
    g_storage.deletePreset(idx);
    sendResponse("delete_preset", true);
    sendPresetList();
}

void Protocol::cmdSetWifi(const String &ssid, const String &password) {
    if (ssid.length() == 0) {
        sendResponse("set_wifi", false, "SSID 不能为空");
        return;
    }
    sendResponse("set_wifi", true, "正在连接...");
    g_comms.wifiConfigure(ssid, password);
    sendWifiStatus();
}

void Protocol::cmdClearError() {
    g_winder.clearError();
    sendResponse("clear_error", true);
}

void Protocol::cmdFactoryReset() {
    sendResponse("factory_reset", true, "即将重启...");
    delay(500);
    g_storage.factoryReset();
    ESP.restart();
}
