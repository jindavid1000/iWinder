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

// ============================================================================
//  初始化
// ============================================================================

void Winder::begin() {
    applyConfig();
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

    g_sensors.begin(c.pinHallSpool, c.pinEndstop, c.pinEndstopRight,
                    c.hallDebounceUs, c.endstopDebounceUs);

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
        g_state.spoolPulses = 0;
        s_lastSpoolPulses = 0;
        g_state.spoolTurns = 0;
        g_state.lengthTheoretical = 0;
        g_state.roundTrips = 0;
        g_servo.moveRight();  // 回右原点（原点 = 右限位）
        _homeGoRight = true;
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
    _targetSpeed = 0;  // 仅回原点，不开始绕线
    _homeGoRight = true;
    g_servo.moveRight();  // 回右原点（右限位）
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
//  舵机速度自动标定（需要双 Endstop）
// ============================================================================

void Winder::startServoCalib() {
    if (g_state.state != STATE_IDLE) return;

    _scalibDist = TRAVEL_RANGE_MM;  // 左右限位之间的实际物理距离
    if (_scalibDist < 1.0f) {
        setError(ERR_SENSOR, "标定距离异常，检查排线参数");
        return;
    }

    _scalibPhase        = SCALIB_HOME;
    _scalibRound        = 0;
    _scalibSpeedRightSum = 0;
    _scalibSpeedLeftSum  = 0;
    _scalibFirstRight    = 0;
    _scalibFirstLeft     = 0;
    _scalibTimeoutMs     = millis() + (uint32_t)(HOMING_TIMEOUT_S * 1000);

    g_motor.stop();
    g_servo.moveHome();  // 满速左行归位
    setState(STATE_SERVO_CALIB);
    Serial.printf("[Winder] 舵机标定开始，距离=%.1fmm\n", _scalibDist);
}

void Winder::doServoCalib() {

    switch (_scalibPhase) {
        // ---- 阶段 0: 归位左限位 ----
        case SCALIB_HOME:
            if (g_sensors.isLeftEndstopPressed()) {
                g_servo.stop();
                g_servo.resetPosition();
                Serial.println("[标定] 左限位已确认，开始右行");

                // 短暂停顿让机械稳定
                delay(200);
                _scalibMoveStartMs = millis();
                _scalibPhase = SCALIB_GO_RIGHT;
                g_servo.moveRight();  // 满速右行
            }
            break;

        // ---- 阶段 1: 满速右行 ----
        case SCALIB_GO_RIGHT:
            if (g_sensors.isRightEndstopPressed()) {
                g_servo.stop();
                g_servo.setPosition(g_config.traverseRightEnd);

                float dtSec = (millis() - _scalibMoveStartMs) / 1000.0f;
                float speed = _scalibDist / dtSec;
                _scalibSpeedRightSum += speed;

                Serial.printf("[标定] 右行 #%d: %.1fmm / %.2fs = %.1f mm/s\n",
                              _scalibRound + 1, _scalibDist, dtSec, speed);

                if (_scalibRound == 0) {
                    _scalibFirstRight = speed;
                }

                delay(200);
                _scalibMoveStartMs = millis();
                _scalibPhase = SCALIB_GO_LEFT;
                g_servo.moveLeft();  // 满速左行
            }
            break;

        // ---- 阶段 2: 满速左行 ----
        case SCALIB_GO_LEFT:
            if (g_sensors.isLeftEndstopPressed()) {
                g_servo.stop();
                g_servo.resetPosition();

                float dtSec = (millis() - _scalibMoveStartMs) / 1000.0f;
                float speed = _scalibDist / dtSec;
                _scalibSpeedLeftSum += speed;

                Serial.printf("[标定] 左行 #%d: %.1fmm / %.2fs = %.1f mm/s\n",
                              _scalibRound + 1, _scalibDist, dtSec, speed);

                if (_scalibRound == 0) {
                    _scalibFirstLeft = speed;

                    // 第一轮合理性判断
                    if (_scalibFirstRight < 0.5f || _scalibFirstRight > 500.0f ||
                        _scalibFirstLeft  < 0.5f || _scalibFirstLeft  > 500.0f) {
                        setError(ERR_SENSOR, "标定数值异常，检查限位/机械");
                        return;
                    }
                }

                _scalibRound++;
                if (_scalibRound < 3) {
                    delay(200);
                    _scalibMoveStartMs = millis();
                    _scalibPhase = SCALIB_GO_RIGHT;
                    g_servo.moveRight();
                } else {
                    _scalibPhase = SCALIB_DONE;
                }
            }
            break;

        case SCALIB_DONE:
            break;
    }

    // 完成
    if (_scalibPhase == SCALIB_DONE) {
        float avgRight = _scalibSpeedRightSum / 3.0f;
        float avgLeft  = _scalibSpeedLeftSum  / 3.0f;

        g_config.servoTraverseSpeedRight = avgRight;
        g_config.servoTraverseSpeedLeft  = avgLeft;
        g_storage.saveConfig(g_config);
        g_servo.setSpeeds(avgLeft, avgRight);

        Serial.printf("[标定] 完成! 右行=%.1f 左行=%.1f mm/s\n", avgRight, avgLeft);

        // 发送标定结果
        String msg = "{\"type\":\"servo_calib_result\",\"speed_right\":"
                   + String(avgRight, 1) + ",\"speed_left\":"
                   + String(avgLeft, 1) + "}\n";
        g_comms.send(msg);

        setState(STATE_IDLE);
        return;
    }

    // 超时
    if (millis() > _scalibTimeoutMs) {
        g_servo.stop();
        String hint = (_scalibPhase == SCALIB_HOME || _scalibPhase == SCALIB_GO_LEFT)
                      ? "左限位未触发" : "右限位未触发（可能未安装？需手动标定）";
        setError(ERR_SENSOR, "舵机标定超时: " + hint);
    }
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
        case STATE_SERVO_CALIB: doServoCalib(); break;
        case STATE_COMPLETED:   doCompleted(); break;
        default: break;
    }

    g_state.uptimeSec = millis() / 1000;

    if (now - _lastReportMs >= g_config.statusReportIntervalMs) {
        _lastReportMs = now;
        reportStatus();
    }
}

// ============================================================================
//  寻原点
// ============================================================================

void Winder::doHoming() {
    bool hit = _homeGoRight ? g_sensors.isRightEndstopPressed()
                            : g_sensors.isLeftEndstopPressed();

    if (hit) {
        if (_homeGoRight) {
            g_servo.stop();
            g_servo.setPosition(g_config.traverseRightEnd);
            Serial.println("[Winder] 右原点已确认（原点 = 右限位）");
        } else {
            g_servo.stop();
            g_servo.resetPosition();
            Serial.println("[Winder] 左限位已确认");
        }
        _bootHoming = false;

        // 总是进入定位阶段
        setState(STATE_POSITIONING);
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
    g_servo.updatePosition(5);  // 更新位置估算
    float pos = g_servo.getPosition();

    // 安全保护：碰到限位立刻停
    if (g_servo.getDirection() == DIR_RIGHT && g_sensors.isRightEndstopPressed()) {
        g_servo.stop();
        g_servo.setPosition(g_config.traverseRightEnd);
        Serial.println("[Winder] 定位中触碰右限位");
    } else if (g_servo.getDirection() == DIR_LEFT && g_sensors.isLeftEndstopPressed()) {
        g_servo.stop();
        g_servo.resetPosition();
        Serial.println("[Winder] 定位中触碰左限位");
    }

    pos = g_servo.getPosition();
    if (pos >= target - 0.5f && pos <= target + 0.5f) {
        g_servo.stop();
        if (_targetSpeed > 0) {
            // 有速度 → 开始绕线，从起始位置向右移动
            g_state.calibCountdown = g_config.calIntervalRounds;
            _roundTrips = 0;
            g_motor.setSpeedPct(_targetSpeed);
            g_state.runStartMs = millis();
            g_servo.moveRight();
            setState(STATE_RUNNING);
            Serial.println("[Winder] 开始绕线");
        } else {
            // 无速度 → 仅回原点，待机
            setState(STATE_IDLE);
            Serial.println("[Winder] 已回到起始位置，待机");
        }
    } else if (pos > target) {
        // 在右边 → 向左移动到起始位置
        if (g_servo.getDirection() != DIR_LEFT) g_servo.moveLeft();
    } else {
        // 在左边 → 向右移动到起始位置
        if (g_servo.getDirection() != DIR_RIGHT) g_servo.moveRight();
    }
}

// ============================================================================
//  绕线运行
// ============================================================================

void Winder::doRunning(uint32_t dtMs) {
    uint32_t newSpool = g_sensors.getSpoolPulsesAndReset();

    g_state.spoolPulses += newSpool;

    g_slip.onSpoolPulses(newSpool);

    float dtSec = dtMs / 1000.0f;
    g_state.spoolTurns      = (float)g_state.spoolPulses / g_config.hallSpoolMagnets;
    g_state.lengthTheoretical = g_slip.getLengthTheoretical();
    g_state.effectiveDiameter = g_slip.getEffectiveDiameter();
    g_state.currentLayer      = g_slip.getCurrentLayer();
    g_state.currentSpeedPct   = g_motor.getCurrentSpeedPct();

    if (newSpool > 0 && dtSec > 0) {
        g_state.spoolRpm = (newSpool / (float)g_config.hallSpoolMagnets) / dtSec * 60.0f;
    }

    // 调试：每 2 秒打印一次脉冲/计数/配置状态
    static uint32_t lastDbgMs = 0;
    if (millis() - lastDbgMs > 2000) {
        lastDbgMs = millis();
        Serial.printf("[DBG] pulses=%u new=%u totalSpool=%u hallMagnets=%u pinHall=%u\n",
                      g_state.spoolPulses, newSpool,
                      g_sensors.getTotalSpoolPulses(),
                      g_config.hallSpoolMagnets, g_config.pinHallSpool);
    }

    processTraverse(dtMs);
    processAutoStop();
}

void Winder::processTraverse(uint32_t dtMs) {
    g_servo.updatePosition(dtMs);
    g_state.traversePos = g_servo.getPosition();
    g_state.traverseDir = g_servo.getDirection();

    float pos   = g_servo.getPosition();
    float right = g_config.traverseRightEnd;
    float left  = g_config.traverseLeftStart;

    // ===== 根据 RPM 计算舵机速度比例 =====
    // 目标：料盘每转一圈，排线移动一个线径的距离
    // 所需排线速度 (mm/s) = RPM/60 * filamentDiameter
    // 舵机满速速度 (mm/s) = servoTraverseSpeed
    // 速度比例 = 所需速度 / 满速速度
    float rpm = g_state.spoolRpm;
    float neededSpeed = (rpm / 60.0f) * g_config.filamentDiameter;
    float servoFullSpeed = (g_servo.getDirection() == DIR_LEFT) ? g_config.servoTraverseSpeedLeft
                                                                  : g_config.servoTraverseSpeedRight;
    if (servoFullSpeed > 0.1f && neededSpeed > 0) {
        float frac = neededSpeed / servoFullSpeed;
        g_servo.setSpeedFraction(frac);
    }

    // 物理限位安全保护（优先于位置估算换向）
    if (g_servo.getDirection() == DIR_RIGHT && g_sensors.isRightEndstopPressed()) {
        g_servo.moveLeft();
        g_servo.setPosition(right);
        Serial.printf("[Winder] 右限位触发换向 pos=%.1f\n", pos);
    } else if (g_servo.getDirection() == DIR_LEFT && g_sensors.isLeftEndstopPressed()) {
        g_servo.moveRight();
        g_servo.setPosition(left);
        _roundTrips++;
        g_state.roundTrips = _roundTrips;
        g_state.calibCountdown = (g_config.calIntervalRounds > _roundTrips)
                                ? (g_config.calIntervalRounds - _roundTrips) : 0;
        Serial.printf("[Winder] 左限位触发换向 pos=%.1f, 来回=%d\n", pos, _roundTrips);

        if (_roundTrips >= g_config.calIntervalRounds) {
            setState(STATE_CALIBRATING);
            g_motor.stop();
            _calibGoRight = true;  // 校准去右限位（原点）
            _calibReturning = false;
            g_servo.moveRight();
            Serial.println("[Winder] 校准: 去右限位（原点）");
            return;
        }
    } else {
        // 位置估算换向（兜底）
        if (g_servo.getDirection() == DIR_RIGHT && pos >= right) {
            g_servo.moveLeft();
            Serial.printf("[Winder] 右端换向 pos=%.1f\n", pos);
        }

        if (g_servo.getDirection() == DIR_LEFT && pos <= left) {
            g_servo.moveRight();
            _roundTrips++;
            g_state.roundTrips = _roundTrips;
            g_state.calibCountdown = (g_config.calIntervalRounds > _roundTrips)
                                    ? (g_config.calIntervalRounds - _roundTrips) : 0;
            Serial.printf("[Winder] 左端换向 pos=%.1f, 来回=%d, 距校准=%d\n",
                          pos, _roundTrips, g_state.calibCountdown);

            if (_roundTrips >= g_config.calIntervalRounds) {
                setState(STATE_CALIBRATING);
                g_motor.stop();
                _calibGoRight = true;  // 校准去右限位（原点）
                _calibReturning = false;
                g_servo.moveRight();
                Serial.println("[Winder] 校准: 去右限位（原点）");
            }
        }
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
                g_state.lengthTheoretical >= g_config.targetLengthM) {
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
        bool hit = _calibGoRight ? g_sensors.isRightEndstopPressed()
                                 : g_sensors.isLeftEndstopPressed();
        if (hit) {
            if (_calibGoRight) {
                g_servo.setPosition(g_config.traverseRightEnd);
                Serial.println("[Winder] 右限位触发，位置校正");
            } else {
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
        // 始终回右原点
        _homeGoRight = true;
        g_servo.moveRight();
        _homingStartMs = millis();
        Serial.println("[Winder] 任务完成，归位中...");
    }

    bool hit = _homeGoRight ? g_sensors.isRightEndstopPressed()
                            : g_sensors.isLeftEndstopPressed();
    if (hit) {
        if (_homeGoRight) {
            g_servo.setPosition(g_config.traverseRightEnd);
        } else {
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
