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
#define PIN_MOTOR_PWM         4       // 收线盘电机 MOS 管 PWM（IRLB8721 栅极，经 150Ω 电阻）
#define PIN_SERVO_PWM         5       // 排线舵机 PWM
#define PIN_ENDSTOP           32       // Endstop 限位信号
#define PIN_ENDSTOP_RIGHT     14       // Endstop 限位信号 (右)
#define PIN_HALL_SPOOL        27       // 霍尔 B — 料盘
#define PIN_ENC_SDA           21       // AS5600 编码器 SDA（可选，闭环排线用）
#define PIN_ENC_SCL           22       // AS5600 编码器 SCL

//============================================================================
//  4.3  传感器参数
//============================================================================
#define HALL_SPOOL_MAGNETS        8       // 料盘单圈磁铁数
#define HALL_DEBOUNCE_US          25000   // 霍尔去抖动 (us)。
                                          // 兼作脉冲合理性下限: 8磁铁时等效最大 ~300RPM，
                                          // 滤除电机PWM耦合到霍尔线的噪声脉冲（5ms 挡不住）
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
#define TRAVERSE_LEFT_START       12.0f   // 绕线左起始位置 (mm，安装偏右，避开左限位)
#define TRAVERSE_RIGHT_END        80.0f   // 绕线右终止换向位置 (mm)
#define TRAVERSE_DIST_PER_REV     1.75f   // 料盘每转一圈排线移动距离 (mm)
#define LEAD_SCREW_PITCH          22.0f   // 丝杆导程 (mm/圈, T8 四头)
#define CAL_INTERVAL_ROUNDS       3       // 每 N 个来回校准一次
#define TRAVEL_RANGE_MM           80.0f   // 左右限位之间的实际物理距离 (mm，用于舵机标定)

//============================================================================
//  4.6  舵机参数 — 鑫辉科技 18KG 数字舵机 (连续旋转)
//  PWM 范围 500-2500us, 50Hz
//============================================================================
#define SERVO_STOP_PULSE          1500    // 停止中位 (us)
#define SERVO_LEFT_PULSE          500     // 左行满速 (向 Endstop)
#define SERVO_RIGHT_PULSE         2500    // 右行满速 (远离 Endstop)
#define SERVO_HOME_PULSE          500     // 寻原点/校准满速左行 (us)
#define SERVO_PWM_FREQ            50      // PWM 频率 (Hz)
#define SERVO_PULSE_MIN           500     // 满速左行极限 (us)
#define SERVO_PULSE_MAX           2500    // 满速右行极限 (us)
#define SERVO_RES_BITS            14      // LEDC 分辨率位数
#define SERVO_MIN_FRAC            0.30f   // 开环兜底模式的固定步进偏移比例（绕线主路径为编码器闭环）

// 排线位置反馈: 0=开环估算(默认) 1=AS5600 磁编码器闭环
// 编码器直测丝杆（推荐）→ 齿比 1.0，mm/圈 = 丝杆导程
#define TRAVERSE_ENCODER          0
#define ENC_GEAR_RATIO            1.0f
#define SERVO_TRAVERSE_SPEED_RIGHT  0.0f  // 右行线速度 (mm/s, 0=未标定)
#define SERVO_TRAVERSE_SPEED_LEFT   0.0f  // 左行线速度 (mm/s, 0=未标定)

//============================================================================
//  4.7  电机参数
//============================================================================
#define DRIVE_MODE               0       // 驱动模式: 0=电动(电机驱动) 1=手动(手摇驱动)
#define MOTOR_PWM_FREQ            20000   // PWM 频率 (Hz)。
                                          // 用 20kHz 而非 1kHz: GPIO4 与舵机信号线相邻，
                                          // 低频 PWM 会耦合进舵机信号导致排线缓慢漂移
#define MOTOR_RES_BITS            10      // LEDC 分辨率位数 (0-1023)
#define MOTOR_MIN_SPEED           20      // 最低稳定转速 (%)
#define MOTOR_DEFAULT_SPEED       100     // 默认运行速度 (%)
#define MOTOR_MAX_SPEED           100     // 最大转速 (%)
#define MOTOR_SOFT_START_MS       1000    // 软启动时间 (ms)

// 缠料检测（电动模式）: 电机运转但霍尔测得料盘 RPM 低于阈值并持续该时长 → 报错
#define JAM_DETECT_MS             4000    // 判定时长 (ms，> 软启动时间)
#define JAM_MIN_RPM               1.0f    // 料盘视为停转的 RPM 阈值
#define JAM_MIN_MOTOR_PCT         10.0f   // 电机速度高于此值才参与判定

// 停转校准（手动模式）: 手摇停转持续该时长且来回数已达标 → 触发周期校准
#define MANUAL_STOP_CALIB_MS      2000    // 停转持续时间 (ms)
#define MANUAL_MIN_RPM            0.5f    // 视为停转的 RPM 阈值
#define SPOOL_STOP_MS             1000    // 超过该时长无霍尔脉冲 → 判定料盘停转


//============================================================================
//  4.9  通信参数
//============================================================================
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
#define MOTOR_DRIVER_MODEL        "IRLB8721"
#define MOTOR_RATED_VOLTAGE       6.0f
#define MOTOR_RATED_RPM           40
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
    uint8_t  pinEndstopRight;
    uint8_t  pinHallSpool;

    // --- 传感器 ---
    uint8_t  hallSpoolMagnets;
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
    float    travelRangeMm;         // 左右限位之间的实际物理距离（位置换算/舵机标定基准）
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

    // --- 排线编码器（可选 AS5600 闭环）---
    uint8_t  traverseEncoder;      // 0=开环估算 1=AS5600 闭环
    uint8_t  pinEncSda;
    uint8_t  pinEncScl;
    float    encGearRatio;         // 丝杆转速 / 编码器轴转速
    float    encMmPerRev;          // 每编码器圈对应丝杆位移 mm（标定自动写入，0=按导程×齿比推算）

    // --- 电机 ---
    uint8_t  driveMode;           // 0=电动 1=手动(手摇)
    uint16_t motorMinSpeed;
    uint16_t motorDefaultSpeed;
    uint16_t motorMaxSpeed;
    uint16_t motorSoftStartMs;

    // --- 打滑 ---

    // --- 任务完成 ---
    uint8_t  autoStopMode;       // 0=manual 1=length 2=turns
    float    targetLengthM;
    uint32_t targetTurns;
    uint8_t  fullLoadWarnPct;

    // --- 通信 ---
    uint16_t statusReportIntervalMs;
    bool     wifiDisconnectStop; // true = WiFi 断开停机

    // 返回此配置的编译时默认值
    static DeviceConfig defaults() {
        DeviceConfig c{};
        c.pinMotorPwm           = PIN_MOTOR_PWM;
        c.pinServoPwm           = PIN_SERVO_PWM;
        c.pinEndstop            = PIN_ENDSTOP;
        c.pinEndstopRight       = PIN_ENDSTOP_RIGHT;
        c.pinHallSpool          = PIN_HALL_SPOOL;

        c.hallSpoolMagnets      = HALL_SPOOL_MAGNETS;
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
        c.travelRangeMm         = TRAVEL_RANGE_MM;
        c.calIntervalRounds     = CAL_INTERVAL_ROUNDS;

        c.servoStopPulse        = SERVO_STOP_PULSE;
        c.servoLeftPulse        = SERVO_LEFT_PULSE;
        c.servoRightPulse       = SERVO_RIGHT_PULSE;
        c.servoHomePulse        = SERVO_HOME_PULSE;
        c.servoPulseMin         = SERVO_PULSE_MIN;
        c.servoPulseMax         = SERVO_PULSE_MAX;
        c.servoTraverseSpeedRight = SERVO_TRAVERSE_SPEED_RIGHT;
        c.servoTraverseSpeedLeft  = SERVO_TRAVERSE_SPEED_LEFT;

        c.traverseEncoder         = TRAVERSE_ENCODER;
        c.pinEncSda               = PIN_ENC_SDA;
        c.pinEncScl               = PIN_ENC_SCL;
        c.encGearRatio            = ENC_GEAR_RATIO;
        c.encMmPerRev             = 0.0f;   // 0 = 未标定，按 leadScrewPitch × encGearRatio 推算

        c.driveMode            = DRIVE_MODE;
        c.motorMinSpeed         = MOTOR_MIN_SPEED;
        c.motorDefaultSpeed     = MOTOR_DEFAULT_SPEED;
        c.motorMaxSpeed         = MOTOR_MAX_SPEED;
        c.motorSoftStartMs      = MOTOR_SOFT_START_MS;

        c.autoStopMode          = AUTO_STOP_MODE;
        c.targetLengthM         = TARGET_LENGTH_M;
        c.targetTurns           = TARGET_TURNS;
        c.fullLoadWarnPct       = FULL_LOAD_WARN_PCT;

        c.statusReportIntervalMs = STATUS_REPORT_INTERVAL_MS;
        c.wifiDisconnectStop    = false;   // WiFi 断开默认继续
        return c;
    }

    // 运行时有效芯轴直径
    float effectiveCoreDiameter() const {
        return spoolHasCardboard ? spoolCoreDiaWithCard : spoolCoreDiaNoCard;
    }
};

#endif // CONFIG_H
