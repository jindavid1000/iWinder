#ifndef TRAVERSE_H
#define TRAVERSE_H
//============================================================================
//  traverse.h — 排线控制器
//  职责: 圈数驱动的绕线目标（三角波）+ 位置闭环（编码器 PI / 开环估算兜底）
//        + 运行中的限位安全保护。
//  位置源: 编码器闭环（首选）或舵机开环估算（未装编码器时的兜底）。
//============================================================================
#include <Arduino.h>
#include "state.h"

class TraverseCtl {
public:
    // ===== 任务生命周期 =====
    void beginWinding();    // 绕线开始: 目标锚定到左起始位置
    void stop();            // 停止排线
    void saveForCalib();    // 周期校准前保存目标
    void restoreAfterCalib(); // 校准后恢复目标（不丢绕线进度）

    // ===== 每拍驱动 =====
    // 圈数推进目标三角波。返回 true 表示目标在左端折返（完成一个来回）。
    bool onSpoolTurns(float turns);
    // 闭环控制（RUNNING 状态每拍调用）
    void update(uint32_t now);

    // ===== 状态查询 =====
    float    targetPos() const { return _windTargetPos; }
    uint16_t roundTrips() const { return _roundTrips; }
    void     resetRoundTrips() { _roundTrips = 0; g_state.roundTrips = 0; }

    // ===== 位置源（编码器 / 舵机估算）=====
    static float pos();          // 当前排线位置 mm
    static void  setPos(float);  // 设置位置基准（锚定）
    static bool  encoderMode();

private:
    // 圈数三角波
    float    _windTargetPos = 0;
    float    _windLastTurns = 0;
    bool     _windDirRight  = true;
    bool     _turnsAdvanced = false;

    // 来回计数
    uint16_t _roundTrips = 0;

    // 保护 / 宽限
    uint32_t _windGraceUntilMs = 0;
    uint32_t _staticSinceMs    = 0;
    uint32_t _stillPressMs     = 0;

    // 方向记忆与开环步进
    TraverseDir _travDir = DIR_RIGHT;

    // 编码器连续旋转速度自适应闭环
    uint32_t _piLastMs  = 0;    // 上一调整时刻（100ms 周期）
    int16_t  _trackMag  = 12;   // 当前驱动幅度（速度差微调收敛）

    // 限位消抖（连续两个控制拍都为低才认）
    uint32_t _leftLowSinceMs  = 0;
    uint32_t _rightLowSinceMs = 0;
    bool     leftEndstopStable(uint32_t now);
    bool     rightEndstopStable(uint32_t now);

    void resumeMove(TraverseDir dir);
    void endstopSafety(uint32_t now);
};

extern TraverseCtl g_traverse;

#endif // TRAVERSE_H
