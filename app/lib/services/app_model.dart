import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'dart:io';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../models/device_config.dart';
import 'comm_manager.dart';

enum ConnectMode { ble, wifi }

class AppModel extends ChangeNotifier {
  final CommManager _comm = CommManager();

  // 连接状态
  bool bleConnected = false;
  bool wifiConnected = false;
  bool get isConnected => bleConnected || wifiConnected;

  // 设备状态
  DeviceStatus status = DeviceStatus();
  DeviceConfig config = DeviceConfig();
  List<String> presets = [];
  String? lastError;

  // 扫描
  bool scanning = false;
  List<BluetoothDevice> scanResults = [];

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
      bleConnected = true;
      // 填入模拟状态数据
      status.state = 'idle';
      status.speed = config.motorDefaultSpeed;
      status.spoolRpm = 0;
      status.spoolTurns = 0;
      status.lengthMeasured = 0;
      status.lengthTheoretical = 0;
      status.effectiveDiameter = config.spoolHasCardboard
          ? config.spoolCoreDiaWithCard
          : config.spoolCoreDiaNoCard;
      status.traversePos = config.traverseLeftStart;
      status.link = 'ble';
    } else {
      bleConnected = false;
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
    _comm.onBleConnectionChanged = (c) {
      bleConnected = c;
      notifyListeners();
    };
    _comm.onWifiConnectionChanged = (c) {
      wifiConnected = c;
      notifyListeners();
    };
    _loadSavedDeviceIP();
    _comm.onScanResult = (devices) {
      scanResults = devices;
      notifyListeners();
    };
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
      final socket = await RawDatagramSocket.bind('0.0.0.0', 8888);
      socket.broadcastEnabled = true;
      final result = socket.timeout(timeout, onTimeout: (sink) {
        sink.close();
      }).firstWhere(
        (event) {
          if (event == RawSocketEvent.read) {
            final datagram = socket.receive();
            if (datagram != null) {
              final msg = String.fromCharCodes(datagram.data);
              if (msg.startsWith('WINDER:')) {
                return true;
              }
            }
          }
          return false;
        },
      );
      await result;
      final datagram = socket.receive();
      socket.close();
      mdnsScanning = false;
      notifyListeners();
      if (datagram != null) {
        final msg = String.fromCharCodes(datagram.data);
        // WINDER:192.168.1.105:8080
        final parts = msg.split(':');
        if (parts.length >= 2) return parts[1];
      }
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
  //  BLE 扫描/连接
  // ===========================================================================

  Future<void> startScan() async {
    scanning = true;
    notifyListeners();
    await _comm.startScan();
    scanning = false;
    notifyListeners();
  }

  Future<void> stopScan() async {
    await _comm.stopScan();
  }

  Future<bool> connectBle(BluetoothDevice device) async {
    final ok = await _comm.connect(device);
    if (ok) {
      // 连接后请求参数和预设列表
      sendGetParams();
      sendListPresets();
    }
    return ok;
  }

  Future<void> disconnect() async {
    await _comm.disconnect();
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
        case 'response':
          // 命令响应，可选处理
          if (msg['ok'] == true && msg['cmd'] == 'save_preset') {
            sendListPresets();
          }
          if (msg['ok'] == true && msg['cmd'] == 'load_preset') {
            sendGetParams();
          }
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
