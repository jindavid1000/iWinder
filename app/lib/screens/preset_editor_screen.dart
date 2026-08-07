import 'package:flutter/material.dart';

import '../models/device_config.dart';

/// 预设编辑器 — 接收初始配置，用户修改后返回新配置
class PresetEditorScreen extends StatefulWidget {
  final String title;
  final DeviceConfig initialConfig;

  const PresetEditorScreen({
    super.key,
    required this.title,
    required this.initialConfig,
  });

  @override
  State<PresetEditorScreen> createState() => _PresetEditorScreenState();
}

class _PresetEditorScreenState extends State<PresetEditorScreen> {
  late DeviceConfig _cfg;

  @override
  void initState() {
    super.initState();
    _cfg = widget.initialConfig.copy();
  }

  void _save() => Navigator.pop(context, _cfg);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        actions: [
          TextButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save, color: Colors.white),
            label: const Text('保存', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _Category(title: '引脚配置', children: [
            _Num(label: '收线盘 PWM', val: _cfg.pinMotorPwm.toDouble(),
              on: (v) => _cfg.pinMotorPwm = v.round()),
            _Num(label: '排线舵机 PWM', val: _cfg.pinServoPwm.toDouble(),
              on: (v) => _cfg.pinServoPwm = v.round()),
            _Num(label: 'Endstop', val: _cfg.pinEndstop.toDouble(),
              on: (v) => _cfg.pinEndstop = v.round()),
            _Num(label: '霍尔 B (料盘)', val: _cfg.pinHallSpool.toDouble(),
              on: (v) => _cfg.pinHallSpool = v.round()),
          ]),
          _Category(title: '传感器参数', children: [
            _Num(label: '料盘磁铁数', val: _cfg.hallSpoolMagnets.toDouble(),
              on: (v) => _cfg.hallSpoolMagnets = v.round()),
          ]),
          _Category(title: '料盘参数', children: [
            _Num(label: '料盘外径 (mm)', val: _cfg.spoolOuterDiameter,
              on: (v) => _cfg.spoolOuterDiameter = v),
            _Num(label: '料盘宽度 (mm)', val: _cfg.spoolWidth,
              on: (v) => _cfg.spoolWidth = v),
            _Num(label: '有纸筒直径 (mm)', val: _cfg.spoolCoreDiaWithCard,
              on: (v) => _cfg.spoolCoreDiaWithCard = v),
            _Num(label: '无纸筒直径 (mm)', val: _cfg.spoolCoreDiaNoCard,
              on: (v) => _cfg.spoolCoreDiaNoCard = v),
            _Num(label: '线径 (mm)', val: _cfg.filamentDiameter,
              on: (v) => _cfg.filamentDiameter = v),
            SwitchListTile(
              title: const Text('有纸筒'),
              value: _cfg.spoolHasCardboard,
              onChanged: (v) => setState(() => _cfg.spoolHasCardboard = v),
            ),
          ]),
          _Category(title: '运动参数', children: [
            _Num(label: '左起始位置 (mm)', val: _cfg.traverseLeftStart,
              on: (v) => _cfg.traverseLeftStart = v),
            _Num(label: '右终止位置 (mm)', val: _cfg.traverseRightEnd,
              on: (v) => _cfg.traverseRightEnd = v),
            _Num(label: '单圈移动距离 (mm)', val: _cfg.traverseDistPerRev,
              on: (v) => _cfg.traverseDistPerRev = v),
            _Num(label: '丝杆导程 (mm)', val: _cfg.leadScrewPitch,
              on: (v) => _cfg.leadScrewPitch = v),
            _Num(label: '校准间隔 (来回)', val: _cfg.calIntervalRounds.toDouble(),
              on: (v) => _cfg.calIntervalRounds = v.round()),
          ]),
          _Category(title: '舵机参数', children: [
            _Num(label: '停止 PWM (us)', val: _cfg.servoStopPulse.toDouble(),
              on: (v) => _cfg.servoStopPulse = v.round()),
            _Num(label: '左行 PWM (us)', val: _cfg.servoLeftPulse.toDouble(),
              on: (v) => _cfg.servoLeftPulse = v.round()),
            _Num(label: '右行 PWM (us)', val: _cfg.servoRightPulse.toDouble(),
              on: (v) => _cfg.servoRightPulse = v.round()),
            _Num(label: '寻原点 PWM (us)', val: _cfg.servoHomePulse.toDouble(),
              on: (v) => _cfg.servoHomePulse = v.round()),
            _Num(label: '右行速度 (mm/s)', val: _cfg.servoTraverseSpeedRight,
              on: (v) => _cfg.servoTraverseSpeedRight = v),
            _Num(label: '左行速度 (mm/s)', val: _cfg.servoTraverseSpeedLeft,
              on: (v) => _cfg.servoTraverseSpeedLeft = v),
          ]),
          _Category(title: '电机参数', children: [
            _Num(label: '最低转速 (%)', val: _cfg.motorMinSpeed.toDouble(),
              on: (v) => _cfg.motorMinSpeed = v.round()),
            _Num(label: '默认转速 (%)', val: _cfg.motorDefaultSpeed.toDouble(),
              on: (v) => _cfg.motorDefaultSpeed = v.round()),
            _Num(label: '最高转速 (%)', val: _cfg.motorMaxSpeed.toDouble(),
              on: (v) => _cfg.motorMaxSpeed = v.round()),
            _Num(label: '软启动时间 (ms)', val: _cfg.motorSoftStartMs.toDouble(),
              on: (v) => _cfg.motorSoftStartMs = v.round()),
         ]),
          _Category(title: '任务完成', children: [
            _DD(label: '完成模式', val: _cfg.autoStopMode, items: const [
              DropdownMenuItem(value: 0, child: Text('手动停止')),
              DropdownMenuItem(value: 1, child: Text('目标长度')),
              DropdownMenuItem(value: 2, child: Text('目标圈数')),
            ], on: (v) { if (v != null) setState(() => _cfg.autoStopMode = v); }),
            if (_cfg.autoStopMode == 1)
              _Num(label: '目标长度 (m)', val: _cfg.targetLengthM,
                on: (v) => _cfg.targetLengthM = v),
            if (_cfg.autoStopMode == 2)
              _Num(label: '目标圈数', val: _cfg.targetTurns.toDouble(),
                on: (v) => _cfg.targetTurns = v.round()),
          ]),
          _Category(title: '通信参数', children: [
            _Num(label: '状态上报间隔 (ms)', val: _cfg.statusReportIntervalMs.toDouble(),
              on: (v) => _cfg.statusReportIntervalMs = v.round()),
            SwitchListTile(
              title: const Text('蓝牙断开停机'),
              value: _cfg.bleDisconnectStop,
              onChanged: (v) => setState(() => _cfg.bleDisconnectStop = v),
            ),
            SwitchListTile(
              title: const Text('WiFi 断开停机'),
              value: _cfg.wifiDisconnectStop,
              onChanged: (v) => setState(() => _cfg.wifiDisconnectStop = v),
            ),
          ]),
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: _save,
            icon: const Icon(Icons.save),
            label: const Text('保存预设'),
          ),
        ],
      ),
    );
  }
}

class _Category extends StatelessWidget {
  final String title;
  final List<Widget> children;
  const _Category({required this.title, required this.children});
  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ExpansionTile(
        title: Text(title),
        initiallyExpanded: false,
        childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
        children: children,
      ),
    );
  }
}

class _Num extends StatelessWidget {
  final String label;
  final double val;
  final void Function(double) on;
  const _Num({required this.label, required this.val, required this.on});
  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(children: [
        SizedBox(width: 130, child: Text(label)),
        Expanded(
          child: TextFormField(
            initialValue: val.toString(),
            keyboardType: const TextInputType.numberWithOptions(decimal: true),
            decoration: const InputDecoration(isDense: true, border: OutlineInputBorder()),
            onChanged: (s) { final v = double.tryParse(s); if (v != null) on(v); },
          ),
        ),
      ]),
    );
  }
}

class _DD extends StatelessWidget {
  final String label;
  final int val;
  final List<DropdownMenuItem<int>> items;
  final void Function(int?) on;
  const _DD({required this.label, required this.val, required this.items, required this.on});
  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(children: [
        SizedBox(width: 130, child: Text(label)),
        Expanded(child: DropdownButtonFormField<int>(
          value: val, items: items,
          decoration: const InputDecoration(isDense: true, border: OutlineInputBorder()),
          onChanged: on,
        )),
      ]),
    );
  }
}
