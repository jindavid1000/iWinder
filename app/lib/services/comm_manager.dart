import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// 统一通信管理器（BLE + WiFi TCP）
class CommManager {
  static const String targetName = 'esp 绕线器';
  static final Guid _serviceUuid =
      Guid('6e400001-b5a3-f393-e0a9-e50e24dcca9e');
  static final Guid _rxUuid =
      Guid('6e400002-b5a3-f393-e0a9-e50e24dcca9e');
  static final Guid _txUuid =
      Guid('6e400003-b5a3-f393-e0a9-e50e24dcca9e');

  // 回调
  void Function(String message)? onMessage;
  void Function(bool connected)? onBleConnectionChanged;
  void Function(bool connected)? onWifiConnectionChanged;
  void Function(List<BluetoothDevice> devices)? onScanResult;

  // BLE 状态
  BluetoothDevice? _bleDevice;
  BluetoothCharacteristic? _rxChar;
  BluetoothCharacteristic? _txChar;
  bool _bleConnected = false;
  StreamSubscription? _scanSub;
  StreamSubscription? _txSub;
  StreamSubscription? _connSub;

  // WiFi 状态
  Socket? _socket;
  bool _wifiConnected = false;
  String _wifiIP = '';
  StreamSubscription? _socketSub;

  // BLE 缓冲（BLE 消息可能分片）
  String _bleBuffer = '';

  bool get bleConnected => _bleConnected;
  bool get wifiConnected => _wifiConnected;
  String get wifiIP => _wifiIP;
  String get activeLink {
    if (_bleConnected && _wifiConnected) return 'both';
    if (_bleConnected) return 'ble';
    if (_wifiConnected) return 'wifi';
    return 'none';
  }

  // ===========================================================================
  //  BLE
  // ===========================================================================

  Future<void> startScan({Duration timeout = const Duration(seconds: 10)}) async {
    _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      final matched = results
          .where((r) => r.device.platformName.isNotEmpty)
          .map((r) => r.device)
          .toList();
      onScanResult?.call(matched);
    });
    await FlutterBluePlus.startScan(timeout: timeout);
  }

  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
  }

  Future<bool> connect(BluetoothDevice device) async {
    try {
      await device.connect(timeout: const Duration(seconds: 10));
      _bleDevice = device;

      _connSub = device.connectionState.listen((state) {
        _bleConnected = state == BluetoothConnectionState.connected;
        onBleConnectionChanged?.call(_bleConnected);
        if (!_bleConnected) {
          _rxChar = null;
          _txChar = null;
        }
      });

      // 发现服务
      List<BluetoothService> services = await device.discoverServices();
      for (var svc in services) {
        if (svc.uuid == _serviceUuid) {
          for (var char in svc.characteristics) {
            if (char.uuid == _rxUuid) _rxChar = char;
            if (char.uuid == _txUuid) _txChar = char;
          }
        }
      }

      if (_txChar != null) {
        await _txChar!.setNotifyValue(true);
        _txSub = _txChar!.lastValueStream.listen((data) {
          _handleBleData(data);
        });
      }

      _bleConnected = true;
      onBleConnectionChanged?.call(true);
      return true;
    } catch (e) {
      print('BLE 连接失败: $e');
      return false;
    }
  }

  Future<void> disconnect() async {
    await _txSub?.cancel();
    await _connSub?.cancel();
    await _bleDevice?.disconnect();
    _bleConnected = false;
    _bleDevice = null;
    _rxChar = null;
    _txChar = null;
    onBleConnectionChanged?.call(false);
  }

  void _handleBleData(List<int> data) {
    _bleBuffer += utf8.decode(data);
    while (_bleBuffer.contains('\n')) {
      int idx = _bleBuffer.indexOf('\n');
      String line = _bleBuffer.substring(0, idx).trim();
      _bleBuffer = _bleBuffer.substring(idx + 1);
      if (line.isNotEmpty) onMessage?.call(line);
    }
    // BLE 消息可能没有换行结尾
    if (_bleBuffer.isNotEmpty && !_bufferWaiting()) {
      String line = _bleBuffer.trim();
      _bleBuffer = '';
      if (line.isNotEmpty) onMessage?.call(line);
    }
  }

  bool _bufferWaiting() {
    // 如果缓冲区看起来像不完整的 JSON（没有闭合括号），等待更多数据
    return !_bleBuffer.contains('}');
  }

  // ===========================================================================
  //  WiFi TCP
  // ===========================================================================

  Future<bool> connectWifi(String ip, int port) async {
    try {
      _socket = await Socket.connect(ip, port,
          timeout: const Duration(seconds: 5));
      _wifiConnected = true;
      _wifiIP = ip;

      _socketSub = _socket!
          .cast<List<int>>()
          .transform(utf8.decoder)
          .transform(const LineSplitter())
          .listen((line) {
        if (line.trim().isNotEmpty) onMessage?.call(line.trim());
      }, onError: (e) {
        _wifiConnected = false;
        onWifiConnectionChanged?.call(false);
      }, onDone: () {
        _wifiConnected = false;
        onWifiConnectionChanged?.call(false);
      });

      onWifiConnectionChanged?.call(true);
      return true;
    } catch (e) {
      print('WiFi 连接失败: $e');
      return false;
    }
  }

  Future<void> disconnectWifi() async {
    await _socketSub?.cancel();
    _socket?.destroy();
    _socket = null;
    _wifiConnected = false;
    onWifiConnectionChanged?.call(false);
  }

  // ===========================================================================
  //  统一发送
  // ===========================================================================

  void send(String message) {
    final data = utf8.encode('$message\n');
    // BLE
    if (_bleConnected && _rxChar != null) {
      _rxChar!.write(data, withoutResponse: false);
    }
    // WiFi
    if (_wifiConnected && _socket != null) {
      _socket!.add(data);
    }
  }

  void sendCmd(Map<String, dynamic> cmd) {
    send(jsonEncode(cmd));
  }

  void dispose() {
    _scanSub?.cancel();
    _txSub?.cancel();
    _connSub?.cancel();
    _socketSub?.cancel();
    _socket?.destroy();
  }
}
