#include "traverse.h"
#include "servo.h"
#include "sensors.h"
#include "encoder.h"
#include "config.h"
#include "winder.h"

TraverseCtl g_traverse;

// ============================================================================
//  位置源
// ============================================================================

bool TraverseCtl::encoderMode() {
    return g_config.traverseEncoder == 1;
}

float TraverseCtl::pos() {
    return (encoderMode() && g_encoder.ok()) ? g_encoder.posMm()
                                             : g_servo.getPosition();
}

void TraverseCtl::setPos(float p) {
    g_servo.setPosition(p);          // 估算源同步，保证可回退
    if (encoderMode()) g_encoder.setPosMm(p);
}

// ============================================================================
//  任务生命周期
// ============================================================================

void TraverseCtl::beginWinding() {
    _windTargetPos = g_config.traverseLeftStart;
    _windDirRight  = true;
    _windLastTurns = g_state.spoolTurns;
    _roundTrips    = 0;
    g_state.roundTrips = 0;
    _windGraceUntilMs = millis() + 10000;   // 定位收敛宽限
    _staticSinceMs = 0;
    _piInteg = 0;
}

void TraverseCtl::stop() {
    if (g_servo.getDirection() != DIR_NONE) {
        _travDir = g_servo.getDirection();
        g_servo.stop();
    }
}

void TraverseCtl::saveForCalib() {
    // 目标与方向由成员天然保存，无需额外快照
}

void TraverseCtl::restoreAfterCalib() {
    _windLastTurns = g_state.spoolTurns;   // 校准期间圈数不推进目标，重新对齐
    float L = g_config.traverseLeftStart, R = g_config.traverseRightEnd;
    if (_windTargetPos > R) _windTargetPos = R;
    if (_windTargetPos < L) _windTargetPos = L;
    _windGraceUntilMs = millis() + 15000;  // 从限位返回原位置的宽限
    _travDir = _windDirRight ? DIR_RIGHT : DIR_LEFT;
    _piInteg = 0;
}

// ============================================================================
//  圈数驱动的目标三角波
// ============================================================================

bool TraverseCtl::onSpoolTurns(float turns) {
    bool leftFold = false;
    _turnsAdvanced = (turns != _windLastTurns);
    if (!_turnsAdvanced) return false;

    float d = (turns - _windLastTurns) * g_config.filamentDiameter;
    _windLastTurns = turns;
    _windTargetPos += _windDirRight ? d : -d;

    float L = g_config.traverseLeftStart;
    float R = g_config.traverseRightEnd;
    if (_windTargetPos >= R) {
        _windTargetPos = R - (_windTargetPos - R);
        _windDirRight = false;
    } else if (_windTargetPos <= L) {
        _windTargetPos = L + (L - _windTargetPos);
        _windDirRight = true;
        _roundTrips++;
        g_state.roundTrips = _roundTrips;
        g_state.calibCountdown = (g_config.calIntervalRounds > _roundTrips)
                               ? (g_config.calIntervalRounds - _roundTrips) : 0;
        leftFold = true;
    }
    return leftFold;
}

// ============================================================================
//  闭环控制
// ============================================================================


// 限位稳定判定: 连续 ≥5ms 为低才认（过滤舵机电流瞬态打出的假脉冲）。
// update() 每拍 ≥5ms 调用一次，连续两拍为低即满足。
bool TraverseCtl::leftEndstopStable(uint32_t now) {
    if (g_sensors.isLeftEndstopPressed()) {
        if (_leftLowSinceMs == 0) { _leftLowSinceMs = now; return false; }
        return (now - _leftLowSinceMs) >= 5;
    }
    _leftLowSinceMs = 0;
    return false;
}

bool TraverseCtl::rightEndstopStable(uint32_t now) {
    if (g_sensors.isRightEndstopPressed()) {
        if (_rightLowSinceMs == 0) { _rightLowSinceMs = now; return false; }
        return (now - _rightLowSinceMs) >= 5;
    }
    _rightLowSinceMs = 0;
    return false;
}

void TraverseCtl::resumeMove(TraverseDir dir) {
    if (g_servo.getDirection() == dir) return;
    if (dir == DIR_LEFT) g_servo.moveLeft();
    else                 g_servo.moveRight();
}

void TraverseCtl::update(uint32_t now) {
    g_servo.updatePosition(5);   // 开环估算源（编码器模式下仅作回退，不参与控制）
    g_state.traversePos = pos();
    g_state.traverseDir = g_servo.getDirection();

    float posNow = pos();
    float err = _windTargetPos - posNow;

    // ---- 诊断 ----
    static uint32_t lastDbgMs = 0;
    if (now - lastDbgMs >= 1000) {
        lastDbgMs = now;
        Serial.printf("[TRA] pos=%.2f tgt=%.2f err=%.2f dir=%s turns=%.1f\n",
                      posNow, _windTargetPos, err,
                      g_servo.getDirection() == DIR_LEFT  ? "L" :
                      g_servo.getDirection() == DIR_RIGHT ? "R" : "-",
                      g_state.spoolTurns);
    }

    // ---- 静止异常保护 ----
    // 料盘静止超 3 秒仍未追上目标（偏差>2mm）→ 坐标链异常（方向/限位镜像等）。
    // 高速停手后目标领先数毫米属正常追赶，3 秒窗口避免误杀。
    if (!_turnsAdvanced && fabsf(err) > 2.0f && now > _windGraceUntilMs) {
        if (_staticSinceMs == 0) _staticSinceMs = now;
        if (now - _staticSinceMs > 3000) {
            stop();
            g_winder.reportFault("排线位置与目标偏差过大且料盘静止，疑似方向/限位接线镜像");
            return;
        }
    } else {
        _staticSinceMs = 0;
    }

    if (fabsf(err) < 0.15f) {
        // 已到位: 暂停等待目标推进（料盘慢/停时排线也停，位置不漂移）
        stop();
        // 静止时仍顶限位 → 舵机中位偏移（停止脉宽下仍在缓慢转动）
        if (g_sensors.isLeftEndstopPressed() || g_sensors.isRightEndstopPressed()) {
            if (_stillPressMs == 0) _stillPressMs = now;
            else if (now - _stillPressMs > 2000) {
                stop();
                g_winder.reportFault("排线静止时仍顶限位: 舵机中位偏移，请微调「停止PWM」直至静止");
                return;
            }
        } else {
            _stillPressMs = 0;
        }
        endstopSafety(now);
        return;
    }

    TraverseDir wantDir = (err > 0) ? DIR_RIGHT : DIR_LEFT;
    _travDir = wantDir;

    if (encoderMode() && g_encoder.ok()) {
        // ===== 编码器闭环: PI 速度控制 =====
        // 目标速度 = 圈数前馈(转速×线径) + 位置误差修正；实测速度来自编码器。
        float full = (wantDir == DIR_LEFT) ? g_config.servoTraverseSpeedLeft
                                           : g_config.servoTraverseSpeedRight;
        if (full < 0.1f) full = 80.0f;

        float vTarget = 0;
        if (fabsf(err) > 0.15f) {
            float ff = (g_state.spoolRpm / 60.0f) * g_config.filamentDiameter;
            vTarget = ff + fabsf(err) * 1.5f;
            if (vTarget > full) vTarget = full;
            if (wantDir == DIR_LEFT) vTarget = -vTarget;
        }

        // 40ms 零阶保持 + 斜率限制: 舵机是开关型对象（30% 偏移≈满速），
        // 无保持的 PI 会在目标低速时输出高频翻转（±70% 抖动）→ 舵机电流
        // 剧烈斩波 → EMI 打到限位线产生假触发。限速后输出必经 0 再换向。
        if (now - _piLastMs < 40) {
            g_servo.drive((int16_t)_piLastOut);   // 保持上一输出
        } else {
            float dt = (now - _piLastMs) / 1000.0f;
            if (dt > 0.2f) dt = 0.2f;
            _piLastMs = now;

            float vMeas = g_encoder.getSpeedMmPerS();
            float vErr = vTarget - vMeas;
            _piInteg += vErr * dt * 2.5f;
            if (_piInteg >  25.0f) _piInteg =  25.0f;
            if (_piInteg < -25.0f) _piInteg = -25.0f;
            // 抗饱和: 实测速度已超目标时泄放积分
            if (fabsf(vMeas) > fabsf(vTarget) + 2.0f) _piInteg *= 0.8f;
            if (vTarget == 0 && fabsf(err) < 0.3f) _piInteg = 0;

            float out = vErr * 3.0f + _piInteg;
            if (out >  70.0f) out =  70.0f;
            if (out < -70.0f) out = -70.0f;
            if (fabsf(vTarget) < 0.05f && fabsf(err) < 0.3f) out = 0;
            if (fabsf(out) < 5.0f && out != 0) out = 0;   // 输出死区

            // 斜率限制: 每个控制周期最多变化 ±30%（换向必经 0，消除电流斩波）
            if (out > _piLastOut + 30.0f) out = _piLastOut + 30.0f;
            if (out < _piLastOut - 30.0f) out = _piLastOut - 30.0f;
            _piLastOut = out;

            g_servo.drive((int16_t)out);
        }
    } else {
        // ===== 开环估算兜底: 迟滞步进 =====
        // 连续旋转舵机低速近乎开关特性，比例调速不可行:
        // 误差 >1mm 以固定偏移走，收敛到 0.3mm 内停。
        if (fabsf(err) > 1.0f) {
            resumeMove(wantDir);
            g_servo.setSpeedFraction(SERVO_MIN_FRAC);
        } else if (fabsf(err) < 0.3f) {
            stop();
        }
    }

    endstopSafety(now);
}

// ============================================================================
//  运行中的限位安全保护
// ============================================================================

void TraverseCtl::endstopSafety(uint32_t now) {
    if (g_servo.getDirection() == DIR_RIGHT && rightEndstopStable(now)) {
        // 位置合理性: 右限位锚定只在排线确实接近右端时接受，
        // 中途位置出现"限位"必为干扰，忽略并记录
        static uint32_t lastIgnR = 0;
        if (pos() < g_config.travelRangeMm - 3.0f) {
            if (now - lastIgnR > 1000) {   // 限流: 持续误压时每秒只提示一次
                lastIgnR = now;
                Serial.printf("[Winder] 持续忽略右限位信号（排线 %.1fmm 处被压/线路异常）\n", pos());
            }
            return;
        }
        g_servo.stop();
        setPos(g_config.travelRangeMm);
        _windDirRight = false;
        if (_windTargetPos > g_config.traverseRightEnd) _windTargetPos = g_config.traverseRightEnd;
        _windGraceUntilMs = now + 10000;   // 位置校正后长距离回退的宽限
        Serial.printf("[Winder] 右限位触发换向 pos=%.1f\n", pos());
    } else if (g_servo.getDirection() == DIR_LEFT && leftEndstopStable(now)) {
        static uint32_t lastIgnL = 0;
        if (pos() > 3.0f) {
            if (now - lastIgnL > 1000) {
                lastIgnL = now;
                Serial.printf("[Winder] 持续忽略左限位信号（排线 %.1fmm 处被压/线路异常）\n", pos());
            }
            return;
        }
        g_servo.stop();
        setPos(0);
        _windDirRight = true;
        if (_windTargetPos < g_config.traverseLeftStart) _windTargetPos = g_config.traverseLeftStart;
        _windGraceUntilMs = now + 10000;
        Serial.printf("[Winder] 左限位触发换向 pos=%.1f\n", pos());
    }
}
