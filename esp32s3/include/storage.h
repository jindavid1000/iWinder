#ifndef STORAGE_H
#define STORAGE_H
//============================================================================
//  storage.h — NVS 参数存储 + 预设管理
//============================================================================
#include <Arduino.h>
#include "config.h"

// 预设方案
struct Preset {
    char        name[MAX_PRESET_NAME_LEN];
    DeviceConfig config;
    bool        valid;    // false = 空槽位
};

class Storage {
public:
    void begin();

    // --- 活跃配置 ---
    void loadConfig(DeviceConfig &cfg);
    void saveConfig(const DeviceConfig &cfg);

    // --- WiFi 凭据 ---
    void loadWiFi(String &ssid, String &password);
    void saveWiFi(const String &ssid, const String &password);
    void clearWiFi();

    // --- 预设管理 ---
    int  getPresetCount();
    bool loadPreset(int index, Preset &out);
    bool savePreset(int index, const char *name, const DeviceConfig &cfg);
    bool deletePreset(int index);
    int  findPresetByName(const char *name);
    int  findFreeSlot();

    // --- 出厂恢复 ---
    void factoryReset();

private:
    // 内部键名定义在 storage.cpp 中
};

extern Storage g_storage;

#endif // STORAGE_H
