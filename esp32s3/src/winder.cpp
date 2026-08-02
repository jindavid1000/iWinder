#include "winder.h"
#include "motor.h"
#include "servo.h"
#include "sensors.h"
#include "slip.h"
#include "comms.h"
#include "protocol.h"
#include "storage.h"
#include "state.h"
#include "config.h"

Winder g_winder;

// RPM 脉冲窗口
static uint32_t s_lastSpoolPulses = 0;
static uint32_t s_lastIdlerPulses = 0;

// Endstop 触发标志（ISR 设置）
static volatile bool s_endstopHit = false;       // 左限位
static volatile bool s_endstopHitRight = false;  // 右限位
static void IRAM_ATTR onEndstopHit(bool isRight) {
    if (isRight) s_endstopHitRight = true;
    else         s_endstopHit = true;
}

// ============================================================================
//  初始化
// ============================================================================

void Winder::begin() {
    applyConfig();
    g_sensors.onEndstop(onEndstopHit);
    _lastTickMs = millis();
}

void Winder::applyConfig() {
    const DeviceConfig &c = g_config;

    if (!_hwInited) {
        g_motor.begin(c.pinMotorPwm, MOTOR_PWM_FREQ, MOTOR_RES_BITS);
        g_servo.begin(c.pinServoPwm, SERVO_PWM_FREQ, SERVO_RES_BITS);
        _hwInited = true;
    }

    g_servo.setPulses(c.servoStopPulse, c.servoLeftPulse, c.servoRightPulse, c.servoHomePulse);
    g_servo.setSpeeds(c.servoTraverseSpeedLeft, c.servoTraverseSpeedRight);

    g_sensors.begin(c.pinHallIdler, c.pinHallSpool, c.pinEndstop, c.pinEndstopRight,
                    c.hallDebounceUs, c.endstopDebounceUs);

    g_slip.setParams(c.slipTolerance, c.stallTimeoutS);
    Serial.println("[Winder] 配置已应用");
}

// ============================================================================
//  状态控制
// ============================================================================

void Winder::setState(DeviceState s) {
    if (g_state.state == s) return;
    g_state.state = s;
    Serial.printf("[Winder] 状态: %s\n", stateName(s));
}

void Winder::setError(ErrorCode code, const String &msg) {
    g_motor.stop();
    g_servo.stop();
    g_state.errorCode = code;
    g_state.errorMsg  = msg;
    setState(STATE_ERROR);
    g_protocol.sendError(code, msg);
    Serial.printf("[Winder] 异常: %s - %s\n", errorName(code), msg.c_str());
}

void Winder::startTask(int speedPct) {
    if (g_state.state == STATE_ERROR) return;
    _targetSpeed = speedPct;

    if (g_state.state == STATE_IDLE || g_state.state == STATE_COMPLETED) {
        g_motor.stop();
        g_slip.reset();
        g_state.idlerPulses = 0;
        g_state.spoolPulses = 0;
        s_lastSpoolPulses = 0;
        s_lastIdlerPulses = 0;
        g_state.spoolTurns = 0;
        g_state.idlerTurns = 0;
        g_state.lengthMeasured = 0;
        g_state.lengthTheoretical = 0;
        g_state.roundTrips = 0;
        setState(STATE_HOMING);
        _homingStartMs = millis();
    } else if (g_state.state == STATE_PAUSED) {
        setState(STATE_RUNNING);
    }
}

void Winder::stopTask() {
    g_motor.stop();
    g_servo.stop();
    setState(STATE_IDLE);
}

void Winder::pauseTask() {
    if (g_state.state != STATE_RUNNING) return;
    g_motor.stop();
    g_servo.stop();
    setState(STATE_PAUSED);
}

void Winder::resumeTask() {
    if (g_state.state != STATE_PAUSED) return;
    setState(STATE_RUNNING);
}

void Winder::goHome() {
    if (g_state.state != STATE_IDLE && g_state.state != STATE_PAUSED) return;
    g_motor.stop();
    // 根据当前位置选最近的端点
    float pos = g_servo.getPosition();
    float mid = (g_config.traverseLeftStart + g_config.traverseRightEnd) / 2.0f;
    _homeGoRight = (pos > mid);
    if (_homeGoRight) g_servo.moveRight();
    else              g_servo.moveHome();
    setState(STATE_HOMING);
    _homingStartMs = millis();
}

void Winder::setSpeed(int speedPct) {
    _targetSpeed = speedPct;
    if (g_state.state == STATE_RUNNING) {
        g_motor.setSpeedPct(speedPct);
    }
}

void Winder::clearError() {
    if (g_state.state != STATE_ERROR) return;
    g_state.errorCode = ERR_NONE;
    g_state.errorMsg  = "";
    setState(STATE_IDLE);
}

// ============================================================================
//  主循环
// ============================================================================

void Winder::update() {
    uint32_t now = millis();
    uint32_t dtMs = now - _lastTickMs;
    if (dtMs < 5) return;
    _lastTickMs = now;

    g_motor.update();

    switch (g_state.state) {
        case STATE_HOMING:      doHoming(); break;
        case STATE_POSITIONING: doPositioning(); break;
        case STATE_RUNNING:     doRunning(dtMs); break;
        case STATE_CALIBRATING: doCalibrating(dtMs); break;
        case STATE_COMPLETED:   doCompleted(); break;
        default: break;
    }

    g_state.uptimeSec = millis() / 1000;

    if (now - _lastReportMs >= g_config.statusReportIntervalMs) {
        _lastReportMs = now;
        reportStatus();
    }

    // 清除非寻原点/校准时的 endstop 事件
    if (g_state.state != STATE_HOMING && g_state.state != STATE_CALIBRATING) {
        s_endstopHit = false;
        s_endstopHitRight = false;
    }
}

// ============================================================================
//  寻原点
// ============================================================================

void Winder::doHoming() {
    bool hit = _homeGoRight ? s_endstopHitRight : s_endstopHit;

    if (hit) {
        if (_homeGoRight) {
            s_endstopHitRight = false;
            g_servo.stop();
            g_servo.setPosition(g_config.traverseRightEnd);
            Serial.println("[Winder] 右原点已确认");
        } else {
            s_endstopHit = false;
            g_servo.stop();
            g_servo.resetPosition();
            Serial.println("[Winder] 左原点已确认");
        }
        _bootHoming = false;

        if (_targetSpeed > 0) {
            setState(STATE_POSITIONING);
        } else {
            setState(STATE_IDLE);
        }
    }

    if ((millis() - _homingStartMs) > (uint32_t)(HOMING_TIMEOUT_S * 1000)) {
        g_servo.stop();
        if (_bootHoming) {
            _bootHoming = false;
            Serial.println("[Winder] 开机寻原点超时（传感器未连接?），继续待机");
            setState(STATE_IDLE);
        } else {
            setError(ERR_HOMING_FAILED, "寻原点超时");
        }
    }
}

// ============================================================================
//  定位到起始位置
// ============================================================================

void Winder::doPositioning() {
    float target = g_config.traverseLeftStart;
    float pos = g_servo.getPosition();

    if (pos >= target - 0.5f && pos <= target + 0.5f) {
        g_servo.stop();
        g_state.calibCountdown = g_config.calIntervalRounds;
        _roundTrips = 0;
        g_motor.setSpeedPct(_targetSpeed);
        g_state.runStartMs = millis();
        g_servo.moveRight();
        setState(STATE_RUNNING);
        Serial.println("[Winder] 开始绕线");
    } else if (pos < target) {
        g_servo.moveRight();
    } else {
        g_servo.moveLeft();
    }
}

// ============================================================================
//  绕线运行
// ============================================================================

void Winder::doRunning(uint32_t dtMs) {
    uint32_t newSpool = g_sensors.getSpoolPulsesAndReset();
    uint32_t newIdler = g_sensors.getIdlerPulsesAndReset();

    g_state.spoolPulses += newSpool;
    g_state.idlerPulses += newIdler;

    g_slip.onSpoolPulses(newSpool);
    g_slip.onIdlerPulses(newIdler);

    float dtSec = dtMs / 1000.0f;
    g_state.spoolTurns      = (float)g_state.spoolPulses / g_config.hallSpoolMagnets;
    g_state.idlerTurns      = (float)g_state.idlerPulses / g_config.hallIdlerMagnets;
    g_state.lengthMeasured    = g_slip.getLengthMeasured();
    g_state.lengthTheoretical = g_slip.getLengthTheoretical();
    g_state.effectiveDiameter = g_slip.getEffectiveDiameter();
    g_state.currentLayer      = g_slip.getCurrentLayer();
    g_state.currentSpeedPct   = g_motor.getCurrentSpeedPct();

    if (newSpool > 0 && dtSec > 0) {
        g_state.spoolRpm = (newSpool / (float)g_config.hallSpoolMagnets) / dtSec * 60.0f;
    }

    processTraverse(dtMs);
    processSlipCheck(dtSec);
    processAutoStop();
}

void Winder::processTraverse(uint32_t dtMs) {
    g_servo.updatePosition(dtMs);
    g_state.traversePos = g_servo.getPosition();
    g_state.traverseDir = g_servo.getDirection();

    float pos   = g_servo.getPosition();
    float right = g_config.traverseRightEnd;
    float left  = g_config.traverseLeftStart;
    float mid   = (left + right) / 2.0f;

    // 右端换向
    if (g_servo.getDirection() == DIR_RIGHT && pos >= right) {
        g_servo.moveLeft();
        Serial.printf("[Winder] 右端换向 pos=%.1f\n", pos);
    }

    // 左端换向
    if (g_servo.getDirection() == DIR_LEFT && pos <= left) {
        g_servo.moveRight();
        _roundTrips++;
        g_state.roundTrips = _roundTrips;
        g_state.calibCountdown = (g_config.calIntervalRounds > _roundTrips)
                                ? (g_config.calIntervalRounds - _roundTrips) : 0;
        Serial.printf("[Winder] 左端换向 pos=%.1f, 来回=%d, 距校准=%d\n",
                      pos, _roundTrips, g_state.calibCountdown);

        // 周期性校准 — 去最近的 endstop
        if (_roundTrips >= g_config.calIntervalRounds) {
            setState(STATE_CALIBRATING);
            g_motor.stop();
            _calibGoRight = (pos > mid);
            _calibReturning = false;
            if (_calibGoRight) {
                g_servo.moveRight();
                Serial.println("[Winder] 校准: 去右限位");
            } else {
                g_servo.moveHome();
                Serial.println("[Winder] 校准: 去左限位");
            }
        }
    }
}

void Winder::processSlipCheck(float dtSec) {
    ErrorCode err = g_slip.check(dtSec);
    if (err != ERR_NONE) {
        String msg;
        switch (err) {
            case ERR_SLIP:  msg = "检测到打滑"; break;
            case ERR_STALL: msg = "卡线/停转"; break;
            case ERR_BREAK: msg = "断线"; break;
            default: msg = "未知异常"; break;
        }
        setError(err, msg);
    }
}

void Winder::processAutoStop() {
    if ((millis() - g_state.runStartMs) / 1000 > (uint32_t)MAX_RUNTIME_S) {
        g_motor.stop();
        g_servo.stop();
        setState(STATE_COMPLETED);
        return;
    }

    switch (g_config.autoStopMode) {
        case 1:
            if (g_config.targetLengthM > 0 &&
                g_state.lengthMeasured >= g_config.targetLengthM) {
                g_motor.stop();
                g_servo.stop();
                setState(STATE_COMPLETED);
            }
            break;
        case 2:
            if (g_config.targetTurns > 0 &&
                g_state.spoolTurns >= g_config.targetTurns) {
                g_motor.stop();
                g_servo.stop();
                setState(STATE_COMPLETED);
            }
            break;
        default: break;
    }

    float warnDia = g_config.spoolOuterDiameter * g_config.fullLoadWarnPct / 100.0f;
    if (g_state.effectiveDiameter > warnDia) {
        Serial.printf("[Winder] 满载预警: 有效直径 %.1fmm > 阈值 %.1fmm\n",
                      g_state.effectiveDiameter, warnDia);
    }
}

// ============================================================================
//  周期性校准（双 Endstop）
// ============================================================================

void Winder::doCalibrating(uint32_t dtMs) {
    g_servo.updatePosition(dtMs);
    g_state.traversePos = g_servo.getPosition();

    if (!_calibReturning) {
        // 阶段 1: 前往目标限位
        bool hit = _calibGoRight ? s_endstopHitRight : s_endstopHit;
        if (hit) {
            if (_calibGoRight) {
                s_endstopHitRight = false;
                g_servo.setPosition(g_config.traverseRightEnd);
                Serial.println("[Winder] 右限位触发，位置校正");
            } else {
                s_endstopHit = false;
                g_servo.resetPosition();
                Serial.println("[Winder] 左限位触发，位置校正");
            }
            _calibReturning = true;
            // 朝起始位置移动
            if (g_servo.getPosition() < g_config.traverseLeftStart) g_servo.moveRight();
            else g_servo.moveLeft();
        }
    } else {
        // 阶段 2: 返回起始位置
        float pos = g_servo.getPosition();
        float target = g_config.traverseLeftStart;
        if (pos >= target - 0.5f && pos <= target + 0.5f) {
            g_servo.stop();
            _roundTrips = 0;
            g_state.roundTrips = 0;
            g_state.calibCountdown = g_config.calIntervalRounds;
            _calibReturning = false;
            g_motor.setSpeedPct(_targetSpeed);
            g_servo.moveRight();
            setState(STATE_RUNNING);
            Serial.println("[Winder] 校准完成，恢复绕线");
        }
    }
}

// ============================================================================
//  任务完成
// ============================================================================

void Winder::doCompleted() {
    static bool homingAfterComplete = false;
    if (!homingAfterComplete) {
        homingAfterComplete = true;
        // 选最近的端点归位
        float pos = g_servo.getPosition();
        float mid = (g_config.traverseLeftStart + g_config.traverseRightEnd) / 2.0f;
        _homeGoRight = (pos > mid);
        if (_homeGoRight) g_servo.moveRight();
        else              g_servo.moveHome();
        _homingStartMs = millis();
        Serial.println("[Winder] 任务完成，归位中...");
    }

    bool hit = _homeGoRight ? s_endstopHitRight : s_endstopHit;
    if (hit) {
        if (_homeGoRight) {
            s_endstopHitRight = false;
            g_servo.setPosition(g_config.traverseRightEnd);
        } else {
            s_endstopHit = false;
            g_servo.resetPosition();
        }
        g_servo.stop();
        homingAfterComplete = false;
        Serial.println("[Winder] 归位完成");
    }

    if ((millis() - _homingStartMs) > (uint32_t)(HOMING_TIMEOUT_S * 1000)) {
        g_servo.stop();
        homingAfterComplete = false;
        Serial.println("[Winder] 归位超时，强制完成");
    }
}

// ============================================================================
//  状态上报
// ============================================================================

void Winder::reportStatus() {
    g_state.activeLink = g_comms.activeLink();
    g_protocol.sendStatus();
}
