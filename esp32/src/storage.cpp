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
