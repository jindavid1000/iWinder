import 'dart:async';
import 'dart:convert';
import 'dart:io';

/// 统一通信管理器（WiFi TCP）
class CommManager {
  // 回调
  void Function(String message)? onMessage;
  void Function(bool connected)? onWifiConnectionChanged;

  // WiFi 状态
  Socket? _socket;
  bool _wifiConnected = false;
  String _wifiIP = '';
  StreamSubscription? _socketSub;

  bool get wifiConnected => _wifiConnected;
  String get wifiIP => _wifiIP;
  String get activeLink => _wifiConnected ? 'wifi' : 'none';

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
    if (_wifiConnected && _socket != null) {
      _socket!.add(data);
    }
  }

  void sendCmd(Map<String, dynamic> cmd) {
    send(jsonEncode(cmd));
  }

  void dispose() {
    _socketSub?.cancel();
    _socket?.destroy();
  }
}
