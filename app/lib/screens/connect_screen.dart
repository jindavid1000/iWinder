import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../services/app_model.dart';

class ConnectScreen extends StatelessWidget {
  const ConnectScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();

    return Scaffold(
      appBar: AppBar(title: const Text('设备连接')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (!model.bleConnected && !model.wifiConnected)
            Card(
              color: Theme.of(context).colorScheme.primaryContainer,
              child: ListTile(
                leading: Icon(Icons.visibility,
                    color: Theme.of(context).colorScheme.primary),
                title: const Text('预览模式'),
                subtitle: const Text('模拟设备连接，浏览所有界面'),
                trailing: FilledButton.tonal(
                  onPressed: () => model.togglePreviewMode(),
                  child: const Text('开启'),
                ),
                onTap: () => model.togglePreviewMode(),
              ),
            ),
          if (model.previewMode)
            Card(
              color: Colors.orange.shade50,
              child: ListTile(
                leading: const Icon(Icons.visibility, color: Colors.orange),
                title: const Text('预览模式已开启'),
                subtitle: const Text('使用模拟数据。点击退出预览模式'),
                trailing: OutlinedButton(
                  onPressed: () => model.togglePreviewMode(),
                  child: const Text('退出'),
                ),
                onTap: () => model.togglePreviewMode(),
              ),
            ),
          if (model.previewMode) const SizedBox(height: 16),
          _ConnectionStatus(model: model),
          const SizedBox(height: 16),
          if (model.savedDeviceIP != null && !model.wifiConnected)
            _SavedDeviceCard(model: model),
          if (model.savedDeviceIP != null && !model.wifiConnected)
            const SizedBox(height: 16),
          _BleSection(model: model),
          const SizedBox(height: 16),
          if (model.isConnected) _WifiSection(model: model),
        ],
      ),
    );
  }
}

class _ConnectionStatus extends StatelessWidget {
  final AppModel model;
  const _ConnectionStatus({required this.model});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(Icons.link, color: model.isConnected ? Colors.green : Colors.grey),
              const SizedBox(width: 8),
              Text('通信链路: ${model.activeLink}'),
            ]),
            const SizedBox(height: 8),
            _StatusChip(label: '蓝牙', active: model.bleConnected, icon: Icons.bluetooth),
            const SizedBox(height: 4),
            _StatusChip(label: 'WiFi', active: model.wifiConnected, icon: Icons.wifi),
            if (model.deviceWifiConnected && model.deviceIP != null) ...[
              const SizedBox(height: 8),
              Text('设备 IP: ${model.deviceIP}', style: const TextStyle(fontFamily: 'monospace')),
            ],
          ],
        ),
      ),
    );
  }
}

class _StatusChip extends StatelessWidget {
  final String label;
  final bool active;
  final IconData icon;
  const _StatusChip({required this.label, required this.active, required this.icon});

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      Icon(icon, size: 18, color: active ? Colors.green : Colors.grey),
      const SizedBox(width: 6),
      Text(label),
      const SizedBox(width: 8),
      Container(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
        decoration: BoxDecoration(
          color: active ? Colors.green.shade50 : Colors.grey.shade100,
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: active ? Colors.green : Colors.grey.shade300),
        ),
        child: Text(active ? '已连接' : '未连接',
            style: TextStyle(fontSize: 12, color: active ? Colors.green : Colors.grey)),
      ),
    ]);
  }
}

class _BleSection extends StatelessWidget {
  final AppModel model;
  const _BleSection({required this.model});

  @override
  Widget build(BuildContext context) {
    if (model.bleConnected) {
      return Card(
        child: ListTile(
          leading: const Icon(Icons.bluetooth_connected, color: Colors.green),
          title: const Text('ESP-Winder'),
          subtitle: const Text('已连接，点击断开'),
          trailing: TextButton(
            onPressed: () => model.disconnect(),
            child: const Text('断开'),
          ),
        ),
      );
    }
    return _TcpConnectSection(model: model);
  }
}

class _TcpConnectSection extends StatefulWidget {
  final AppModel model;
  const _TcpConnectSection({required this.model});
  @override
  State<_TcpConnectSection> createState() => _TcpConnectSectionState();
}

class _TcpConnectSectionState extends State<_TcpConnectSection> {
  final _ipCtrl = TextEditingController(text: '192.168.4.1');
  final _portCtrl = TextEditingController(text: '8080');
  bool _connecting = false;

  @override
  void dispose() {
    _ipCtrl.dispose();
    _portCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final m = widget.model;
    if (m.wifiConnected) {
      return Card(
        child: ListTile(
          leading: const Icon(Icons.wifi, color: Colors.green),
          title: Text('已连接: ${m.connectedIP}'),
          subtitle: const Text('TCP 连接已建立'),
          trailing: TextButton(
            onPressed: () => m.disconnectWifi(),
            child: const Text('断开'),
          ),
        ),
      );
    }
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(Icons.wifi, color: Theme.of(context).colorScheme.primary),
              const SizedBox(width: 8),
              Text('WiFi 连接', style: Theme.of(context).textTheme.titleMedium),
            ]),
            const SizedBox(height: 4),
            const Text(
              '1. 手机 WiFi 设置连接热点 ESP-Winder\n'
              '2. 输入 IP 地址（默认 192.168.4.1）\n'
              '3. 点击连接',
              style: TextStyle(fontSize: 12, color: Colors.grey, height: 1.6),
            ),
            const SizedBox(height: 12),
            Row(children: [
              const SizedBox(width: 40, child: Text('IP')),
              Expanded(
                child: TextField(
                  controller: _ipCtrl,
                  decoration: const InputDecoration(
                    isDense: true,
                    border: OutlineInputBorder(),
                    hintText: '192.168.4.1',
                  ),
                ),
              ),
            ]),
            const SizedBox(height: 8),
            Row(children: [
              const SizedBox(width: 40, child: Text('端口')),
              Expanded(
                child: TextField(
                  controller: _portCtrl,
                  keyboardType: TextInputType.number,
                  decoration: const InputDecoration(
                    isDense: true,
                    border: OutlineInputBorder(),
                    hintText: '8080',
                  ),
                ),
              ),
            ]),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: _connecting
                  ? null
                  : () async {
                      setState(() => _connecting = true);
                      final ok = await m.connectWifi(
                        _ipCtrl.text,
                        port: int.tryParse(_portCtrl.text) ?? 8080,
                      );
                      setState(() => _connecting = false);
                      if (ok && mounted) {
                        m.sendGetParams();
                        m.sendListPresets();
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('连接成功')),
                        );
                      } else if (mounted) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('连接失败，检查 IP 和端口')),
                        );
                      }
                    },
              icon: _connecting
                  ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
                  : const Icon(Icons.cable),
              label: Text(_connecting ? '连接中...' : '连接'),
            ),
          ],
        ),
      ),
    );
  }
}

class _WifiSection extends StatefulWidget {
  final AppModel model;
  const _WifiSection({required this.model});
  @override
  State<_WifiSection> createState() => _WifiSectionState();
}

class _WifiSectionState extends State<_WifiSection> {
  final _ssidCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  bool _obscure = true;

  @override
  void dispose() {
    _ssidCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('WiFi 配网', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 8),
            if (widget.model.deviceWifiConnected)
              Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: Row(children: [
                  const Icon(Icons.wifi, color: Colors.green, size: 18),
                  const SizedBox(width: 6),
                  Text('设备已联网: ${widget.model.deviceSSID}'),
                  const Spacer(),
                  Text(widget.model.deviceIP ?? '', style: const TextStyle(fontFamily: 'monospace')),
                ]),
              ),
            TextField(
              controller: _ssidCtrl,
              decoration: const InputDecoration(labelText: 'WiFi 名称', border: OutlineInputBorder(), isDense: true),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: _passCtrl,
              obscureText: _obscure,
              decoration: InputDecoration(
                labelText: 'WiFi 密码',
                border: const OutlineInputBorder(),
                isDense: true,
                suffixIcon: IconButton(
                  icon: Icon(_obscure ? Icons.visibility : Icons.visibility_off),
                  onPressed: () => setState(() => _obscure = !_obscure),
                ),
              ),
            ),
            const SizedBox(height: 12),
            Wrap(spacing: 8, children: [
              FilledButton.icon(
                onPressed: () => widget.model.sendSetWifi(_ssidCtrl.text, _passCtrl.text),
                icon: const Icon(Icons.wifi_protected_setup),
                label: const Text('发送配网'),
              ),
              OutlinedButton.icon(
                onPressed: () => widget.model.sendGetWifiStatus(),
                icon: const Icon(Icons.refresh),
                label: const Text('查询状态'),
              ),
              if (widget.model.deviceIP != null)
                OutlinedButton.icon(
                  onPressed: () => widget.model.connectWifi(widget.model.deviceIP!),
                  icon: const Icon(Icons.cable),
                  label: const Text('连接 TCP'),
                ),
            ]),
          ],
        ),
      ),
    );
  }
}


class _SavedDeviceCard extends StatelessWidget {
  final AppModel model;
  const _SavedDeviceCard({required this.model});

  @override
  Widget build(BuildContext context) {
    return Card(
      color: Colors.green.shade50,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              const Icon(Icons.bookmark, color: Colors.green, size: 20),
              const SizedBox(width: 8),
              Text('已保存的设备', style: Theme.of(context).textTheme.titleMedium),
            ]),
            const SizedBox(height: 8),
            Row(children: [
              const Icon(Icons.wifi, size: 18, color: Colors.grey),
              const SizedBox(width: 6),
              Text(model.savedDeviceSSID ?? ''),
            ]),
            const SizedBox(height: 4),
            Row(children: [
              const Icon(Icons.computer, size: 18, color: Colors.grey),
              const SizedBox(width: 6),
              Text(model.savedDeviceIP ?? '', style: const TextStyle(fontFamily: 'monospace', fontSize: 16)),
            ]),
            const SizedBox(height: 12),
            Row(children: [
              FilledButton.icon(
                onPressed: () => model.connectWifi(model.savedDeviceIP!),
                icon: const Icon(Icons.cable),
                label: const Text('快速连接'),
              ),
              const SizedBox(width: 8),
              TextButton.icon(
                onPressed: () => model.clearSavedDeviceIP(),
                icon: const Icon(Icons.delete_outline, size: 20),
                label: const Text('清除'),
              ),
            ]),
          ],
        ),
      ),
    );
  }
}
