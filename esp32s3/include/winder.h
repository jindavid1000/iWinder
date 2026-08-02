#ifndef WINDER_H
#define WINDER_H
//============================================================================
//  winder.h — 绕线主控制器（状态机 + 协调各模块）
//  运行在 core 1 的 FreeRTOS 任务中，10ms 循环。
//============================================================================
#include <Arduino.h>

#include "state.h"

class Winder {
public:
    void begin();
    void update();   // 在 loop 中调用

    // 控制接口（由 protocol 调用）
    void startTask(int speedPct);
    void stopTask();
    void pauseTask();
    void resumeTask();
    void goHome();
    void setSpeed(int speedPct);
    void clearError();

    // 配置变更后重新初始化硬件
    void applyConfig();

private:
    uint32_t _lastTickMs   = 0;
    uint32_t _lastReportMs = 0;
    bool     _hwInited     = false;

    // 排线状态
    uint16_t _roundTrips   = 0;    // 当前周期来回数
    uint32_t _homingStartMs = 0;
    bool     _bootHoming   = true;  // 开机寻原点（超时软处理）

    // 校准目标（双 Endstop）
    bool     _calibGoRight  = false;  // true = 去右限位
    bool     _calibReturning = false; // 限位已触发，正在返回起始位置
    bool     _homeGoRight   = false;  // 寻原点/归位方向

    // 速度
    int      _targetSpeed  = 0;

    void setState(DeviceState s);
    void setError(ErrorCode code, const String &msg);

    void doHoming();
    void doPositioning();
    void doRunning(uint32_t dtMs);
    void doCalibrating(uint32_t dtMs);
    void doCompleted();

    void processTraverse(uint32_t dtMs);
    void processSlipCheck(float dtSec);
    void processAutoStop();
    void reportStatus();
};

extern Winder g_winder;

#endif // WINDER_H
