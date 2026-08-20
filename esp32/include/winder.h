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

    // 排线位置反馈: 0=舵机开环估算 1=AS5600 编码器闭环
    bool encMode() const { return g_config.traverseEncoder == 1; }
    float travPos();               // 当前排线位置（按模式选源）
    void  setTravPos(float pos);   // 设置位置基准（双源同步）

private:
    uint32_t _lastTickMs   = 0;
    uint32_t _lastReportMs = 0;
    bool     _hwInited     = false;
    bool     _encInited    = false;
    float    _encRevsAtLeft = 0;   // 编码器比例标定: 左限位处的圈数基准

    // RPM: 脉冲间隔法（低速稳定）+ 无脉冲超时判停
    uint32_t _lastPulseMs  = 0;   // 最近一次霍尔脉冲时间（0=从未）

    // 排线状态
    uint16_t _roundTrips   = 0;    // 当前周期来回数
    uint32_t _homingStartMs = 0;
    bool     _bootHoming   = true;  // 开机寻原点（超时软处理）

    // 校准目标（双 Endstop）
    uint32_t _calibStartMs  = 0;   // 校准开始时间（超时保护）
    float    _calibSavedTarget   = 0;   // 校准前的绕线目标（完成后恢复）
    bool     _calibSavedDirRight = true;
    bool     _homeGoRight   = false;  // 寻原点/归位方向

    // 舵机速度标定
    enum CalibPhase : uint8_t {
        SCALIB_HOME,        // 归位左限位
        SCALIB_GO_RIGHT,    // 满速右行计时
        SCALIB_GO_LEFT,     // 满速左行计时
        SCALIB_SLOW_RIGHT,  // 低速右行计时（测非线性指数）
        SCALIB_SLOW_LEFT,   // 低速左行计时（测非线性指数）
        SCALIB_DONE
    };
    CalibPhase _scalibPhase = SCALIB_HOME;
    uint8_t    _scalibRound = 0;          // 当前来回
    uint32_t   _scalibMoveStartMs = 0;    // 满速运动起点时间
    uint32_t   _scalibTimeoutMs   = 0;    // 超时截止
    float      _scalibSpeedRightSum = 0;
    float      _scalibSpeedLeftSum  = 0;
    float      _scalibSlowRight = 0;      // 低速(40%)实测速度
    float      _scalibSlowLeft  = 0;
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

    // 低速时间切片（舵机死区补偿）
    uint32_t _sliceStartMs = 0;   // 切片周期起点
    uint32_t _stillPressMs = 0;   // 静止状态下限位被压起始时间（中位偏移检测）

    // 编码器闭环 PI 速度控制
    float    _piInteg  = 0;       // 积分项（%偏移）
    uint32_t _piLastMs = 0;       // 上次控制周期时间

    // 圈数驱动的排线目标位置（三角波：每圈 +1 线径，到边界折返）
    float    _windTargetPos  = 0;
    float    _windLastTurns  = 0;   // 上次推进目标时已记录的料盘圈数
    bool     _windDirRight   = true;
    bool     _turnsAdvanced  = false;      // 本 tick 圈数是否增长
    uint32_t _windGraceUntilMs = 0;        // 校准/启动后的回归宽限期
    uint32_t _staticSinceMs   = 0;         // 料盘静止起始时间（停转追赶判定用）

    void resumeTraverse();        // 按记忆方向恢复排线运动
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
