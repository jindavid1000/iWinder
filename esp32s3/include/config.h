#ifndef CONFIG_H
#define CONFIG_H
//============================================================================
//  config.h — 耗材绕线器集中配置文件
//  所有可调参数的编译时默认值。运行时由 NVS 覆盖（APP 可改）。
//  对应文档「描述.md」第四节「配置文件规范」。
//============================================================================
#include <Arduino.h>

//============================================================================
//  4.2  引脚配置
//============================================================================
#define PIN_MOTOR_PWM         4       // 收线盘电机 MOS 管 PWM
#define PIN_SERVO_PWM         5       // 排线舵机 PWM
#define PIN_ENDSTOP           6       // Endstop 限位信号
#define PIN_HALL_IDLER        7       // 霍尔 A — 从动轮
#define PIN_HALL_SPOOL        8       // 霍尔 B — 料盘

//============================================================================
//  4.3  传感器参数
//============================================================================
#define HALL_IDLER_MAGNETS        2       // 从动轮单圈磁铁数
#define HALL_SPOOL_MAGNETS        4       // 料盘单圈磁铁数
#define IDLER_DIAMETER            10.0f   // 从动轮直径 (mm)
#define HALL_DEBOUNCE_US          5000    // 霍尔去抖动 (us)
#define ENDSTOP_DEBOUNCE_US       20000   // Endstop 去抖动 (us)

//============================================================================
//  4.4  料盘参数 — 拓竹 1kg 默认
//============================================================================
#define SPOOL_OUTER_DIAMETER          200.0f  // 料盘外径 (mm)
#define SPOOL_WIDTH                   68.0f   // 料盘绕线区宽度 (mm)
#define SPOOL_CORE_DIA_WITH_CARD      87.0f   // 有纸筒芯轴直径 (mm)
#define SPOOL_CORE_DIA_NO_CARD        81.5f   // 无纸筒芯轴直径 (mm)
#define FILAMENT_DIAMETER             1.75f   // 线径 (mm)

//============================================================================
//  4.5  运动参数
//============================================================================
#define TRAVERSE_LEFT_START       0.0f    // 绕线左起始位置 (mm，相对于原点)
#define TRAVERSE_RIGHT_END        68.0f   // 绕线右终止换向位置 (mm)
#define TRAVERSE_DIST_PER_REV     1.75f   // 料盘每转一圈排线移动距离 (mm)
#define LEAD_SCREW_PITCH          8.0f    // 丝杆导程 (mm/圈, T8)
#define CAL_INTERVAL_ROUNDS       3       // 每 N 个来回校准一次

//============================================================================
//  4.6  舵机参数 — 鑫辉科技 18KG 数字舵机 (连续旋转)
//  PWM 范围 500-2500us, 50Hz
//============================================================================
#define SERVO_STOP_PULSE          1500    // 停止中位 (us)
#define SERVO_LEFT_PULSE          1000    // 左行 (向 Endstop, 约半速)
#define SERVO_RIGHT_PULSE         2000    // 右行 (远离 Endstop, 约半速)
#define SERVO_HOME_PULSE          1300    // 寻原点/校准慢速左行 (us)
#define SERVO_PWM_FREQ            50      // PWM 频率 (Hz)
#define SERVO_PULSE_MIN           500     // 满速左行极限 (us)
#define SERVO_PULSE_MAX           2500    // 满速右行极限 (us)
#define SERVO_RES_BITS            16      // LEDC 分辨率位数
#define SERVO_TRAVERSE_SPEED_RIGHT  0.0f  // 右行线速度 (mm/s, 0=未标定)
#define SERVO_TRAVERSE_SPEED_LEFT   0.0f  // 左行线速度 (mm/s, 0=未标定)

//============================================================================
//  4.7  电机参数
//============================================================================
#define MOTOR_PWM_FREQ            1000    // PWM 频率 (Hz)
#define MOTOR_RES_BITS            10      // LEDC 分辨率位数 (0-1023)
#define MOTOR_MIN_SPEED           20      // 最低稳定转速 (%)
#define MOTOR_DEFAULT_SPEED       50      // 默认运行速度 (%)
#define MOTOR_MAX_SPEED           100     // 最大转速 (%)
#define MOTOR_SOFT_START_MS       1000    // 软启动时间 (ms)

//============================================================================
//  4.8  打滑检测参数
//============================================================================
#define SLIP_TOLERANCE            10.0f   // 打滑容差 (%)
#define STALL_TIMEOUT_S           3.0f    // 卡线超时 (s)

//============================================================================
//  4.9  通信参数
//============================================================================
#define BLE_DEVICE_NAME           "ESP-Winder"
#define BLE_DEVICE_NAME_CN        "esp 绕线器"
#define BLE_SERVICE_UUID          "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_RX_CHAR_UUID          "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_CHAR_UUID          "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define WIFI_TCP_PORT             8080
#define STATUS_REPORT_INTERVAL_MS 500

//============================================================================
//  4.10 安全参数
//============================================================================
#define HOMING_TIMEOUT_S          30
#define MAX_RUNTIME_S             3600
#define WIFI_CONNECT_TIMEOUT_S    15

//============================================================================
//  WiFi STA 凭据（连家庭路由器，AP 模式 DHCP 不工作时使用）
//  修改为你的 WiFi 名称和密码
//============================================================================
#define WIFI_STA_SSID             "DISABLED_STA_TEST"
#define WIFI_STA_PASSWORD         "00000000"

//============================================================================
//  4.11 任务完成参数
//============================================================================
// AUTO_STOP_MODE: 0=manual, 1=length, 2=turns
#define AUTO_STOP_MODE            0
#define TARGET_LENGTH_M           0.0f    // 0 = 不限
#define TARGET_TURNS              0       // 0 = 不限
#define FULL_LOAD_WARN_PCT        95

//============================================================================
//  4.12 硬件型号信息
//============================================================================
#define MOTOR_DRIVER_MODEL        "IRF520"
#define MOTOR_RATED_VOLTAGE       6.0f
#define MOTOR_RATED_RPM           300
#define SERVO_MODEL_NAME          "鑫辉18KG"

//============================================================================
//  预设存储
//============================================================================
#define MAX_PRESETS               10
#define MAX_PRESET_NAME_LEN       32
#define NVS_NAMESPACE             "winder"

//============================================================================
//  运行时配置结构体
//  运行时可被 APP 修改的完整参数集。首次启动从上面的 #define 初始化，
//  之后从 NVS 加载。每个预设方案存储一份完整的 DeviceConfig。
//============================================================================
struct DeviceConfig {
    // --- 引脚 ---
    uint8_t  pinMotorPwm;
    uint8_t  pinServoPwm;
    uint8_t  pinEndstop;
    uint8_t  pinHallIdler;
    uint8_t  pinHallSpool;

    // --- 传感器 ---
    uint8_t  hallIdlerMagnets;
    uint8_t  hallSpoolMagnets;
    float    idlerDiameter;
    uint32_t hallDebounceUs;
    uint32_t endstopDebounceUs;

    // --- 料盘 ---
    float    spoolOuterDiameter;
    float    spoolWidth;
    float    spoolCoreDiaWithCard;
    float    spoolCoreDiaNoCard;
    bool     spoolHasCardboard;
    float    filamentDiameter;

    // --- 运动 ---
    float    traverseLeftStart;
    float    traverseRightEnd;
    float    traverseDistPerRev;
    float    leadScrewPitch;
    uint8_t  calIntervalRounds;

    // --- 舵机 ---
    uint16_t servoStopPulse;
    uint16_t servoLeftPulse;
    uint16_t servoRightPulse;
    uint16_t servoHomePulse;
    uint16_t servoPulseMin;
    uint16_t servoPulseMax;
    float    servoTraverseSpeedRight;
    float    servoTraverseSpeedLeft;

    // --- 电机 ---
    uint16_t motorMinSpeed;
    uint16_t motorDefaultSpeed;
    uint16_t motorMaxSpeed;
    uint16_t motorSoftStartMs;

    // --- 打滑 ---
    float    slipTolerance;
    float    stallTimeoutS;

    // --- 任务完成 ---
    uint8_t  autoStopMode;       // 0=manual 1=length 2=turns
    float    targetLengthM;
    uint32_t targetTurns;
    uint8_t  fullLoadWarnPct;

    // --- 通信 ---
    uint16_t statusReportIntervalMs;
    bool     bleDisconnectStop;  // true = BLE 断开停机
    bool     wifiDisconnectStop; // true = WiFi 断开停机

    // 返回此配置的编译时默认值
    static DeviceConfig defaults() {
        DeviceConfig c{};
        c.pinMotorPwm           = PIN_MOTOR_PWM;
        c.pinServoPwm           = PIN_SERVO_PWM;
        c.pinEndstop            = PIN_ENDSTOP;
        c.pinHallIdler          = PIN_HALL_IDLER;
        c.pinHallSpool          = PIN_HALL_SPOOL;

        c.hallIdlerMagnets      = HALL_IDLER_MAGNETS;
        c.hallSpoolMagnets      = HALL_SPOOL_MAGNETS;
        c.idlerDiameter         = IDLER_DIAMETER;
        c.hallDebounceUs        = HALL_DEBOUNCE_US;
        c.endstopDebounceUs     = ENDSTOP_DEBOUNCE_US;

        c.spoolOuterDiameter    = SPOOL_OUTER_DIAMETER;
        c.spoolWidth            = SPOOL_WIDTH;
        c.spoolCoreDiaWithCard  = SPOOL_CORE_DIA_WITH_CARD;
        c.spoolCoreDiaNoCard    = SPOOL_CORE_DIA_NO_CARD;
        c.spoolHasCardboard     = true;
        c.filamentDiameter      = FILAMENT_DIAMETER;

        c.traverseLeftStart     = TRAVERSE_LEFT_START;
        c.traverseRightEnd      = TRAVERSE_RIGHT_END;
        c.traverseDistPerRev    = TRAVERSE_DIST_PER_REV;
        c.leadScrewPitch        = LEAD_SCREW_PITCH;
        c.calIntervalRounds     = CAL_INTERVAL_ROUNDS;

        c.servoStopPulse        = SERVO_STOP_PULSE;
        c.servoLeftPulse        = SERVO_LEFT_PULSE;
        c.servoRightPulse       = SERVO_RIGHT_PULSE;
        c.servoHomePulse        = SERVO_HOME_PULSE;
        c.servoPulseMin         = SERVO_PULSE_MIN;
        c.servoPulseMax         = SERVO_PULSE_MAX;
        c.servoTraverseSpeedRight = SERVO_TRAVERSE_SPEED_RIGHT;
        c.servoTraverseSpeedLeft  = SERVO_TRAVERSE_SPEED_LEFT;

        c.motorMinSpeed         = MOTOR_MIN_SPEED;
        c.motorDefaultSpeed     = MOTOR_DEFAULT_SPEED;
        c.motorMaxSpeed         = MOTOR_MAX_SPEED;
        c.motorSoftStartMs      = MOTOR_SOFT_START_MS;

        c.slipTolerance         = SLIP_TOLERANCE;
        c.stallTimeoutS         = STALL_TIMEOUT_S;

        c.autoStopMode          = AUTO_STOP_MODE;
        c.targetLengthM         = TARGET_LENGTH_M;
        c.targetTurns           = TARGET_TURNS;
        c.fullLoadWarnPct       = FULL_LOAD_WARN_PCT;

        c.statusReportIntervalMs = STATUS_REPORT_INTERVAL_MS;
        c.bleDisconnectStop     = true;   // BLE 断开默认停机
        c.wifiDisconnectStop    = false;   // WiFi 断开默认继续
        return c;
    }

    // 运行时有效芯轴直径
    float effectiveCoreDiameter() const {
        return spoolHasCardboard ? spoolCoreDiaWithCard : spoolCoreDiaNoCard;
    }
};

#endif // CONFIG_H
