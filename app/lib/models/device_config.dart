import 'dart:convert';

/// 设备配置 — 镜像固件 config.h 的 DeviceConfig 结构体
class DeviceConfig {
  // 引脚
  int pinMotorPwm;
  int pinServoPwm;
  int pinEndstop;
  int pinEndstopRight;
  int pinHallSpool;

  // 传感器
  int hallSpoolMagnets;
  int hallDebounceUs;
  int endstopDebounceUs;

  // 料盘
  double spoolOuterDiameter;
  double spoolWidth;
  double spoolCoreDiaWithCard;
  double spoolCoreDiaNoCard;
  bool spoolHasCardboard;
  double filamentDiameter;

  // 运动
  double traverseLeftStart;
  double traverseRightEnd;
  double traverseDistPerRev;
  double leadScrewPitch;
  int calIntervalRounds;

  // 舵机
  int servoStopPulse;
  int servoLeftPulse;
  int servoRightPulse;
  int servoHomePulse;
  int servoPulseMin;
  int servoPulseMax;
  double servoTraverseSpeedRight;
  double servoTraverseSpeedLeft;

  // 电机
  int driveMode; // 0=电动 1=手动(手摇)
  int motorMinSpeed;
  int motorDefaultSpeed;
  int motorMaxSpeed;
  int motorSoftStartMs;

  // 任务完成
  int autoStopMode;
  double targetLengthM;
  int targetTurns;
  int fullLoadWarnPct;

  // 通信
  int statusReportIntervalMs;
  bool wifiDisconnectStop;

  DeviceConfig({
    this.pinMotorPwm = 4,
    this.pinServoPwm = 5,
    this.pinEndstop = 32,
    this.pinEndstopRight = 14,
    this.pinHallSpool = 27,
    this.hallSpoolMagnets = 4,
    this.hallDebounceUs = 5000,
    this.endstopDebounceUs = 20000,
    this.spoolOuterDiameter = 200.0,
    this.spoolWidth = 68.0,
    this.spoolCoreDiaWithCard = 87.0,
    this.spoolCoreDiaNoCard = 81.5,
    this.spoolHasCardboard = true,
    this.filamentDiameter = 1.75,
    this.traverseLeftStart = 0.0,
    this.traverseRightEnd = 68.0,
    this.traverseDistPerRev = 1.75,
    this.leadScrewPitch = 8.0,
    this.calIntervalRounds = 3,
    this.servoStopPulse = 1500,
    this.servoLeftPulse = 1000,
    this.servoRightPulse = 2000,
    this.servoHomePulse = 1300,
    this.servoPulseMin = 500,
    this.servoPulseMax = 2500,
    this.servoTraverseSpeedRight = 0.0,
    this.servoTraverseSpeedLeft = 0.0,
    this.driveMode = 0,
    this.motorMinSpeed = 20,
    this.motorDefaultSpeed = 50,
    this.motorMaxSpeed = 100,
    this.motorSoftStartMs = 1000,
    this.autoStopMode = 0,
    this.targetLengthM = 0.0,
    this.targetTurns = 0,
    this.fullLoadWarnPct = 95,
    this.statusReportIntervalMs = 500,
    this.wifiDisconnectStop = false,
  });

  /// 从 ESP 返回的 JSON 参数对象构造
  factory DeviceConfig.fromMap(Map<String, dynamic> m) {
    final c = DeviceConfig();
    c.pinMotorPwm = _i(m, 'pinMotorPwm', 4);
    c.pinServoPwm = _i(m, 'pinServoPwm', 5);
    c.pinEndstop = _i(m, 'pinEndstop', 32);
    c.pinEndstopRight = _i(m, 'pinEndstopRight', 14);
    c.pinHallSpool = _i(m, 'pinHallSpool', 27);
    c.hallSpoolMagnets = _i(m, 'hallSpoolMagnets', 4);
    c.hallDebounceUs = _i(m, 'hallDebounceUs', 5000);
    c.endstopDebounceUs = _i(m, 'endstopDebounceUs', 20000);
    c.spoolOuterDiameter = _d(m, 'spoolOuterDiameter', 200.0);
    c.spoolWidth = _d(m, 'spoolWidth', 68.0);
    c.spoolCoreDiaWithCard = _d(m, 'spoolCoreDiaWithCard', 87.0);
    c.spoolCoreDiaNoCard = _d(m, 'spoolCoreDiaNoCard', 81.5);
    c.spoolHasCardboard = _i(m, 'spoolHasCardboard', 1) != 0;
    c.filamentDiameter = _d(m, 'filamentDiameter', 1.75);
    c.traverseLeftStart = _d(m, 'traverseLeftStart', 0.0);
    c.traverseRightEnd = _d(m, 'traverseRightEnd', 68.0);
    c.traverseDistPerRev = _d(m, 'traverseDistPerRev', 1.75);
    c.leadScrewPitch = _d(m, 'leadScrewPitch', 8.0);
    c.calIntervalRounds = _i(m, 'calIntervalRounds', 3);
    c.servoStopPulse = _i(m, 'servoStopPulse', 1500);
    c.servoLeftPulse = _i(m, 'servoLeftPulse', 1000);
    c.servoRightPulse = _i(m, 'servoRightPulse', 2000);
    c.servoHomePulse = _i(m, 'servoHomePulse', 1300);
    c.servoPulseMin = _i(m, 'servoPulseMin', 500);
    c.servoPulseMax = _i(m, 'servoPulseMax', 2500);
    c.servoTraverseSpeedRight = _d(m, 'servoTraverseSpeedRight', 0.0);
    c.servoTraverseSpeedLeft = _d(m, 'servoTraverseSpeedLeft', 0.0);
    c.driveMode = _i(m, 'driveMode', 0);
    c.motorMinSpeed = _i(m, 'motorMinSpeed', 20);
    c.motorDefaultSpeed = _i(m, 'motorDefaultSpeed', 50);
    c.motorMaxSpeed = _i(m, 'motorMaxSpeed', 100);
    c.motorSoftStartMs = _i(m, 'motorSoftStartMs', 1000);
    c.autoStopMode = _i(m, 'autoStopMode', 0);
    c.targetLengthM = _d(m, 'targetLengthM', 0.0);
    c.targetTurns = _i(m, 'targetTurns', 0);
    c.fullLoadWarnPct = _i(m, 'fullLoadWarnPct', 95);
    c.statusReportIntervalMs = _i(m, 'statusReportIntervalMs', 500);
    c.wifiDisconnectStop = _i(m, 'wifiDisconnectStop', 0) != 0;
    return c;
  }

  /// 序列化为 JSON Map（用于 set_params 命令）
  Map<String, dynamic> toMap() => {
    'pinMotorPwm': pinMotorPwm,
    'pinServoPwm': pinServoPwm,
    'pinEndstop': pinEndstop,
    'pinEndstopRight': pinEndstopRight,
    'pinHallSpool': pinHallSpool,
    'hallSpoolMagnets': hallSpoolMagnets,
    'hallDebounceUs': hallDebounceUs,
    'endstopDebounceUs': endstopDebounceUs,
    'spoolOuterDiameter': spoolOuterDiameter,
    'spoolWidth': spoolWidth,
    'spoolCoreDiaWithCard': spoolCoreDiaWithCard,
    'spoolCoreDiaNoCard': spoolCoreDiaNoCard,
    'spoolHasCardboard': spoolHasCardboard ? 1 : 0,
    'filamentDiameter': filamentDiameter,
    'traverseLeftStart': traverseLeftStart,
    'traverseRightEnd': traverseRightEnd,
    'traverseDistPerRev': traverseDistPerRev,
    'leadScrewPitch': leadScrewPitch,
    'calIntervalRounds': calIntervalRounds,
    'servoStopPulse': servoStopPulse,
    'servoLeftPulse': servoLeftPulse,
    'servoRightPulse': servoRightPulse,
    'servoHomePulse': servoHomePulse,
    'servoPulseMin': servoPulseMin,
    'servoPulseMax': servoPulseMax,
    'servoTraverseSpeedRight': servoTraverseSpeedRight,
    'servoTraverseSpeedLeft': servoTraverseSpeedLeft,
    'driveMode': driveMode,
    'motorMinSpeed': motorMinSpeed,
    'motorDefaultSpeed': motorDefaultSpeed,
    'motorMaxSpeed': motorMaxSpeed,
    'motorSoftStartMs': motorSoftStartMs,
    'autoStopMode': autoStopMode,
    'targetLengthM': targetLengthM,
    'targetTurns': targetTurns,
    'fullLoadWarnPct': fullLoadWarnPct,
    'statusReportIntervalMs': statusReportIntervalMs,
    'wifiDisconnectStop': wifiDisconnectStop ? 1 : 0,
  };

  DeviceConfig copy() => DeviceConfig.fromMap(toMap());

  static int _i(Map<String, dynamic> m, String k, int d) =>
      m.containsKey(k) ? (m[k] as num).toInt() : d;
  static double _d(Map<String, dynamic> m, String k, double d) =>
      m.containsKey(k) ? (m[k] as num).toDouble() : d;
}

/// 预设方案
class Preset {
  String name;
  DeviceConfig config;
  Preset({required this.name, required this.config});
}

/// ESP 实时状态（从 status 消息解析）
class DeviceStatus {
  DeviceStatus();
  String state = 'idle';
  int speed = 0;
  double spoolRpm = 0;
  double spoolTurns = 0;
  double lengthTheoretical = 0;
  double effectiveDiameter = 0;
  int currentLayer = 0;
  double traversePos = 0;
  String traverseDir = 'none';
  int roundTrips = 0;
  int calibCountdown = 0;
  String link = 'none';
  int uptime = 0;
  String? errorCode;
  String? errorMsg;

  factory DeviceStatus.fromMap(Map<String, dynamic> m) {
    final s = DeviceStatus();
    s.state = m['state'] ?? 'idle';
    s.speed = (m['speed'] as num?)?.toInt() ?? 0;
    s.spoolRpm = (m['spool_rpm'] as num?)?.toDouble() ?? 0;
    s.spoolTurns = (m['spool_turns'] as num?)?.toDouble() ?? 0;
    s.lengthTheoretical = (m['length'] as num?)?.toDouble() ?? 0;
    s.effectiveDiameter = (m['effective_diameter'] as num?)?.toDouble() ?? 0;
    s.currentLayer = (m['current_layer'] as num?)?.toInt() ?? 0;
    s.traversePos = (m['traverse_pos'] as num?)?.toDouble() ?? 0;
    s.traverseDir = m['traverse_dir'] ?? 'none';
    s.roundTrips = (m['round_trips'] as num?)?.toInt() ?? 0;
    s.calibCountdown = (m['calib_countdown'] as num?)?.toInt() ?? 0;
    s.link = m['link'] ?? 'none';
    s.uptime = (m['uptime'] as num?)?.toInt() ?? 0;
    s.errorCode = m['error_code'];
    s.errorMsg = m['error_msg'];
    return s;
  }

  bool get isError => state == 'error';
  bool get isRunning => state == 'running';
  bool get isPaused => state == 'paused';
}
