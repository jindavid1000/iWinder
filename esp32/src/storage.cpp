#include "storage.h"
#include <Preferences.h>

Storage g_storage;

static const char *K_CFG        = "cfg";
static const char *K_WIFI_SSID  = "wssid";
static const char *K_WIFI_PASS  = "wpass";

void Storage::begin() {
    // 无需显式初始化，Preferences 每次 begin 即可
}

// --- 活跃配置 ---
void Storage::loadConfig(DeviceConfig &cfg) {
    Preferences prefs;
    // 先尝试只读打开
    bool ok = prefs.begin(NVS_NAMESPACE, true);
    if (!ok) {
        // namespace 不存在（首次启动），用默认值并创建
        cfg = DeviceConfig::defaults();
        prefs.end();
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putBytes(K_CFG, &cfg, sizeof(DeviceConfig));
        prefs.end();
        Serial.println("[Storage] 首次启动，已写入默认配置");
        return;
    }
    size_t len = prefs.getBytesLength(K_CFG);
    if (len == sizeof(DeviceConfig)) {
        prefs.getBytes(K_CFG, &cfg, sizeof(DeviceConfig));
        // 一次性迁移: 行程修正 64→59（实测触发间距 58.8mm，原 64 为含回弹的过测量）
        if (cfg.travelRangeMm == 64.0f && cfg.traverseRightEnd == 62.0f) {
            cfg.travelRangeMm    = 59.0f;
            cfg.traverseRightEnd = 56.0f;
            prefs.end();
            saveConfig(cfg);
            Serial.println("[Storage] 已迁移行程参数: 限位间距 64→59mm，右换向 62→56mm");
            return;
        }
        // 一次性迁移: 左起始 12→2（整机右移 10mm，料盘左法兰对齐坐标 2）
        if (!prefs.getBool("lftDef2", false)) {
            prefs.putBool("lftDef2", true);
            if (cfg.traverseLeftStart == 12.0f) {
                cfg.traverseLeftStart = 2.0f;
                prefs.end();
                saveConfig(cfg);
                Serial.println("[Storage] 已迁移: 左起始位置 12→2mm");
                return;
            }
        }
        // 一次性迁移: 默认改为 AS5600 编码器闭环（2026-08，本机已装编码器且实测可靠）
        if (!prefs.getBool("encDef1", false)) {
            prefs.putBool("encDef1", true);
            if (cfg.traverseEncoder == 0) {
                cfg.traverseEncoder = 1;
                prefs.end();
                saveConfig(cfg);
                Serial.println("[Storage] 已迁移: 默认位置反馈 → AS5600 编码器闭环");
                return;
            }
        }
        // 迁移: 行程参数修正（2026-08，test_encoder 实测限位间距 64.3mm，原 80 为估计值）
        if (cfg.travelRangeMm == 80.0f && cfg.traverseRightEnd == 80.0f) {
            cfg.travelRangeMm    = 64.0f;
            cfg.traverseRightEnd = 62.0f;
            prefs.end();
            saveConfig(cfg);
            Serial.println("[Storage] 已迁移行程参数: 限位间距 80→64mm，右换向 80→62mm");
            return;
        }
        if (cfg.travelRangeMm == 70.0f && cfg.traverseRightEnd == 68.0f) {
            cfg.travelRangeMm    = 64.0f;
            cfg.traverseRightEnd = 62.0f;
            prefs.end();
            saveConfig(cfg);
            Serial.println("[Storage] 已迁移行程参数: 限位间距 70→64mm，右换向 68→62mm");
            return;
        }
        // 迁移: 左右限位引脚互换（2026-08 接线对调）
        if (cfg.pinEndstop == 32 && cfg.pinEndstopRight == 14) {
            cfg.pinEndstop = 14;
            cfg.pinEndstopRight = 32;
            prefs.end();
            saveConfig(cfg);
            Serial.println("[Storage] 已迁移限位引脚: 左=14 右=32");
            return;
        }
    } else {
        cfg = DeviceConfig::defaults();
    }
    prefs.end();
}

void Storage::saveConfig(const DeviceConfig &cfg) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes(K_CFG, &cfg, sizeof(DeviceConfig));
    prefs.end();
}

// --- WiFi 凭据 ---
void Storage::loadWiFi(String &ssid, String &password) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    ssid     = prefs.getString(K_WIFI_SSID, "");
    password = prefs.getString(K_WIFI_PASS, "");
    prefs.end();
}

void Storage::saveWiFi(const String &ssid, const String &password) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(K_WIFI_SSID, ssid);
    prefs.putString(K_WIFI_PASS, password);
    prefs.end();
}

void Storage::clearWiFi() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(K_WIFI_SSID);
    prefs.remove(K_WIFI_PASS);
    prefs.end();
}

// --- 预设管理 ---
// 每个预设存储为 "pre0" ~ "pre9" 的 blob
static String presetKey(int index) {
    char key[8];
    snprintf(key, sizeof(key), "pre%d", index);
    return String(key);
}

int Storage::getPresetCount() {
    int count = 0;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    for (int i = 0; i < MAX_PRESETS; i++) {
        if (prefs.getBytesLength(presetKey(i).c_str()) == sizeof(Preset)) {
            count++;
        }
    }
    prefs.end();
    return count;
}

bool Storage::loadPreset(int index, Preset &out) {
    if (index < 0 || index >= MAX_PRESETS) return false;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    String key = presetKey(index);
    bool ok = (prefs.getBytesLength(key.c_str()) == sizeof(Preset));
    if (ok) {
        prefs.getBytes(key.c_str(), &out, sizeof(Preset));
    }
    prefs.end();
    return ok;
}

bool Storage::savePreset(int index, const char *name, const DeviceConfig &cfg) {
    if (index < 0 || index >= MAX_PRESETS) return false;
    Preset p;
    memset(&p, 0, sizeof(p));
    strncpy(p.name, name, MAX_PRESET_NAME_LEN - 1);
    p.config = cfg;
    p.valid  = true;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    String key = presetKey(index);
    prefs.putBytes(key.c_str(), &p, sizeof(Preset));
    prefs.end();
    return true;
}

bool Storage::deletePreset(int index) {
    if (index < 0 || index >= MAX_PRESETS) return false;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    String key = presetKey(index);
    bool ok = prefs.remove(key.c_str());
    prefs.end();
    return ok;
}

int Storage::findPresetByName(const char *name) {
    for (int i = 0; i < MAX_PRESETS; i++) {
        Preset p;
        if (loadPreset(i, p) && p.valid && strcmp(p.name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int Storage::findFreeSlot() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    for (int i = 0; i < MAX_PRESETS; i++) {
        if (prefs.getBytesLength(presetKey(i).c_str()) != sizeof(Preset)) {
            prefs.end();
            return i;
        }
    }
    prefs.end();
    return -1;  // 满
}

// --- 出厂恢复 ---
void Storage::factoryReset() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
}
