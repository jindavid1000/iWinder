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
    void startServoCalib();      // 舵机速度自动标定

    // 配置变更后重新初始化硬件
    void applyConfig();

    // 驱动模式: 0=电动 1=手动(手摇，电机不输出)
    bool isManualMode() const { return g_config.driveMode == 1; }

private:
    uint32_t _lastTickMs   = 0;
    uint32_t _lastReportMs = 0;
    bool     _hwInited     = false;

    // RPM 固定窗口统计
    uint32_t _rpmWinStartMs = 0;   // 当前窗口起始时间
    uint32_t _rpmWinPulses  = 0;   // 当前窗口内累计脉冲

    // 排线状态
    uint16_t _roundTrips   = 0;    // 当前周期来回数
    uint32_t _homingStartMs = 0;
    bool     _bootHoming   = true;  // 开机寻原点（超时软处理）

    // 校准目标（双 Endstop）
    bool     _calibGoRight  = false;  // true = 去右限位
    bool     _calibReturning = false; // 限位已触发，正在返回起始位置
    bool     _homeGoRight   = false;  // 寻原点/归位方向

    // 舵机速度标定
    enum CalibPhase : uint8_t {
        SCALIB_HOME,        // 归位左限位
        SCALIB_GO_RIGHT,    // 满速右行计时
        SCALIB_GO_LEFT,     // 满速左行计时
        SCALIB_DONE
    };
    CalibPhase _scalibPhase = SCALIB_HOME;
    uint8_t    _scalibRound = 0;          // 当前来回
    uint32_t   _scalibMoveStartMs = 0;    // 满速运动起点时间
    uint32_t   _scalibTimeoutMs   = 0;    // 超时截止
    float      _scalibSpeedRightSum = 0;
    float      _scalibSpeedLeftSum  = 0;
    float      _scalibDist = 0;           // 标定距离 mm
    float      _scalibFirstRight = 0;     // 第一轮速度（合理性判断）
    float      _scalibFirstLeft  = 0;

    // 速度
    int      _targetSpeed  = 0;
    float    _smoothRpm    = 0;  // RPM EMA 平滑值

    // 手动模式: 停转触发校准
    bool     _manualSeenSpinning = false;  // 本周期内检测到手摇转起过
    uint32_t _manualZeroSinceMs  = 0;      // RPM 归零起始时间（0=正在转）

    // 电动模式: 缠料检测（电机运转但料盘停转）
    uint32_t _jamStallMs = 0;              // 停转起始时间（0=正常）

    // 排线方向记忆（料盘停转舵机暂停后，恢复时用于继续原方向）
    TraverseDir _travDir = DIR_RIGHT;

    void enterCalibrating();

    void setState(DeviceState s);
    void setError(ErrorCode code, const String &msg);

    void doHoming();
    void doPositioning();
    void doRunning(uint32_t dtMs);
    void doCalibrating(uint32_t dtMs);
    void doCompleted();
    void doServoCalib();

    void processTraverse(uint32_t dtMs);
    void processAutoStop();
    void reportStatus();
};

extern Winder g_winder;

#endif // WINDER_H
