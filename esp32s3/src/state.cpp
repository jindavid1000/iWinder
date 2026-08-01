#include "state.h"

// 全局实例定义
DeviceConfig  g_config;
RuntimeState  g_state;
portMUX_TYPE  g_dataMux = portMUX_INITIALIZER_UNLOCKED;

const char* stateName(DeviceState s) {
    switch (s) {
        case STATE_IDLE:        return "idle";
        case STATE_HOMING:      return "homing";
        case STATE_POSITIONING: return "positioning";
        case STATE_RUNNING:     return "running";
        case STATE_PAUSED:      return "paused";
        case STATE_CALIBRATING: return "calibrating";
        case STATE_ERROR:       return "error";
        case STATE_COMPLETED:   return "completed";
        default:                return "unknown";
    }
}

const char* errorName(ErrorCode e) {
    switch (e) {
        case ERR_NONE:          return "none";
        case ERR_SLIP:          return "slip";
        case ERR_STALL:         return "stall";
        case ERR_BREAK:         return "break";
        case ERR_HOMING_FAILED: return "homing_failed";
        case ERR_SENSOR:        return "sensor_error";
        default:                return "unknown";
    }
}

const char* linkName(CommLink l) {
    switch (l) {
        case LINK_NONE: return "none";
        case LINK_BLE:  return "ble";
        case LINK_WIFI: return "wifi";
        case LINK_BOTH: return "both";
        default:        return "none";
    }
}
