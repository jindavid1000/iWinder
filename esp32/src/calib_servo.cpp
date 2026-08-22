#include "calib_servo.h"
#include "servo.h"
#include "motor.h"
#include "sensors.h"
#include "encoder.h"
#include "traverse.h"
#include "winder.h"
#include "comms.h"
#include "storage.h"
#include "state.h"
#include "config.h"

ServoCalibrator g_scalib;

static float g_distFallbackVal = 80.0f;

// 单趟实测距离: 编码器圈数 × 每圈位移（编码器直测丝杆时 = 导程；加齿轮时已含齿比）。
// 无编码器时退回「限位间距」参数。
static float passDistEnc(float startRevs, float fallback) {
    if (TraverseCtl::encoderMode() && g_encoder.ok()) {
        return fabsf(g_encoder.getRevs() - startRevs) * g_encoder.getMmPerRev();
    }
    return fallback;
}

bool ServoCalibrator::start() {
    if (g_state.state != STATE_IDLE) return false;
    _dist = g_config.travelRangeMm;
    g_distFallbackVal = _dist;
    if (_dist < 1.0f) return false;

    _phase = PHASE_HOME;
    _round = 0;
    _sumRight = _sumLeft = 0;
    _slowRight = _slowLeft = 0;
    _timeoutMs = millis() + (uint32_t)(HOMING_TIMEOUT_S * 1000);

    g_motor.stop();
    g_servo.moveHome();      // 满速左行归位
    g_winder.setStateExternal(STATE_SERVO_CALIB);
    Serial.printf("[标定] 开始，距离=%.1fmm\n", _dist);
    return true;
}

void ServoCalibrator::update() {
    if (_phase == PHASE_IDLE || _phase == PHASE_DONE) return;

    if (millis() > _timeoutMs) {
        _phase = PHASE_IDLE;
        g_servo.stop();
        g_winder.reportFault("舵机标定超时: 限位未触发（检查限位接线）");
        return;
    }

    switch (_phase) {
        case PHASE_HOME:      // 归位左限位
            if (g_sensors.isLeftEndstopPressed()) {
                g_servo.stop();
                TraverseCtl::setPos(0);           // 触发瞬间锚定（首次触发点）
                _encRevsAtLeft = g_encoder.getRevs();
                Serial.println("[标定] 左限位已确认，开始右行");
                delay(800);                       // 等柔性振荡平息
                _moveStartMs = millis();
                _passStartRevs = g_encoder.getRevs();
                _phase = PHASE_GO_RIGHT;
                g_servo.moveRight();
            }
            break;

        case PHASE_GO_RIGHT: {                    // 满速右行
            if (!g_sensors.isRightEndstopPressed()) break;
            uint32_t hitMs = millis();            // 到达时刻先取，停稳等待不计入行程
            g_servo.stop();
            delay(600);
            TraverseCtl::setPos(g_config.travelRangeMm);

            // 编码器比例标定: 限位间实际转数 → 每圈位移 mm
            if (TraverseCtl::encoderMode()) {
                float revs = g_encoder.getRevs() - _encRevsAtLeft;
                if (revs > 1.0f) {
                    float mmPerRev = _dist / revs;
                    g_config.encMmPerRev = mmPerRev;
                    g_storage.saveConfig(g_config);
                    g_encoder.setMmPerRev(mmPerRev);
                    TraverseCtl::setPos(g_config.travelRangeMm);
                    Serial.printf("[标定] 编码器比例: %.2f mm/圈 (%.1fmm / %.2f圈)\n",
                                  mmPerRev, _dist, revs);
                }
            }

            float dtSec = (hitMs - _moveStartMs) / 1000.0f;
            float dist = passDistEnc(_passStartRevs, _dist);
            float speed = dist / dtSec;
            _sumRight += speed;
            Serial.printf("[标定] 右行 #%d: %.1fmm / %.2fs = %.1f mm/s\n",
                          _round + 1, dist, dtSec, speed);
            delay(200);
            _moveStartMs = millis();
            _passStartRevs = g_encoder.getRevs();
            _phase = PHASE_GO_LEFT;
            g_servo.moveLeft();
            break;
        }

        case PHASE_GO_LEFT: {                     // 满速左行
            if (!g_sensors.isLeftEndstopPressed()) break;
            g_servo.stop();
            TraverseCtl::setPos(0);

            float dtSec = (millis() - _moveStartMs) / 1000.0f;
            float dist = passDistEnc(_passStartRevs, _dist);
            float speed = dist / dtSec;
            _sumLeft += speed;
            Serial.printf("[标定] 左行 #%d: %.1fmm / %.2fs = %.1f mm/s\n",
                          _round + 1, dist, dtSec, speed);

            _round++;
            if (_round < 3) {
                delay(200);
                _moveStartMs = millis();
                _passStartRevs = g_encoder.getRevs();
                _phase = PHASE_GO_RIGHT;
                g_servo.moveRight();
            } else {
                // 满速来回完成 → 步进速度(30%)来回
                delay(200);
                g_servo.setSpeedFraction(SERVO_MIN_FRAC);
                _moveStartMs = millis();
                _passStartRevs = g_encoder.getRevs();
                _phase = PHASE_SLOW_RIGHT;
                g_servo.moveRight();
                Serial.printf("[标定] 满速测量完成，步进速度(%.0f%%)测量中\n",
                              SERVO_MIN_FRAC * 100);
            }
            break;
        }

        case PHASE_SLOW_RIGHT: {                  // 步进速度右行
            if (!g_sensors.isRightEndstopPressed()) break;
            g_servo.stop();
            TraverseCtl::setPos(g_config.travelRangeMm);
            _slowRight = passDistEnc(_passStartRevs, _dist) / ((millis() - _moveStartMs) / 1000.0f);
            Serial.printf("[标定] 步进右行: %.1f mm/s\n", _slowRight);
            delay(200);
            g_servo.setSpeedFraction(SERVO_MIN_FRAC);
            _moveStartMs = millis();
            _passStartRevs = g_encoder.getRevs();
            _phase = PHASE_SLOW_LEFT;
            g_servo.moveLeft();
            break;
        }

        case PHASE_SLOW_LEFT: {                   // 步进速度左行
            if (!g_sensors.isLeftEndstopPressed()) break;
            g_servo.stop();
            TraverseCtl::setPos(0);
            _slowLeft = passDistEnc(_passStartRevs, _dist) / ((millis() - _moveStartMs) / 1000.0f);
            Serial.printf("[标定] 步进左行: %.1f mm/s\n", _slowLeft);
            _phase = PHASE_DONE;
            break;
        }

        default:
            break;
    }

    if (_phase == PHASE_DONE) {
        float avgRight = _sumRight / 3.0f;
        float avgLeft  = _sumLeft  / 3.0f;
        g_config.servoTraverseSpeedRight = avgRight;
        g_config.servoTraverseSpeedLeft  = avgLeft;
        g_storage.saveConfig(g_config);
        g_servo.setSpeeds(avgLeft, avgRight);

        Serial.printf("[标定] 完成! 右行=%.1f 左行=%.1f mm/s\n", avgRight, avgLeft);

        String msg = "{\"type\":\"servo_calib_result\",\"speed_right\":"
                   + String(avgRight, 1) + ",\"speed_left\":"
                   + String(avgLeft, 1) + "}\n";
        g_comms.send(msg);

        _phase = PHASE_IDLE;
        g_winder.setStateExternal(STATE_IDLE);
    }
}
