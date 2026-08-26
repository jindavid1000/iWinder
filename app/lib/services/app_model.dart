import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'dart:io';

import '../models/device_config.dart';
import 'comm_manager.dart';

class AppModel extends ChangeNotifier {
  final CommManager _comm = CommManager();

  // 连接状态
  bool wifiConnected = false;
  bool get isConnected => wifiConnected;

  // 设备状态
  DeviceStatus status = DeviceStatus();
  DeviceConfig config = DeviceConfig();
  int _targetSpeed = 50;  // 滑块设定速度，启动时用这个值
  int get targetSpeed => _targetSpeed;
  set targetSpeed(int v) {
    _targetSpeed = v;
    notifyListeners();
  }
  List<String> presets = [];
  String? lastError;

  // WiFi 信息
  String? deviceIP;
  String? deviceSSID;
  bool deviceWifiConnected = false;
  String connectedIP = '';  // 实际连接用的 IP

  // 已保存的设备局域网 IP
  String? savedDeviceIP;
  String? savedDeviceSSID;

  // 预览模式（模拟器调试用）
  bool previewMode = false;

  // 预览模式本地预设管理
  final List<MapEntry<String, DeviceConfig>> _localPresets = [];

  void togglePreviewMode() {
    previewMode = !previewMode;
    if (previewMode) {
      wifiConnected = true;
      // 填入模拟状态数据
      status.state = 'idle';
      status.speed = config.motorDefaultSpeed;
      status.spoolRpm = 0;
      status.spoolTurns = 0;
      status.lengthTheoretical = 0;
      status.effectiveDiameter = config.spoolHasCardboard
          ? config.spoolCoreDiaWithCard
          : config.spoolCoreDiaNoCard;
      status.traversePos = config.traverseLeftStart;
      status.link = 'wifi';
    } else {
      wifiConnected = false;
    }
    notifyListeners();
  }

  void localSavePreset(String name, DeviceConfig cfg) {
    final idx = _localPresets.indexWhere((e) => e.key == name);
    if (idx >= 0) {
      _localPresets[idx] = MapEntry(name, cfg);
    } else {
      _localPresets.add(MapEntry(name, cfg));
    }
    presets = _localPresets.map((e) => e.key).toList();
    notifyListeners();
  }

  void localLoadPreset(String name) {
    final entry = _localPresets.where((e) => e.key == name).firstOrNull;
    if (entry != null) {
      config = entry.value.copy();
      notifyListeners();
    }
  }

  void localDeletePreset(String name) {
    _localPresets.removeWhere((e) => e.key == name);
    presets = _localPresets.map((e) => e.key).toList();
    notifyListeners();
  }

  AppModel() {
    _comm.onMessage = _handleMessage;
    _comm.onWifiConnectionChanged = (c) {
      wifiConnected = c;
      notifyListeners();
    };
    _loadSavedDeviceIP();
  }

  String get activeLink => _comm.activeLink;

  bool mdnsScanning = false;

  // ===========================================================================
  //  mDNS 局域网设备发现（无需提前知道 IP）
  // ===========================================================================

  Future<String?> discoverDevice({Duration timeout = const Duration(seconds: 4)}) async {
    mdnsScanning = true;
    notifyListeners();
    try {
      final socket = await RawDatagramSocket.bind('0.0.0.0', 0);
      socket.broadcastEnabled = true;

      // 主动发送发现广播，ESP32 收到后单播回复（避免 Android 丢弃广播包的问题）
      for (int i = 0; i < 3; i++) {
        socket.send(
          utf8.encode('DISCOVER_WINDER'),
          InternetAddress('255.255.255.255'),
          8888,
        );
        if (i < 2) await Future.delayed(const Duration(milliseconds: 600));
      }

      // 监听 ESP32 的单播回复
      String? foundIP;
      await for (final event in socket.timeout(timeout, onTimeout: (sink) {
        sink.close();
      })) {
        if (event == RawSocketEvent.read) {
          final dg = socket.receive();
          if (dg != null) {
            final msg = String.fromCharCodes(dg.data);
            // WINDER:192.168.1.105:8080
            if (msg.startsWith('WINDER:')) {
              final parts = msg.split(':');
              if (parts.length >= 2) {
                foundIP = parts[1];
                break;
              }
            }
          }
        }
      }
      socket.close();
      mdnsScanning = false;
      notifyListeners();
      return foundIP;
    } catch (e) {
      print('UDP 发现失败: $e');
    }
    mdnsScanning = false;
    notifyListeners();
    return null;
  }

  // ===========================================================================
  //  本地持久化（保存配网后的设备 IP）
  // ===========================================================================

  void _loadSavedDeviceIP() async {
    final prefs = await SharedPreferences.getInstance();
    savedDeviceIP = prefs.getString("device_ip");
    savedDeviceSSID = prefs.getString("device_ssid");
    notifyListeners();
  }

  void _saveDeviceIP(String ip, String ssid) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString("device_ip", ip);
    await prefs.setString("device_ssid", ssid);
    savedDeviceIP = ip;
    savedDeviceSSID = ssid;
    notifyListeners();
  }

  void clearSavedDeviceIP() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove("device_ip");
    await prefs.remove("device_ssid");
    savedDeviceIP = null;
    savedDeviceSSID = null;
    notifyListeners();
  }

  // ===========================================================================
  //  WiFi
  // ===========================================================================

  Future<bool> connectWifi(String ip, {int port = 8080}) async {
    final ok = await _comm.connectWifi(ip, port);
    if (ok) {
      connectedIP = ip;
    }
    return ok;
  }

  Future<void> disconnectWifi() async {
    await _comm.disconnectWifi();
    connectedIP = '';
  }

  // ===========================================================================
  //  发送命令
  // ===========================================================================

  void _send(Map<String, dynamic> cmd) => _comm.sendCmd(cmd);

  void sendStart(int speed) => _send({'cmd': 'start', 'speed': speed});
  void sendStop() => _send({'cmd': 'stop'});
  void sendPause() => _send({'cmd': 'pause'});
  void sendResume() => _send({'cmd': 'resume'});
  void sendHome() => _send({'cmd': 'home'});
  void sendSetSpeed(int speed) => _send({'cmd': 'set_speed', 'speed': speed});
  void sendGetStatus() => _send({'cmd': 'get_status'});
  void sendGetParams() => _send({'cmd': 'get_params'});
  void sendSetParams(Map<String, dynamic> params) =>
      _send({'cmd': 'set_params', 'params': params});
  void sendSavePreset(String name) =>
      _send({'cmd': 'save_preset', 'name': name});
  void sendLoadPreset(String name) =>
      _send({'cmd': 'load_preset', 'name': name});
  void sendDeletePreset(String name) =>
      _send({'cmd': 'delete_preset', 'name': name});
  void sendListPresets() => _send({'cmd': 'list_presets'});
  void sendSetWifi(String ssid, String password) =>
      _send({'cmd': 'set_wifi', 'ssid': ssid, 'password': password});
  void sendGetWifiStatus() => _send({'cmd': 'get_wifi_status'});
  void sendClearError() => _send({'cmd': 'clear_error'});
  void sendFactoryReset() => _send({'cmd': 'factory_reset'});

  // ===========================================================================
  //  设备授权
  // ===========================================================================
  String? licenseDeviceId;
  bool licenseOk = false;
  String? licenseExpiry;
  String? serverLicense;      // 在线申请/查询取得的许可证（自动填入输入框）
  String? serverLicenseMsg;   // 服务器返回的提示信息

  void sendLicenseQuery() => _send({'cmd': 'license'});
  void sendLicenseActivate(String key) => _send({'cmd': 'license', 'key': key});
  void sendLicenseApply(String name, String contact) =>
      _send({'cmd': 'license_apply', 'name': name, 'contact': contact});
  void sendLicenseFetch() => _send({'cmd': 'license_query'});

  // 固件 OTA
  String? fwCurrent;          // 当前固件版本（状态回报）
  bool fwUpdateAvailable = false;
  String? fwLatest;
  String? fwNotes;
  String? fwMsg;

  void sendOtaCheck() => _send({'cmd': 'ota_check'});
  void sendOtaStart() => _send({'cmd': 'ota_start'});

  // ===========================================================================
  //  绕线范围联动（消除冗余定义）
  //  物理模型: 料盘靠一侧安装，右法兰坐标(右终止)由机器安装决定，
  //  左起始 = 右终止 − 料盘宽度，自动推导，杜绝两边参数打架。
  //  特殊安装(左右都不固定)可切手动指定。
  // ===========================================================================
  bool leftAutoDerive = true;

  void setSpoolWidth(double v) {
    config.spoolWidth = v;
    _syncLeftStart();
  }

  void setTraverseRightEnd(double v) {
    config.traverseRightEnd = v;
    _syncLeftStart();
  }

  void _syncLeftStart() {
    if (!leftAutoDerive) return;
    final left = config.traverseRightEnd - config.spoolWidth;
    config.traverseLeftStart = left.clamp(0.0, config.traverseRightEnd);
    notifyListeners();
  }
  void sendCalibrateServo() => _send({'cmd': 'calibrate_servo'});

  // 下发当前配置到 ESP
  void pushConfig() {
    sendSetParams(config.toMap());
  }

  // ===========================================================================
  //  接收消息处理
  // ===========================================================================

  void _handleMessage(String line) {
    try {
      final msg = jsonDecode(line) as Map<String, dynamic>;
      final type = msg['type'] as String?;

      switch (type) {
        case 'status':
          status = DeviceStatus.fromMap(msg);
          break;
        case 'error':
          lastError = msg['msg'] as String? ?? '未知错误';
          status.errorCode = msg['code'] as String?;
          status.errorMsg = lastError;
          status.state = 'error';
          break;
        case 'params':
          if (msg.containsKey('params')) {
            final p = msg['params'];
            if (p is String) {
              config = DeviceConfig.fromMap(jsonDecode(p));
            } else if (p is Map) {
              config = DeviceConfig.fromMap(p.cast<String, dynamic>());
            }
          }
          break;
        case 'preset_list':
          presets = (msg['presets'] as List?)
                  ?.map((e) => e.toString())
                  .toList() ??
              [];
          break;
        case 'wifi_status':
          deviceWifiConnected = msg['connected'] as bool? ?? false;
          deviceIP = msg['ip'] as String?;
          deviceSSID = msg['ssid'] as String?;
          // 配网成功后 ESP 回报局域网 IP，保存到本地
          if (deviceWifiConnected && deviceIP != null && !deviceIP!.startsWith('192.168.4.')) {
            _saveDeviceIP(deviceIP!, deviceSSID ?? '');
          }
          break;
        case 'license':
          licenseDeviceId = msg['device_id'] as String?;
          licenseOk = msg['licensed'] as bool? ?? false;
          licenseExpiry = msg['expiry'] as String?;
          break;
        case 'status':
          if (msg['fw_version'] != null) fwCurrent = msg['fw_version'] as String;
          break;
        case 'license_apply':
        case 'license_query':
          serverLicenseMsg = msg['msg'] as String?;
          serverLicense = msg['license'] as String?;
          break;
        case 'ota_check':
          fwCurrent = msg['current'] as String?;
          fwUpdateAvailable = msg['update_available'] as bool? ?? false;
          fwLatest = msg['latest'] as String?;
          fwNotes = msg['notes'] as String?;
          fwMsg = msg['msg'] as String?;
          break;
        case 'response':
          // 激活/许可证命令的响应 → 刷新授权状态
          if (msg['cmd'] == 'license') {
            sendLicenseQuery();
          }
          if (msg['ok'] == true && msg['cmd'] == 'calibrate_servo') {
            // 标定已开始，等结果
          }
          // 命令响应，可选处理
          if (msg['ok'] == true && msg['cmd'] == 'save_preset') {
            sendListPresets();
          }
          if (msg['ok'] == true && msg['cmd'] == 'load_preset') {
            sendGetParams();
          }
          break;
        case 'servo_calib_result':
          final sr = (msg['speed_right'] as num?)?.toDouble() ?? 0;
          final sl = (msg['speed_left'] as num?)?.toDouble() ?? 0;
          config.servoTraverseSpeedRight = sr;
          config.servoTraverseSpeedLeft = sl;
          lastError = null;
          notifyListeners();
          break;
      }
      notifyListeners();
    } catch (e) {
      print('消息解析失败: $e ($line)');
    }
  }

  void clearLastError() {
    lastError = null;
    notifyListeners();
  }

  @override
  void dispose() {
    _comm.dispose();
    super.dispose();
  }
}
