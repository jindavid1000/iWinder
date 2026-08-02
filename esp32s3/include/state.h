#ifndef STATE_H
#define STATE_H
//============================================================================
//  state.h — 设备状态机 + 运行时状态
//============================================================================
#include <Arduino.h>
#include "config.h"

// 设备状态枚举（对应文档 9.4 状态机）
enum DeviceState : uint8_t {
    STATE_IDLE        = 0,
    STATE_HOMING      = 1,
    STATE_POSITIONING = 2,   // 定位到绕线起始位置
    STATE_RUNNING     = 3,
    STATE_PAUSED      = 4,
    STATE_CALIBRATING = 5,
    STATE_ERROR       = 6,
    STATE_COMPLETED   = 7
};

// 排线方向
enum TraverseDir : uint8_t {
    DIR_NONE  = 0,
    DIR_LEFT  = 1,   // 向 Endstop（位置减小）
    DIR_RIGHT = 2    // 远离 Endstop（位置增大）
};

// 错误码
enum ErrorCode : uint8_t {
    ERR_NONE          = 0,
    ERR_SLIP          = 1,
    ERR_STALL         = 2,
    ERR_BREAK         = 3,
    ERR_HOMING_FAILED = 4,
    ERR_SENSOR        = 5
};

// 通信链路
enum CommLink : uint8_t {
    LINK_NONE = 0,
    LINK_BLE  = 1,
    LINK_WIFI = 2,
    LINK_BOTH = 3
};

// 运行时实时状态（由 winder 更新，由 protocol 上报）
struct RuntimeState {
    DeviceState   state         = STATE_IDLE;
    ErrorCode     errorCode     = ERR_NONE;
    String        errorMsg;

    // 速度
    float         currentSpeedPct = 0;   // 当前电机速度 %

    // 脉冲计数（ISR 安全）
    volatile uint32_t idlerPulses = 0;
    volatile uint32_t spoolPulses = 0;

    // 派生数据
    float         spoolRpm        = 0;
    float         spoolTurns      = 0;
    float         idlerTurns      = 0;
    float         lengthMeasured  = 0;   // m
    float         lengthTheoretical = 0; // m
    float         effectiveDiameter = 0; // mm
    uint16_t      currentLayer    = 0;

    // 排线
    float         traversePos     = 0;   // mm
    TraverseDir   traverseDir     = DIR_NONE;
    uint16_t      roundTrips      = 0;   // 当前周期已完成来回数
    uint16_t      calibCountdown  = 0;   // 距下次校准剩余来回数

    // 通信
    CommLink      activeLink      = LINK_NONE;
    bool          wifiConnected   = false;
    String        wifiIP          = "";
    String        wifiSSID        = "";

    // 运行时间
    uint32_t      uptimeSec       = 0;
    uint32_t      runStartMs      = 0;
};

// 全局实例
extern DeviceConfig  g_config;
extern RuntimeState  g_state;
extern portMUX_TYPE  g_dataMux;     // 保护脉冲计数

// 状态名称字符串
const char* stateName(DeviceState s);
const char* errorName(ErrorCode e);
const char* linkName(CommLink l);

#endif // STATE_H
