import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../services/app_model.dart';

class ControlScreen extends StatelessWidget {
  const ControlScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final s = model.status;

    return Scaffold(
      appBar: AppBar(
        title: const Text('绕线控制'),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 16),
            child: Center(child: Text('链路: ${model.activeLink}')),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (model.lastError != null) _ErrorBanner(model: model),
          _StateIndicator(state: s.state),
          const SizedBox(height: 16),
          _MonitoringGrid(model: model),
          const SizedBox(height: 16),
          _TraverseIndicator(model: model),
          const SizedBox(height: 16),
          _SpeedControl(model: model),
          const SizedBox(height: 16),
          _ControlButtons(model: model),
        ],
      ),
    );
  }
}

class _StateIndicator extends StatelessWidget {
  final String state;
  const _StateIndicator({required this.state});

  @override
  Widget build(BuildContext context) {
    final (color, label) = _stateInfo(state);
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 16),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: color.withValues(alpha: 0.3)),
      ),
      child: Row(children: [
        Icon(Icons.circle, color: color, size: 12),
        const SizedBox(width: 8),
        Text(label, style: TextStyle(color: color, fontWeight: FontWeight.w600, fontSize: 15)),
      ]),
    );
  }

  (Color, String) _stateInfo(String s) {
    switch (s) {
      case 'running': return (Colors.green, '运行中');
      case 'paused': return (Colors.orange, '已暂停');
      case 'homing': return (Colors.blue, '寻原点中');
      case 'positioning': return (Colors.blue, '定位中');
      case 'calibrating': return (Colors.purple, '校准中');
      case 'error': return (Colors.red, '异常: $s');
      case 'completed': return (Colors.teal, '已完成');
      default: return (Colors.grey, '待机');
    }
  }
}

class _ErrorBanner extends StatelessWidget {
  final AppModel model;
  const _ErrorBanner({required this.model});

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.red.shade50,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Colors.red.shade200),
      ),
      child: Row(children: [
        const Icon(Icons.error_outline, color: Colors.red),
        const SizedBox(width: 8),
        Expanded(child: Text(model.lastError!, style: TextStyle(color: Colors.red.shade800))),
        TextButton(
          onPressed: () {
            model.sendClearError();
            model.clearLastError();
          },
          child: const Text('清除'),
        ),
      ]),
    );
  }
}

class _MonitoringGrid extends StatelessWidget {
  final AppModel model;
  const _MonitoringGrid({required this.model});

  @override
  Widget build(BuildContext context) {
    final s = model.status;
    final theme = Theme.of(context);
    return GridView.count(
      crossAxisCount: 2,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      mainAxisSpacing: 8,
      crossAxisSpacing: 8,
      childAspectRatio: 2.2,
      children: [
        _Metric(label: '转速', value: '${s.spoolRpm.toStringAsFixed(1)}', unit: 'RPM'),
        _Metric(label: '料盘圈数', value: '${s.spoolTurns.toStringAsFixed(1)}', unit: '圈'),
        _Metric(label: '出线长度', value: '${s.lengthMeasured.toStringAsFixed(2)}', unit: 'm'),
        _Metric(label: '理论长度', value: '${s.lengthTheoretical.toStringAsFixed(2)}', unit: 'm'),
        _Metric(label: '有效直径', value: '${s.effectiveDiameter.toStringAsFixed(1)}', unit: 'mm'),
        _Metric(label: '当前层数', value: '${s.currentLayer}', unit: '层'),
      ],
    );
  }
}

class _Metric extends StatelessWidget {
  final String label;
  final String value;
  final String unit;
  const _Metric({required this.label, required this.value, required this.unit});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text(label, style: const TextStyle(fontSize: 12, color: Colors.grey)),
            Row(
              crossAxisAlignment: CrossAxisAlignment.baseline,
              textBaseline: TextBaseline.alphabetic,
              children: [
                Text(value, style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
                const SizedBox(width: 4),
                Text(unit, style: const TextStyle(fontSize: 13, color: Colors.grey)),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _TraverseIndicator extends StatelessWidget {
  final AppModel model;
  const _TraverseIndicator({required this.model});

  @override
  Widget build(BuildContext context) {
    final s = model.status;
    final pos = s.traversePos;
    final right = model.config.traverseRightEnd;
    final left = model.config.traverseLeftStart;
    final range = (right - left).clamp(0.1, double.infinity);
    final pct = ((pos - left) / range).clamp(0.0, 1.0);

    final dirIcon = s.traverseDir == 'right'
        ? Icons.arrow_forward
        : s.traverseDir == 'left'
            ? Icons.arrow_back
            : Icons.pause;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              const Text('排线位置', style: TextStyle(fontSize: 13, color: Colors.grey)),
              const Spacer(),
              Icon(dirIcon, size: 18),
              const SizedBox(width: 4),
              Text('${pos.toStringAsFixed(1)} mm'),
            ]),
            const SizedBox(height: 10),
            ClipRRect(
              borderRadius: BorderRadius.circular(4),
              child: LinearProgressIndicator(
                value: pct,
                minHeight: 8,
                backgroundColor: Colors.grey.shade200,
              ),
            ),
            const SizedBox(height: 6),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text('${left.toStringAsFixed(0)} mm', style: const TextStyle(fontSize: 11, color: Colors.grey)),
                Text('来回 ${s.roundTrips} | 距校准 ${s.calibCountdown}',
                    style: const TextStyle(fontSize: 11, color: Colors.grey)),
                Text('${right.toStringAsFixed(0)} mm', style: const TextStyle(fontSize: 11, color: Colors.grey)),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _SpeedControl extends StatefulWidget {
  final AppModel model;
  const _SpeedControl({required this.model});

  @override
  State<_SpeedControl> createState() => _SpeedControlState();
}

class _SpeedControlState extends State<_SpeedControl> {
  bool _dragging = false;
  double _dragValue = 0;

  @override
  Widget build(BuildContext context) {
    final model = widget.model;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              const Text('运行速度'),
              const Spacer(),
              Text('${(_dragging ? _dragValue : model.targetSpeed.toDouble()).round()}%',
                  style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            ]),
            Slider(
              value: (_dragging ? _dragValue : model.targetSpeed.toDouble()).clamp(0, 100),
              min: 0,
              max: 100,
              divisions: 100,
              onChangeStart: (v) {
                _dragging = true;
                _dragValue = v;
              },
              onChanged: (v) {
                setState(() => _dragValue = v);
                model.sendSetSpeed(v.round());  // 只发指令，不动 model，避免打断手势
              },
              onChangeEnd: (v) {
                model.targetSpeed = v.round();  // 松手才提交
                setState(() => _dragging = false);
              },
            ),
          ],
        ),
      ),
    );
  }
}

class _ControlButtons extends StatelessWidget {
  final AppModel model;
  const _ControlButtons({required this.model});

  @override
  Widget build(BuildContext context) {
    final state = model.status.state;
    final isRunning = state == 'running';
    final isPaused = state == 'paused';

    return Wrap(
      alignment: WrapAlignment.center,
      spacing: 8,
      runSpacing: 8,
      children: [
        FilledButton.icon(
          onPressed: (isRunning || isPaused) ? null : () => model.sendStart(model.targetSpeed),
          icon: const Icon(Icons.play_arrow),
          label: const Text('启动'),
        ),
        FilledButton.tonalIcon(
          onPressed: isRunning ? model.sendPause : null,
          icon: const Icon(Icons.pause),
          label: const Text('暂停'),
        ),
        FilledButton.tonalIcon(
          onPressed: isPaused ? model.sendResume : null,
          icon: const Icon(Icons.fast_forward),
          label: const Text('恢复'),
        ),
        FilledButton.tonalIcon(
          onPressed: (state == 'idle' || isPaused) ? model.sendHome : null,
          icon: const Icon(Icons.home),
          label: const Text('回原点'),
        ),
        FilledButton.icon(
          onPressed: state == 'idle' ? null : model.sendStop,
          icon: const Icon(Icons.stop),
          label: const Text('停止'),
          style: FilledButton.styleFrom(backgroundColor: Colors.red),
        ),
      ],
    );
  }
}
