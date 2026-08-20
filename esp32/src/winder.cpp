#include "winder.h"
#include "motor.h"
#include "servo.h"
#include "sensors.h"
#include "encoder.h"
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
    g_servo.setSpeedExp(c.servoSpeedExp);

    g_sensors.begin(c.pinHallSpool, c.pinEndstop, c.pinEndstopRight,
                    c.hallDebounceUs, c.endstopDebounceUs);

    // AS5600 编码器（可选闭环）
    if (c.traverseEncoder) {
        if (!_encInited) {
            g_encoder.begin(c.pinEncSda, c.pinEncScl);
            _encInited = true;
        }
        // 比例: 优先用标定值，否则按 丝杆导程 × 齿比 推算
        float mmPerRev = (c.encMmPerRev > 0.1f) ? c.encMmPerRev
                        : c.leadScrewPitch * ((c.encGearRatio > 0.1f) ? c.encGearRatio : 1.0f);
        g_encoder.setMmPerRev(mmPerRev);
        Serial.printf("[Winder] 排线闭环: AS5600, %.2f mm/圈\n", mmPerRev);
    }

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

    // 前置检查: 排线位置估算依赖舵机标定速度，未标定时定位/绕线会
    // 陷入"顶限位"死循环，直接拒绝启动
    if (g_config.servoTraverseSpeedLeft < 0.1f || g_config.servoTraverseSpeedRight < 0.1f) {
        setError(ERR_SENSOR, "舵机速度未标定，请先在 App 参数页执行「舵机速度标定」");
        return;
    }

    _targetSpeed = speedPct;
    _manualSeenSpinning = false;
    _manualZeroSinceMs  = 0;
    _jamStallMs         = 0;

    if (g_state.state == STATE_IDLE || g_state.state == STATE_COMPLETED) {
        g_motor.stop();
        g_slip.reset();
        g_state.spoolPulses = 0;
        s_lastSpoolPulses = 0;
        g_state.spoolTurns = 0;
        g_state.lengthTheoretical = 0;
        g_state.roundTrips = 0;
        _smoothRpm = 0;
        _lastPulseMs = 0;
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
    if (g_state.state == STATE_RUNNING && !isManualMode()) {
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

    _scalibDist = g_config.travelRangeMm;  // 左右限位之间的实际物理距离
    if (_scalibDist < 1.0f) {
        setError(ERR_SENSOR, "标定距离异常，检查排线参数");
        return;
    }

    _scalibPhase        = SCALIB_HOME;
    _scalibRound        = 0;
    _scalibSpeedRightSum = 0;
    _scalibSpeedLeftSum  = 0;
    _scalibSlowRight     = 0;
    _scalibSlowLeft      = 0;
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
                setTravPos(0);
                _encRevsAtLeft = g_encoder.getRevs();   // 编码器比例标定基准
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
                setTravPos(g_config.travelRangeMm);

                // 编码器比例标定: 限位间实际转数 → 每圈位移 mm。
                // 限位安装绝对位置不准没关系（±5mm），触发点重复性才重要。
                if (encMode()) {
                    float revs = g_encoder.getRevs() - _encRevsAtLeft;
                    if (revs > 1.0f) {
                        float mmPerRev = _scalibDist / revs;
                        g_config.encMmPerRev = mmPerRev;
                        g_storage.saveConfig(g_config);
                        g_encoder.setMmPerRev(mmPerRev);
                        setTravPos(g_config.travelRangeMm);   // 用新比例重设基准
                        Serial.printf("[标定] 编码器比例: %.2f mm/圈 (%.1fmm / %.2f圈)\n",
                                      mmPerRev, _scalibDist, revs);
                    }
                }

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
                setTravPos(0);

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
                    // 满速来回完成 → 追加步进速度(SERVO_MIN_FRAC)来回。
                    // 绕线步进固定用该偏移，直接在此工作点标定，估算无需外推。
                    delay(200);
                    g_servo.setSpeedFraction(SERVO_MIN_FRAC);
                    _scalibMoveStartMs = millis();
                    _scalibPhase = SCALIB_SLOW_RIGHT;
                    g_servo.moveRight();
                    Serial.printf("[标定] 满速测量完成，步进速度(%.0f%%)测量中\n", SERVO_MIN_FRAC * 100);
                }
            }
            break;

        // ---- 阶段 3: 低速(40%)右行 ----
        case SCALIB_SLOW_RIGHT:
            if (g_sensors.isRightEndstopPressed()) {
                g_servo.stop();
                setTravPos(g_config.travelRangeMm);
                float dtSec = (millis() - _scalibMoveStartMs) / 1000.0f;
                _scalibSlowRight = _scalibDist / dtSec;
                Serial.printf("[标定] 低速右行: %.1f mm/s\n", _scalibSlowRight);
                delay(200);
                g_servo.setSpeedFraction(SERVO_MIN_FRAC);
                _scalibMoveStartMs = millis();
                _scalibPhase = SCALIB_SLOW_LEFT;
                g_servo.moveLeft();
            }
            break;

        // ---- 阶段 4: 低速(40%)左行 ----
        case SCALIB_SLOW_LEFT:
            if (g_sensors.isLeftEndstopPressed()) {
                g_servo.stop();
                setTravPos(0);
                float dtSec = (millis() - _scalibMoveStartMs) / 1000.0f;
                _scalibSlowLeft = _scalibDist / dtSec;
                Serial.printf("[标定] 低速左行: %.1f mm/s\n", _scalibSlowLeft);
                _scalibPhase = SCALIB_DONE;
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

        // 由满速/低速两档推算幂律指数 k: v = v_full × frac^k → k = ln(v_slow/v_full)/ln(0.4)
        float k = 1.0f;
        if (_scalibSlowRight > 0.5f && _scalibSlowLeft > 0.5f &&
            avgRight > 1.0f && avgLeft > 1.0f) {
            float kR = logf(_scalibSlowRight / avgRight) / logf(SERVO_MIN_FRAC);
            float kL = logf(_scalibSlowLeft  / avgLeft)  / logf(SERVO_MIN_FRAC);
            k = (kR + kL) / 2.0f;
            if (k < 0.05f) k = 0.05f;
            if (k > 3.0f) k = 3.0f;
        }
        g_config.servoSpeedExp = k;
        g_storage.saveConfig(g_config);
        g_servo.setSpeeds(avgLeft, avgRight);
        g_servo.setSpeedExp(k);

        Serial.printf("[标定] 完成! 右行=%.1f 左行=%.1f mm/s, 速度指数 k=%.2f\n", avgRight, avgLeft, k);

        // 发送标定结果
        String msg = "{\"type\":\"servo_calib_result\",\"speed_right\":"
                   + String(avgRight, 1) + ",\"speed_left\":"
                   + String(avgLeft, 1) + ",\"speed_exp\":"
                   + String(k, 2) + "}\n";
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

    // 编码器轮询（多圈累计）+ 在位检测
    if (encMode()) {
        g_encoder.poll();
        static uint32_t lastEncWarnMs = 0;
        if (!g_encoder.ok() && millis() - lastEncWarnMs > 5000) {
            lastEncWarnMs = millis();
            Serial.println("[Winder] 警告: AS5600 编码器无响应，位置将不更新");
        }
    }

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
            setTravPos(g_config.travelRangeMm);
            Serial.println("[Winder] 右原点已确认（原点 = 右限位）");
        } else {
            g_servo.stop();
            setTravPos(0);
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
    float pos = travPos();

    // 安全保护：碰到限位立刻停。若目标本身不在限位处，说明位置估算失真
    // （限位间距或舵机标定不准），报错而非硬顶限位。
    if (g_servo.getDirection() == DIR_RIGHT && g_sensors.isRightEndstopPressed()) {
        g_servo.stop();
        setTravPos(g_config.travelRangeMm);
        if (target < g_config.travelRangeMm - 1.0f) {
            setError(ERR_SENSOR, "定位顶右限位: 限位间距或舵机标定不准");
            return;
        }
        Serial.println("[Winder] 定位中触碰右限位");
    } else if (g_servo.getDirection() == DIR_LEFT && g_sensors.isLeftEndstopPressed()) {
        g_servo.stop();
        setTravPos(0);
        if (target > 1.0f) {
            setError(ERR_SENSOR, "定位顶左限位: 左起始过小，或限位间距/舵机标定不准");
            return;
        }
        Serial.println("[Winder] 定位中触碰左限位");
    }

    pos = travPos();
    if (pos >= target - 0.5f && pos <= target + 0.5f) {
        g_servo.stop();
        if (_targetSpeed > 0) {
            // 有速度 → 开始绕线，从起始位置向右移动
            g_state.calibCountdown = g_config.calIntervalRounds;
            _roundTrips = 0;
            if (isManualMode()) {
                g_motor.stop();               // 手动模式: 电机不输出，靠手摇
            } else {
                g_motor.setSpeedPct(_targetSpeed);
            }
            g_state.runStartMs = millis();
            // 初始化圈数驱动的排线目标
            _windTargetPos = g_config.traverseLeftStart;
            _windDirRight  = true;
            _windLastTurns = g_state.spoolTurns;
            _windGraceUntilMs = millis() + 10000;   // 定位收敛宽限
            g_servo.moveRight();
            setState(STATE_RUNNING);
            Serial.println(isManualMode() ? "[Winder] 开始绕线 (手动模式)" : "[Winder] 开始绕线");
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
    uint32_t now = millis();
    uint32_t newSpool = g_sensors.getSpoolPulsesAndReset();

    g_state.spoolPulses += newSpool;

    g_slip.onSpoolPulses(newSpool);

    float dtSec = dtMs / 1000.0f;
    g_state.spoolTurns      = (float)g_state.spoolPulses / g_config.hallSpoolMagnets;

    // ===== 排线目标推进（圈数驱动三角波）=====
    // 料盘每转一圈，目标位置前进一个线径；到绕线边界折返。
    // 从左端折返视作完成一个来回。
    _turnsAdvanced = (g_state.spoolTurns != _windLastTurns);
    if (_turnsAdvanced) {
        float d = (g_state.spoolTurns - _windLastTurns) * g_config.filamentDiameter;
        _windLastTurns = g_state.spoolTurns;
        _windTargetPos += _windDirRight ? d : -d;
        float left = g_config.traverseLeftStart;
        float right = g_config.traverseRightEnd;
        if (_windTargetPos >= right) {
            _windTargetPos = right - (_windTargetPos - right);
            _windDirRight = false;
            Serial.printf("[Winder] 目标到右端 %.1f 折返\n", right);
        } else if (_windTargetPos <= left) {
            _windTargetPos = left + (left - _windTargetPos);
            _windDirRight = true;
            _roundTrips++;
            g_state.roundTrips = _roundTrips;
            g_state.calibCountdown = (g_config.calIntervalRounds > _roundTrips)
                                    ? (g_config.calIntervalRounds - _roundTrips) : 0;
            Serial.printf("[Winder] 目标到左端 %.1f 折返, 来回=%d\n", left, _roundTrips);
            // 手动模式不按来回数触发校准（改为停转触发，见下方）
            if (!isManualMode() && _roundTrips >= g_config.calIntervalRounds) {
                enterCalibrating();
                return;
            }
        }
    }

    g_state.lengthTheoretical = g_slip.getLengthTheoretical();
    g_state.effectiveDiameter = g_slip.getEffectiveDiameter();
    g_state.currentLayer      = g_slip.getCurrentLayer();
    g_state.currentSpeedPct   = g_motor.getCurrentSpeedPct();

    // RPM 估算：脉冲间隔法。
    // 之前用 250ms 固定窗口数脉冲，低速时（10RPM ≈ 1.25s/脉冲）多数窗口为空，
    // 估算值在 0/30 之间跳变，导致慢速手摇时排线不动；EMA 衰减又永远到不了
    // 精确 0，停转后舵机以 1% 最低速继续爬。现改为：
    //  - 有脉冲: 用最近两次脉冲的间隔直接算瞬时 RPM，EMA 平滑
    //  - 无脉冲超过 SPOOL_STOP_MS: 判定停转，直接归零
    //  - 低于 MANUAL_MIN_RPM: 直接归零（杜绝显示 0 但仍爬行）
    if (newSpool > 0) {
        _lastPulseMs = now;
        uint32_t intervalUs = g_sensors.getSpoolIntervalUs();
        if (intervalUs > 0) {
            float instRpm = 60.0f / ((float)g_config.hallSpoolMagnets * (float)intervalUs / 1e6f);
            _smoothRpm = (_smoothRpm == 0) ? instRpm : _smoothRpm * 0.6f + instRpm * 0.4f;
        }
    } else if (_lastPulseMs != 0 && now - _lastPulseMs > SPOOL_STOP_MS) {
        _smoothRpm = 0;
    }
    if (_smoothRpm < MANUAL_MIN_RPM) _smoothRpm = 0;
    g_state.spoolRpm = _smoothRpm;

    if (isManualMode()) {
        // ===== 手动模式: 停转触发校准 =====
        // 条件: 本周期内手摇转起过 + 已完成至少 calIntervalRounds 个来回
        //       + 手摇停转持续 MANUAL_STOP_CALIB_MS
        if (_smoothRpm > MANUAL_MIN_RPM) {
            _manualSeenSpinning = true;
            _manualZeroSinceMs  = 0;
        } else if (_manualSeenSpinning &&
                   _roundTrips >= (g_config.calIntervalRounds > 0 ? g_config.calIntervalRounds : 1)) {
            if (_manualZeroSinceMs == 0) _manualZeroSinceMs = now;
            if (now - _manualZeroSinceMs >= MANUAL_STOP_CALIB_MS) {
                Serial.println("[Winder] 手动模式: 检测到停转，触发校准");
                enterCalibrating();
                return;
            }
        }
    } else {
        // ===== 电动模式: 缠料检测 =====
        // 电机在转但霍尔测得料盘停转，持续 JAM_DETECT_MS → 缠料/堵转
        if (g_motor.getCurrentSpeedPct() > JAM_MIN_MOTOR_PCT && _smoothRpm < JAM_MIN_RPM) {
            if (_jamStallMs == 0) _jamStallMs = now;
            else if (now - _jamStallMs >= JAM_DETECT_MS) {
                setError(ERR_JAM, "缠料/堵转: 电机运转但料盘未转动，请检查排线");
                return;
            }
        } else {
            _jamStallMs = 0;
        }
    }

    // 调试：每 2 秒打印一次
    static uint32_t lastDbgMs = 0;
    if (millis() - lastDbgMs > 2000) {
        lastDbgMs = millis();
        Serial.printf("[DBG] pulses=%u new=%u totalSpool=%u hallMagnets=%u pinHall=%u rpm=%.1f\n",
                      g_state.spoolPulses, newSpool,
                      g_sensors.getTotalSpoolPulses(),
                      g_config.hallSpoolMagnets, g_config.pinHallSpool, _smoothRpm);
    }

    processTraverse(dtMs);
    processAutoStop();
}

// 按记忆方向恢复排线运动（舵机因停转/切片间歇被暂停后调用）
void Winder::resumeTraverse() {
    if (g_servo.getDirection() != DIR_NONE) return;
    if (_travDir == DIR_LEFT) g_servo.moveLeft();
    else                      g_servo.moveRight();
}

// 排线位置源: 编码器闭环 / 舵机开环估算
float Winder::travPos() {
    return encMode() ? g_encoder.posMm() : g_servo.getPosition();
}

void Winder::setTravPos(float pos) {
    g_servo.setPosition(pos);       // 估算源同步，保证随时可回退
    if (encMode()) g_encoder.setPosMm(pos);
}

void Winder::processTraverse(uint32_t dtMs) {
    g_servo.updatePosition(dtMs);
    g_state.traversePos = travPos();
    g_state.traverseDir = g_servo.getDirection();

    uint32_t now = millis();
    float pos = travPos();

    // ===== 圈数驱动的排线（位置误差闭环）=====
    // 目标位置 _windTargetPos 由料盘圈数推进（见 doRunning），每圈 +1 线径。
    // 舵机朝目标收敛：料盘转了 N 圈，排线必然走 N×线径——舵机死区/低速
    // 漏掉的距离会累积为误差，误差越大追得越快，自动补齐。
    float err = _windTargetPos - pos;

    // 硬性保险: 料盘静止超过 3 秒后，排线仍未追上目标（偏差 >2mm）才报错。
    // 不加静止时间的话，高速手摇后停手的瞬间目标领先数毫米属正常追赶，
    // 会被误杀。校准/启动后的回归宽限期同样豁免。
    if (!_turnsAdvanced && fabs(err) > 2.0f && now > _windGraceUntilMs) {
        if (_staticSinceMs == 0) _staticSinceMs = now;
        if (now - _staticSinceMs > 3000) {
            if (g_servo.getDirection() != DIR_NONE) g_servo.stop();
            setError(ERR_SENSOR, "排线位置与目标偏差过大且料盘静止，疑似方向/限位接线镜像");
            return;
        }
    } else {
        _staticSinceMs = 0;
    }

    // 诊断: 每秒打印排线闭环状态（定位问题用）
    static uint32_t lastTravDbgMs = 0;
    if (now - lastTravDbgMs >= 1000) {
        lastTravDbgMs = now;
        Serial.printf("[TRA] pos=%.2f tgt=%.2f err=%.2f dir=%s turns=%.1f\n",
                      pos, _windTargetPos, err,
                      g_servo.getDirection() == DIR_LEFT  ? "L" :
                      g_servo.getDirection() == DIR_RIGHT ? "R" : "-",
                      g_state.spoolTurns);
    }

    if (fabs(err) < 0.2f) {
        // 已到位: 暂停等待目标推进（料盘慢/停时排线也停，位置不漂移）
        if (g_servo.getDirection() != DIR_NONE) {
            _travDir = g_servo.getDirection();
            g_servo.stop();
        }
        // 静止时仍顶限位 → 舵机中位偏移，停止脉宽下仍在缓慢转动
        if (g_sensors.isLeftEndstopPressed() || g_sensors.isRightEndstopPressed()) {
            if (_stillPressMs == 0) _stillPressMs = now;
            else if (now - _stillPressMs > 2000) {
                setError(ERR_SENSOR, "排线静止时仍顶限位: 舵机中位偏移，请微调「停止PWM」直至静止");
                return;
            }
        } else {
            _stillPressMs = 0;
        }
    } else {
        TraverseDir wantDir = (err > 0) ? DIR_RIGHT : DIR_LEFT;
        _travDir = wantDir;
        float servoFullSpeed = (wantDir == DIR_LEFT) ? g_config.servoTraverseSpeedLeft
                                                     : g_config.servoTraverseSpeedRight;
        if (encMode() && g_encoder.ok()) {
            // ===== 编码器闭环: PI 速度控制 =====
            // 目标速度 = 圈数前馈(转速×线径) + 位置误差修正，方向由误差决定；
            // 实测速度来自编码器，PI 连续调节舵机脉宽偏移。
            // 舵机物理上达不到的低速由 PI 自然退化为小幅修正脉冲（极限环），
            // 位置精度始终由编码器保证。开环估算模式走下方的迟滞步进。
            TraverseDir wantDir = (err > 0) ? DIR_RIGHT : DIR_LEFT;
            _travDir = wantDir;
            float full = (wantDir == DIR_LEFT) ? g_config.servoTraverseSpeedLeft
                                               : g_config.servoTraverseSpeedRight;
            if (full < 0.1f) full = 1.0f;

            float vTarget = 0;
            if (fabs(err) > 0.15f) {
                float ff = (g_state.spoolRpm / 60.0f) * g_config.filamentDiameter;
                vTarget = ff + fabs(err) * 1.5f;
                if (vTarget > full) vTarget = full;
                if (wantDir == DIR_LEFT) vTarget = -vTarget;
            }

            float dt = (now - _piLastMs) / 1000.0f;
            if (dt < 0.005f) dt = 0.005f;
            if (dt > 0.1f)   dt = 0.1f;
            _piLastMs = now;

            float vMeas = g_encoder.getSpeedMmPerS();
            float vErr  = vTarget - vMeas;

            // 积分（带限幅防饱和；到位静止时泄放）
            _piInteg += vErr * dt * 4.0f;
            if (_piInteg >  30.0f) _piInteg =  30.0f;
            if (_piInteg < -30.0f) _piInteg = -30.0f;
            if (vTarget == 0 && fabs(err) < 0.3f) _piInteg *= 0.9f;

            float out = vErr * 6.0f + _piInteg;
            if (out >  70.0f) out =  70.0f;
            if (out < -70.0f) out = -70.0f;
            if (fabs(vTarget) < 0.05f && fabs(err) < 0.3f) out = 0;
            if (fabs(out) < 5.0f && out != 0) out = 0;   // 输出死区，防无意义抖动

            g_servo.drive((int16_t)out);
        } else if (servoFullSpeed > 0.1f) {
            // 迟滞步进控制（针对"开关型"非线性舵机）:
            // 该舵机 40% 偏移即产生 92% 速度，几乎没有线性调速区间，
            // 比例调速/时间切片均不可行。改为: 误差 >1mm 以固定偏移走，
            // 收敛到 0.3mm 内停。步进频率自然跟随圈数速度（转得快步子密）。
            // 位置估算用幂律模型（k 由标定实测），在固定偏移点与实测吻合。
            if (fabs(err) > 1.0f) {
                _travDir = wantDir;
                if (g_servo.getDirection() != wantDir) {
                    if (wantDir == DIR_LEFT) g_servo.moveLeft();
                    else                     g_servo.moveRight();
                } else {
                    resumeTraverse();
                }
                g_servo.setSpeedFraction(SERVO_MIN_FRAC);
            } else if (fabs(err) < 0.3f) {
                if (g_servo.getDirection() != DIR_NONE) {
                    _travDir = g_servo.getDirection();
                    g_servo.stop();
                }
            }
            // 0.3~1.0mm 之间: 保持当前状态（迟滞带，防抖动）
        } else {
            // 未标定舵机速度，无法估算位置——保持停止并提示
            if (g_servo.getDirection() != DIR_NONE) g_servo.stop();
            static uint32_t lastNoCalMs = 0;
            if (millis() - lastNoCalMs > 5000) {
                lastNoCalMs = millis();
                Serial.println("[Winder] 警告: 舵机速度未标定，排线不动作（请先执行舵机速度标定）");
            }
        }
    }

    // 物理限位安全保护（真实触发时校正位置、方向与目标，三者保持一致）
    if (g_servo.getDirection() == DIR_RIGHT && g_sensors.isRightEndstopPressed()) {
        // 自适应速度校正: 提前撞右限位（估算 < 限位间距）说明排线实际走得比估算快，
        // 即舵机实际速度高于标定值（低速切片非线性导致），按比例上调两个方向的速度。
        if (pos > g_config.traverseLeftStart + 5.0f && pos < g_config.travelRangeMm - 2.0f) {
            float corr = g_config.travelRangeMm / pos;
            if (corr > 1.8f) corr = 1.8f;
            if (corr > 1.05f) {
                g_config.servoTraverseSpeedLeft  *= corr;
                g_config.servoTraverseSpeedRight *= corr;
                g_servo.setSpeeds(g_config.servoTraverseSpeedLeft, g_config.servoTraverseSpeedRight);
                Serial.printf("[Winder] 舵机速度自校正 x%.2f（撞右限位时估算 %.1f < %.1f）\n",
                              corr, pos, g_config.travelRangeMm);
            }
        }
        g_servo.moveLeft();
        setTravPos(g_config.travelRangeMm);
        _windDirRight = false;
        if (_windTargetPos > g_config.traverseRightEnd) _windTargetPos = g_config.traverseRightEnd;
        _windGraceUntilMs = now + 10000;   // 位置校正后需要长距离回退，重置宽限
        Serial.printf("[Winder] 右限位触发换向 pos=%.1f\n", pos);
    } else if (g_servo.getDirection() == DIR_LEFT && g_sensors.isLeftEndstopPressed()) {
        g_servo.moveRight();
        setTravPos(0);
        _windDirRight = true;
        if (_windTargetPos < g_config.traverseLeftStart) _windTargetPos = g_config.traverseLeftStart;
        _windGraceUntilMs = now + 10000;   // 位置校正后需要长距离回退，重置宽限
        Serial.printf("[Winder] 左限位触发换向 pos=%.1f\n", pos);
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

// 进入周期校准（电动: 来回数达标触发; 手动: 停转触发）
// 保存当前绕线目标，校准（位置对齐右限位）后恢复，不丢失绕线进度
void Winder::enterCalibrating() {
    setState(STATE_CALIBRATING);
    g_motor.stop();
    _calibSavedTarget   = _windTargetPos;
    _calibSavedDirRight = _windDirRight;
    _calibStartMs   = millis();   // 超时保护起点
    _manualSeenSpinning = false;   // 手动模式: 校准后重新等待手摇转起
    _manualZeroSinceMs  = 0;
    _jamStallMs         = 0;
    g_servo.moveRight();
    Serial.println("[Winder] 校准: 去右限位（原点）");
}

void Winder::doCalibrating(uint32_t dtMs) {
    g_servo.updatePosition(dtMs);
    g_state.traversePos = travPos();

    // 超时保护: 限位迟迟不触发（断线/卡死）则报错，避免排线一直顶限位
    if (millis() - _calibStartMs > (uint32_t)(HOMING_TIMEOUT_S * 1000)) {
        g_servo.stop();
        setError(ERR_SENSOR, "周期校准超时: 限位未触发（检查限位接线）");
        return;
    }

    // 去右限位对齐位置（右限位 = 位置 travelRangeMm）
    if (g_sensors.isRightEndstopPressed()) {
        setTravPos(g_config.travelRangeMm);
        Serial.println("[Winder] 右限位触发，位置校正");

        // 恢复校准前的绕线目标，排线自动回到原位置继续
        _roundTrips = 0;
        g_state.roundTrips = 0;
        g_state.calibCountdown = g_config.calIntervalRounds;
        _windTargetPos = _calibSavedTarget;
        _windDirRight  = _calibSavedDirRight;
        _windLastTurns = g_state.spoolTurns;   // 圈数重新对齐（校准期间目标不推进）
        // 目标夹回绕线区间（防漂移后越界）
        if (_windTargetPos > g_config.traverseRightEnd) _windTargetPos = g_config.traverseRightEnd;
        if (_windTargetPos < g_config.traverseLeftStart) _windTargetPos = g_config.traverseLeftStart;
        _travDir = _windDirRight ? DIR_RIGHT : DIR_LEFT;
        _windGraceUntilMs = millis() + 15000;   // 从限位返回原位置的宽限期
        if (isManualMode()) {
            g_motor.stop();     // 手动模式继续手摇
        } else {
            g_motor.setSpeedPct(_targetSpeed);
        }
        setState(STATE_RUNNING);
        Serial.printf("[Winder] 校准完成，恢复绕线（目标 %.1fmm）\n", _windTargetPos);
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
            setTravPos(g_config.travelRangeMm);
        } else {
            setTravPos(0);
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
