#include "winder.h"
#include "motor.h"
#include "servo.h"
#include "sensors.h"
#include "encoder.h"
#include "traverse.h"
#include "calib_servo.h"
#include "slip.h"
#include "comms.h"
#include "protocol.h"
#include "storage.h"
#include "state.h"
#include "config.h"

Winder g_winder;

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
    g_motor.setDriver(c.motorDriver);

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

void Winder::reportFault(const char *msg) {
    setError(ERR_SENSOR, msg);
}

void Winder::startTask(int speedPct) {
    if (g_state.state == STATE_ERROR) return;

    // 前置检查: 开环估算模式必须有舵机标定速度；编码器闭环模式不需要
    if (!TraverseCtl::encoderMode() &&
        (g_config.servoTraverseSpeedLeft < 0.1f || g_config.servoTraverseSpeedRight < 0.1f)) {
        setError(ERR_SENSOR, "舵机速度未标定，请先标定或启用编码器闭环");
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
        g_state.spoolTurns = 0;
        g_state.lengthTheoretical = 0;
        g_state.roundTrips = 0;
        _smoothRpm = 0;
        _lastPulseMs = 0;
        _homeGoRight = true;
        _encRevsAtHomeStart = g_encoder.getRevs();
        g_servo.moveRight();  // 回右原点（原点 = 右限位）
        setState(STATE_HOMING);
        _homingStartMs = millis();
    } else if (g_state.state == STATE_PAUSED) {
        setState(STATE_RUNNING);
    }
}

void Winder::stopTask() {
    g_motor.stop();
    g_traverse.stop();
    setState(STATE_IDLE);
}

void Winder::pauseTask() {
    if (g_state.state != STATE_RUNNING) return;
    g_motor.stop();
    g_traverse.stop();
    setState(STATE_PAUSED);
}

void Winder::resumeTask() {
    if (g_state.state != STATE_PAUSED) return;
    setState(STATE_RUNNING);
}

void Winder::goHome() {
    if (g_state.state != STATE_IDLE && g_state.state != STATE_PAUSED) return;
    g_motor.stop();
    g_traverse.stop();
    _targetSpeed = 0;       // 仅回原点，不开始绕线
    _homeGoRight = true;
    _encRevsAtHomeStart = g_encoder.getRevs();
    g_servo.moveRight();    // 回右原点（右限位）
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

void Winder::startServoCalib() {
    g_scalib.start();
}

// ============================================================================
//  周期校准（去右限位对齐位置后恢复绕线目标）
// ============================================================================

void Winder::enterCalibrating() {
    setState(STATE_CALIBRATING);
    g_motor.stop();
    g_traverse.saveForCalib();
    _calibStartMs = millis();
    _manualSeenSpinning = false;   // 手动模式: 校准后重新等待手摇转起
    _manualZeroSinceMs  = 0;
    _jamStallMs         = 0;
    g_servo.moveRight();
    Serial.println("[Winder] 校准: 去右限位（原点）");
}

void Winder::doCalibrating(uint32_t dtMs) {
    g_servo.updatePosition(dtMs);
    g_state.traversePos = TraverseCtl::pos();

    if (millis() - _calibStartMs > (uint32_t)(HOMING_TIMEOUT_S * 1000)) {
        g_servo.stop();
        setError(ERR_SENSOR, "周期校准超时: 限位未触发（检查限位接线）");
        return;
    }

    if (g_sensors.isRightEndstopPressed()) {
        g_servo.stop();
        TraverseCtl::setPos(g_config.travelRangeMm);   // 触发瞬间锚定
        delay(600);                                    // 等柔性振荡平息
        Serial.println("[Winder] 右限位触发，位置校正");

        g_traverse.resetRoundTrips();
        g_state.calibCountdown = g_config.calIntervalRounds;
        g_traverse.restoreAfterCalib();
        if (isManualMode()) {
            g_motor.stop();     // 手动模式继续手摇
        } else {
            g_motor.setSpeedPct(_targetSpeed);
        }
        setState(STATE_RUNNING);
        Serial.printf("[Winder] 校准完成，恢复绕线（目标 %.1fmm）\n", g_traverse.targetPos());
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

    // 编码器在位检测（轮询在独立 1kHz 任务，见 encoder.cpp）
    if (TraverseCtl::encoderMode()) {
        static uint32_t lastEncWarnMs = 0;
        if (!g_encoder.ok() && millis() - lastEncWarnMs > 5000) {
            lastEncWarnMs = millis();
            Serial.println("[Winder] 警告: AS5600 编码器无响应，位置将不更新");
        }
    }

    if (g_scalib.active()) g_scalib.update();

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
}

// ============================================================================
//  寻原点（右限位 = 原点，锚定位置 = 限位间距）
// ============================================================================

void Winder::doHoming() {
    bool hit = _homeGoRight ? g_sensors.isRightEndstopPressed()
                            : g_sensors.isLeftEndstopPressed();

    if (hit) {
        if (_homeGoRight) {
            g_servo.stop();
            // 锚定取"首次触发点"（几何位置，重复性 ±0.1mm），
            // 而非停稳点（柔性行程回弹量随撞击速度漂移 2~8mm）。
            float dRevs = g_encoder.getRevs() - _encRevsAtHomeStart;
            if (fabsf(dRevs) > 0.5f) {
                // getRevs 带旧符号，真实原始计数方向 = dRevs × 旧符号。
                // 寻原点为右行，要求"右行 → 位置增大"：
                // 新符号 = 旧符号 × sign(dRevs)。（直接取 sign(dRevs) 在旧符号
                // 为 -1 时会把正确的符号再翻反）
                int8_t ns = g_encoder.getSign() * ((dRevs > 0) ? 1 : -1);
                if (ns != g_encoder.getSign()) {
                    g_encoder.setSign(ns);
                    Serial.printf("[Winder] 编码器方向自学习: 符号=%d\n", ns);
                }
            }
            TraverseCtl::setPos(g_config.travelRangeMm);
            Serial.println("[Winder] 右原点已确认（原点 = 右限位）");
            delay(600);   // 锚定后等柔性振荡平息（不改变锚点）
        } else {
            g_servo.stop();
            TraverseCtl::setPos(0);
            Serial.println("[Winder] 左限位已确认");
            delay(600);
        }
        _bootHoming = false;
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
//  定位到绕线起始位置
// ============================================================================

void Winder::doPositioning() {
    float target = g_config.traverseLeftStart;
    g_servo.updatePosition(5);
    float pos = TraverseCtl::pos();

    // 诊断: 每 300ms 打印（含编码器原始状态）
    static uint32_t lastPosDbgMs = 0;
    if (millis() - lastPosDbgMs >= 300) {
        lastPosDbgMs = millis();
        Serial.printf("[POS] pos=%.2f target=%.1f dir=%s enc=%.2fr wrap=%u maxpoll=%lums%s%s\n",
                      pos, target,
                      g_servo.getDirection() == DIR_LEFT  ? "L" :
                      g_servo.getDirection() == DIR_RIGHT ? "R" : "-",
                      g_encoder.getRevs(), g_encoder.unwrapCorrections(),
                      (unsigned long)g_encoder.maxPollMs(),
                      g_sensors.isLeftEndstopPressed()  ? " [左限位]" : "",
                      g_sensors.isRightEndstopPressed() ? " [右限位]" : "");
    }

    // 碰到限位 → 停稳 → 重新锚定 → 继续定位（自愈）。
    // 只有锚定时估算与预期偏差 >3mm 才报错（坐标链异常）。
    if (g_servo.getDirection() == DIR_RIGHT && g_sensors.isRightEndstopPressed()) {
        g_servo.stop();
        float posAtHit = TraverseCtl::pos();     // 触发瞬间锚定（首次触发点）
        TraverseCtl::setPos(g_config.travelRangeMm);
        delay(600);                              // 等柔性振荡平息（不改锚点）
        Serial.printf("[Winder] 定位中触碰右限位（触发时估算 %.1f）\n", posAtHit);
        if (posAtHit < g_config.travelRangeMm - 3.0f) {
            setError(ERR_SENSOR, "定位顶右限位且偏差>3mm: 检查限位间距/编码器标定");
            return;
        }
    } else if (g_servo.getDirection() == DIR_LEFT && g_sensors.isLeftEndstopPressed()) {
        g_servo.stop();
        float posAtHit = TraverseCtl::pos();
        TraverseCtl::setPos(0);
        delay(600);
        Serial.printf("[Winder] 定位中触碰左限位（触发时估算 %.1f）\n", posAtHit);
        if (posAtHit > target + 3.0f) {
            setError(ERR_SENSOR, "定位顶左限位且偏差>3mm: 检查限位间距/编码器标定");
            return;
        }
        // 锚定为 0 后，若目标 >0，下方定位逻辑自动右行到目标（自愈）
    }

    pos = TraverseCtl::pos();

    // 编码器符号自检: 命令某方向行驶而位置持续朝反方向变化 →
    // 符号学反（寻原点自学习可能被干扰污染），翻转后重新寻原点。
    static float    symChkStartPos = 0;
    static uint32_t symChkStartMs  = 0;
    TraverseDir cdir = g_servo.getDirection();
    if (TraverseCtl::encoderMode() && g_encoder.ok() &&
        (cdir == DIR_LEFT || cdir == DIR_RIGHT)) {
        if (symChkStartMs == 0) { symChkStartMs = millis(); symChkStartPos = pos; }
        float moved = pos - symChkStartPos;
        float expect = (cdir == DIR_LEFT) ? -1.0f : 1.0f;
        if (fabsf(moved) > 3.0f && millis() - symChkStartMs > 300 &&
            moved * expect < 0) {
            // 熔断: 连续两次翻转说明位置数据本身是垃圾（I2C 总线异常），
            // 不再无限寻原点，直接报错
            static uint8_t symFlips = 0;
            if (++symFlips >= 2) {
                symFlips = 0;
                g_servo.stop();
                setError(ERR_SENSOR, "编码器位置数据异常（符号反复学反）: "
                                     "检查 AS5600 接线/干扰，或暂时切回开环模式");
                return;
            }
            g_encoder.setSign(-g_encoder.getSign());
            Serial.printf("[Winder] 定位方向自检: 位置与行驶方向相反，"
                          "编码器符号翻转为 %d，重新寻原点\n", g_encoder.getSign());
            symChkStartMs = 0;
            _homeGoRight = true;
            _encRevsAtHomeStart = g_encoder.getRevs();
            g_servo.moveRight();
            setState(STATE_HOMING);
            _homingStartMs = millis();
            return;
        }
    } else {
        symChkStartMs = 0;
    }

    if (pos >= target - 2.0f && pos <= target + 2.0f) {
        // 到位窗口 ±2mm: 定位只需粗到位（绕线闭环自己会精调）。
        // 满速 ~60mm/s 进停有 ~100ms 舵机延迟 ≈ 4mm 滑行，窗口太窄
        // 会永远追不准，反而在起点附近来回冲。
        g_servo.stop();
        if (_targetSpeed > 0) {
            g_state.calibCountdown = g_config.calIntervalRounds;
            if (isManualMode()) {
                g_motor.stop();               // 手动模式: 电机不输出
            } else {
                g_motor.setSpeedPct(_targetSpeed);
            }
            g_state.runStartMs = millis();
            g_traverse.beginWinding();
            setState(STATE_RUNNING);
            Serial.println(isManualMode() ? "[Winder] 开始绕线 (手动模式)" : "[Winder] 开始绕线");
        } else {
            setState(STATE_IDLE);
            Serial.println("[Winder] 已回到起始位置，待机");
        }
    } else if (pos > target) {
        if (g_servo.getDirection() != DIR_LEFT) { g_servo.moveLeft(); g_servo.setSpeedFraction(1.0f); }
        // 近靶减速: 8mm 内降到 35%，抵消 ~100ms 停止延迟带来的 4mm 滑行
        if (pos - target < 8.0f) g_servo.setSpeedFraction(0.35f);
    } else {
        if (g_servo.getDirection() != DIR_RIGHT) { g_servo.moveRight(); g_servo.setSpeedFraction(1.0f); }
        if (target - pos < 8.0f) g_servo.setSpeedFraction(0.35f);
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

    g_state.spoolTurns        = (float)g_state.spoolPulses / g_config.hallSpoolMagnets;
    g_state.lengthTheoretical = g_slip.getLengthTheoretical();
    g_state.effectiveDiameter = g_slip.getEffectiveDiameter();
    g_state.currentLayer      = g_slip.getCurrentLayer();
    g_state.currentSpeedPct   = g_motor.getCurrentSpeedPct();

    // RPM 估算: 脉冲间隔法。
    //  - 有脉冲: 用最近两次脉冲间隔直接算瞬时 RPM，EMA 平滑
    //  - 无脉冲超过 SPOOL_STOP_MS: 判定停转归零
    //  - 低于 MANUAL_MIN_RPM: 归零（杜绝显示 0 但仍爬行）
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
        if (_smoothRpm > MANUAL_MIN_RPM) {
            _manualSeenSpinning = true;
            _manualZeroSinceMs  = 0;
        } else if (_manualSeenSpinning &&
                   g_traverse.roundTrips() >= (g_config.calIntervalRounds > 0 ? g_config.calIntervalRounds : 1)) {
            if (_manualZeroSinceMs == 0) _manualZeroSinceMs = now;
            if (now - _manualZeroSinceMs >= MANUAL_STOP_CALIB_MS) {
                Serial.println("[Winder] 手动模式: 检测到停转，触发校准");
                enterCalibrating();
                return;
            }
        }
    } else {
        // ===== 电动模式: 缠料检测 =====
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

    // 排线目标推进 + 闭环控制
    bool leftFold = g_traverse.onSpoolTurns(g_state.spoolTurns);
    if (leftFold && !isManualMode() &&
        g_traverse.roundTrips() >= g_config.calIntervalRounds) {
        enterCalibrating();   // 电动模式: 来回数达标触发周期校准
        return;
    }
    g_traverse.update(now);

    processAutoStop();
}

// ============================================================================
//  任务完成
// ============================================================================

void Winder::processAutoStop() {
    if ((millis() - g_state.runStartMs) / 1000 > (uint32_t)MAX_RUNTIME_S) {
        g_motor.stop();
        g_traverse.stop();
        setState(STATE_COMPLETED);
        return;
    }

    switch (g_config.autoStopMode) {
        case 1:
            if (g_config.targetLengthM > 0 &&
                g_state.lengthTheoretical >= g_config.targetLengthM) {
                g_motor.stop();
                g_traverse.stop();
                setState(STATE_COMPLETED);
            }
            break;
        case 2:
            if (g_config.targetTurns > 0 &&
                g_state.spoolTurns >= g_config.targetTurns) {
                g_motor.stop();
                g_traverse.stop();
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

void Winder::doCompleted() {
    static bool homingAfterComplete = false;
    if (!homingAfterComplete) {
        homingAfterComplete = true;
        _homeGoRight = true;
        _encRevsAtHomeStart = g_encoder.getRevs();
        g_servo.moveRight();
        _homingStartMs = millis();
        Serial.println("[Winder] 任务完成，归位中...");
    }

    bool hit = g_sensors.isRightEndstopPressed();
    if (hit) {
        g_servo.stop();
        TraverseCtl::setPos(g_config.travelRangeMm);
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
