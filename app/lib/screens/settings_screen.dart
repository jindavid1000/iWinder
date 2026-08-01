import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../services/app_model.dart';

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();

    return Scaffold(
      appBar: AppBar(title: const Text('参数设置')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _PresetSection(model: model),
          const SizedBox(height: 16),
          _PushConfigButton(model: model),
          const SizedBox(height: 8),
          _ParamCategory(title: '引脚配置', children: [
            _NumField(label: '收线盘 PWM', value: model.config.pinMotorPwm.toDouble(),
                onChanged: (v) => model.config.pinMotorPwm = v.round()),
            _NumField(label: '排线舵机 PWM', value: model.config.pinServoPwm.toDouble(),
                onChanged: (v) => model.config.pinServoPwm = v.round()),
            _NumField(label: 'Endstop', value: model.config.pinEndstop.toDouble(),
                onChanged: (v) => model.config.pinEndstop = v.round()),
            _NumField(label: '霍尔 A (从动轮)', value: model.config.pinHallIdler.toDouble(),
                onChanged: (v) => model.config.pinHallIdler = v.round()),
            _NumField(label: '霍尔 B (料盘)', value: model.config.pinHallSpool.toDouble(),
                onChanged: (v) => model.config.pinHallSpool = v.round()),
          ]),
          _ParamCategory(title: '传感器参数', children: [
            _NumField(label: '从动轮磁铁数', value: model.config.hallIdlerMagnets.toDouble(),
                onChanged: (v) => model.config.hallIdlerMagnets = v.round()),
            _NumField(label: '料盘磁铁数', value: model.config.hallSpoolMagnets.toDouble(),
                onChanged: (v) => model.config.hallSpoolMagnets = v.round()),
            _NumField(label: '从动轮直径 (mm)', value: model.config.idlerDiameter,
                onChanged: (v) => model.config.idlerDiameter = v),
          ]),
          _ParamCategory(title: '料盘参数', children: [
            _NumField(label: '料盘外径 (mm)', value: model.config.spoolOuterDiameter,
                onChanged: (v) => model.config.spoolOuterDiameter = v),
            _NumField(label: '料盘宽度 (mm)', value: model.config.spoolWidth,
                onChanged: (v) => model.config.spoolWidth = v),
            _NumField(label: '有纸筒直径 (mm)', value: model.config.spoolCoreDiaWithCard,
                onChanged: (v) => model.config.spoolCoreDiaWithCard = v),
            _NumField(label: '无纸筒直径 (mm)', value: model.config.spoolCoreDiaNoCard,
                onChanged: (v) => model.config.spoolCoreDiaNoCard = v),
            _NumField(label: '线径 (mm)', value: model.config.filamentDiameter,
                onChanged: (v) => model.config.filamentDiameter = v),
            SwitchListTile(
              title: const Text('有纸筒'),
              value: model.config.spoolHasCardboard,
              onChanged: (v) { model.config.spoolHasCardboard = v; model.notifyListeners(); },
            ),
          ]),
          _ParamCategory(title: '运动参数', children: [
            _NumField(label: '左起始位置 (mm)', value: model.config.traverseLeftStart,
                onChanged: (v) => model.config.traverseLeftStart = v),
            _NumField(label: '右终止位置 (mm)', value: model.config.traverseRightEnd,
                onChanged: (v) => model.config.traverseRightEnd = v),
            _NumField(label: '单圈移动距离 (mm)', value: model.config.traverseDistPerRev,
                onChanged: (v) => model.config.traverseDistPerRev = v),
            _NumField(label: '丝杆导程 (mm)', value: model.config.leadScrewPitch,
                onChanged: (v) => model.config.leadScrewPitch = v),
            _NumField(label: '校准间隔 (来回)', value: model.config.calIntervalRounds.toDouble(),
                onChanged: (v) => model.config.calIntervalRounds = v.round()),
          ]),
          _ParamCategory(title: '舵机参数', children: [
            _NumField(label: '停止 PWM (us)', value: model.config.servoStopPulse.toDouble(),
                onChanged: (v) => model.config.servoStopPulse = v.round()),
            _NumField(label: '左行 PWM (us)', value: model.config.servoLeftPulse.toDouble(),
                onChanged: (v) => model.config.servoLeftPulse = v.round()),
            _NumField(label: '右行 PWM (us)', value: model.config.servoRightPulse.toDouble(),
                onChanged: (v) => model.config.servoRightPulse = v.round()),
            _NumField(label: '寻原点 PWM (us)', value: model.config.servoHomePulse.toDouble(),
                onChanged: (v) => model.config.servoHomePulse = v.round()),
            _NumField(label: '右行速度 (mm/s)', value: model.config.servoTraverseSpeedRight,
                onChanged: (v) => model.config.servoTraverseSpeedRight = v),
            _NumField(label: '左行速度 (mm/s)', value: model.config.servoTraverseSpeedLeft,
                onChanged: (v) => model.config.servoTraverseSpeedLeft = v),
          ]),
          _ParamCategory(title: '电机参数', children: [
            _NumField(label: '最低转速 (%)', value: model.config.motorMinSpeed.toDouble(),
                onChanged: (v) => model.config.motorMinSpeed = v.round()),
            _NumField(label: '默认转速 (%)', value: model.config.motorDefaultSpeed.toDouble(),
                onChanged: (v) => model.config.motorDefaultSpeed = v.round()),
            _NumField(label: '最高转速 (%)', value: model.config.motorMaxSpeed.toDouble(),
                onChanged: (v) => model.config.motorMaxSpeed = v.round()),
            _NumField(label: '软启动时间 (ms)', value: model.config.motorSoftStartMs.toDouble(),
                onChanged: (v) => model.config.motorSoftStartMs = v.round()),
          ]),
          _ParamCategory(title: '打滑检测', children: [
            _NumField(label: '容差阈值 (%)', value: model.config.slipTolerance,
                onChanged: (v) => model.config.slipTolerance = v),
            _NumField(label: '卡线超时 (s)', value: model.config.stallTimeoutS,
                onChanged: (v) => model.config.stallTimeoutS = v),
          ]),
          _ParamCategory(title: '任务完成', children: [
            _DropdownField(label: '完成模式', value: model.config.autoStopMode,
              items: const [
                DropdownMenuItem(value: 0, child: Text('手动停止')),
                DropdownMenuItem(value: 1, child: Text('目标长度')),
                DropdownMenuItem(value: 2, child: Text('目标圈数')),
              ],
              onChanged: (v) { if (v != null) { model.config.autoStopMode = v; model.notifyListeners(); } },
            ),
            if (model.config.autoStopMode == 1)
              _NumField(label: '目标长度 (m)', value: model.config.targetLengthM,
                  onChanged: (v) => model.config.targetLengthM = v),
            if (model.config.autoStopMode == 2)
              _NumField(label: '目标圈数', value: model.config.targetTurns.toDouble(),
                  onChanged: (v) => model.config.targetTurns = v.round()),
          ]),
          _ParamCategory(title: '通信参数', children: [
            _NumField(label: '状态上报间隔 (ms)', value: model.config.statusReportIntervalMs.toDouble(),
                onChanged: (v) => model.config.statusReportIntervalMs = v.round()),
            SwitchListTile(
              title: const Text('蓝牙断开停机'),
              value: model.config.bleDisconnectStop,
              onChanged: (v) { model.config.bleDisconnectStop = v; model.notifyListeners(); },
            ),
            SwitchListTile(
              title: const Text('WiFi 断开停机'),
              value: model.config.wifiDisconnectStop,
              onChanged: (v) { model.config.wifiDisconnectStop = v; model.notifyListeners(); },
            ),
          ]),
          _DangerZone(model: model),
        ],
      ),
    );
  }
}

class _PresetSection extends StatelessWidget {
  final AppModel model;
  const _PresetSection({required this.model});

  void _showSaveDialog(BuildContext context) {
    final ctrl = TextEditingController();
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('保存预设'),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          decoration: const InputDecoration(labelText: '预设名称', border: OutlineInputBorder()),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
          FilledButton(
            onPressed: () {
              if (ctrl.text.isNotEmpty) {
                model.sendSavePreset(ctrl.text);
                Navigator.pop(ctx);
              }
            },
            child: const Text('保存'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    String? selected;
    if (model.presets.isNotEmpty) selected = model.presets.first;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('预设方案', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 12),
            if (model.presets.isEmpty)
              const Text('暂无预设', style: TextStyle(color: Colors.grey))
            else
              Wrap(
                spacing: 6,
                runSpacing: 6,
                children: model.presets.map((name) {
                  return Chip(
                    label: Text(name),
                    deleteIcon: const Icon(Icons.close, size: 18),
                    onDeleted: () {
                      showDialog(
                        context: context,
                        builder: (ctx) => AlertDialog(
                          title: Text('删除预设 "$name"?'),
                          actions: [
                            TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
                            FilledButton(
                              onPressed: () { model.sendDeletePreset(name); Navigator.pop(ctx); },
                              style: FilledButton.styleFrom(backgroundColor: Colors.red),
                              child: const Text('删除'),
                            ),
                          ],
                        ),
                      );
                    },
                  );
                }).toList(),
              ),
            const SizedBox(height: 12),
            Wrap(spacing: 8, children: [
              OutlinedButton.icon(
                onPressed: model.presets.isEmpty ? null : null,
                icon: const Icon(Icons.download),
                label: const Text('加载'),
              ),
              FilledButton.icon(
                onPressed: () => _showSaveDialog(context),
                icon: const Icon(Icons.save),
                label: const Text('保存当前'),
              ),
            ]),
          ],
        ),
      ),
    );
  }
}

class _PushConfigButton extends StatelessWidget {
  final AppModel model;
  const _PushConfigButton({required this.model});

  @override
  Widget build(BuildContext context) {
    return FilledButton.icon(
      onPressed: () {
        model.pushConfig();
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('参数已下发')),
        );
      },
      icon: const Icon(Icons.cloud_upload),
      label: const Text('下发参数到设备'),
    );
  }
}

class _ParamCategory extends StatelessWidget {
  final String title;
  final List<Widget> children;
  const _ParamCategory({required this.title, required this.children});

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

class _NumField extends StatelessWidget {
  final String label;
  final double value;
  final void Function(double) onChanged;
  const _NumField({required this.label, required this.value, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(children: [
        SizedBox(width: 130, child: Text(label)),
        Expanded(
          child: TextFormField(
            initialValue: value.toString(),
            keyboardType: const TextInputType.numberWithOptions(decimal: true),
            decoration: const InputDecoration(isDense: true, border: OutlineInputBorder()),
            onChanged: (str) {
              final v = double.tryParse(str);
              if (v != null) onChanged(v);
            },
          ),
        ),
      ]),
    );
  }
}

class _DropdownField extends StatelessWidget {
  final String label;
  final int value;
  final List<DropdownMenuItem<int>> items;
  final void Function(int?) onChanged;
  const _DropdownField({required this.label, required this.value, required this.items, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(children: [
        SizedBox(width: 130, child: Text(label)),
        Expanded(child: DropdownButtonFormField<int>(
          value: value,
          items: items,
          decoration: const InputDecoration(isDense: true, border: OutlineInputBorder()),
          onChanged: onChanged,
        )),
      ]),
    );
  }
}

class _DangerZone extends StatelessWidget {
  final AppModel model;
  const _DangerZone({required this.model});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(top: 16),
      color: Colors.red.shade50,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('危险操作', style: TextStyle(color: Colors.red.shade800, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: () {
                showDialog(
                  context: context,
                  builder: (ctx) => AlertDialog(
                    title: const Text('恢复出厂设置'),
                    content: const Text('将清除所有参数和预设，设备将重启。确认？'),
                    actions: [
                      TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
                      FilledButton(
                        onPressed: () { model.sendFactoryReset(); Navigator.pop(ctx); },
                        style: FilledButton.styleFrom(backgroundColor: Colors.red),
                        child: const Text('确认'),
                      ),
                    ],
                  ),
                );
              },
              icon: const Icon(Icons.restart_alt, color: Colors.red),
              label: const Text('恢复出厂设置'),
            ),
          ],
        ),
      ),
    );
  }
}
