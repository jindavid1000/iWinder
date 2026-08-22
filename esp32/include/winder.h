#ifndef WINDER_H
#define WINDER_H
//============================================================================
//  winder.h — 绕线主控（状态机 + 任务编排）
//  排线闭环见 traverse.h，舵机标定见 calib_servo.h。
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
    void startServoCalib();      // 委托给 ServoCalibrator

    // 配置变更后重新初始化硬件
    void applyConfig();

    // 供 traverse / calib_servo 模块使用
    void reportFault(const char *msg);      // 排线模块上报异常
    void setStateExternal(DeviceState s) { setState(s); }

    // 驱动模式: 0=电动 1=手动(手摇，电机不输出)
    bool isManualMode() const { return g_config.driveMode == 1; }

private:
    uint32_t _lastTickMs   = 0;
    uint32_t _lastReportMs = 0;
    bool     _hwInited     = false;
    bool     _encInited    = false;

    // RPM: 脉冲间隔法 + 无脉冲超时判停
    uint32_t _lastPulseMs  = 0;

    // 寻原点 / 定位
    uint32_t _homingStartMs = 0;
    bool     _bootHoming    = true;
    bool     _homeGoRight   = false;
    float    _encRevsAtHomeStart = 0;   // 寻原点起始圈数（编码器方向自学习）

    // 周期校准
    uint32_t _calibStartMs = 0;

    // 手动模式: 停转触发校准
    bool     _manualSeenSpinning = false;
    uint32_t _manualZeroSinceMs  = 0;

    // 电动模式: 缠料检测
    uint32_t _jamStallMs = 0;

    int      _targetSpeed = 0;
    float    _smoothRpm   = 0;

    void setState(DeviceState s);
    void setError(ErrorCode code, const String &msg);
    void enterCalibrating();

    void doHoming();
    void doPositioning();
    void doRunning(uint32_t dtMs);
    void doCalibrating(uint32_t dtMs);
    void doCompleted();
    void processAutoStop();
    void reportStatus();
};

extern Winder g_winder;

#endif // WINDER_H
