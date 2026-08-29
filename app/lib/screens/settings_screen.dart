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
          _PushConfigButton(model: model),
          const SizedBox(height: 8),
          _ParamCategory(title: '驱动模式', children: [
            _DropdownField(label: '驱动方式', value: model.config.driveMode,
              items: const [
                DropdownMenuItem(value: 0, child: Text('电动（电机驱动）')),
                DropdownMenuItem(value: 1, child: Text('手动（手摇驱动）')),
              ],
              onChanged: (v) { if (v != null) { model.config.driveMode = v; model.notifyListeners(); } },
            ),
            Text(
              model.config.driveMode == 1
                ? '手动模式：电机不输出，手摇驱动料盘，排线按实测转速自动跟随。'
                  '缠料检测关闭；周期校准改为「手摇停转约 2 秒且来回数达标」时自动触发。'
                : '电动模式：电机按设定转速驱动料盘，电机运转但料盘停转会触发缠料保护。'
                  '每完成设定个来回自动校准一次排线位置。',
              style: const TextStyle(fontSize: 12, color: Colors.grey, height: 1.5),
            ),
            _DropdownField(label: '电机驱动电路', value: model.config.motorDriver,
              items: const [
                DropdownMenuItem(value: 0, child: Text('MOS 管调速（GPIO 输出 PWM）')),
                DropdownMenuItem(value: 1, child: Text('L298N 开关（不调速，ENA 插跳线）')),
              ],
              onChanged: (v) { if (v != null) { model.config.motorDriver = v; model.notifyListeners(); } },
            ),
            Text(
              model.config.motorDriver == 1
                ? 'L298N 接法：ENA 插跳线帽（全速），ESP32 电机引脚接 IN1，IN2 接 GND，'
                  '电机方向由接线决定。只能启停，不能调速。'
                : 'MOS 管接法：GPIO 经 150Ω 接栅极，10kΩ 下拉到 GND，电机两端并联续流二极管。',
              style: const TextStyle(fontSize: 12, color: Colors.grey, height: 1.5),
            ),
          ]),
          _ParamCategory(title: '引脚配置', children: [
            _NumField(label: '收线盘 PWM', value: model.config.pinMotorPwm.toDouble(),
                onChanged: (v) => model.config.pinMotorPwm = v.round()),
            _NumField(label: '排线舵机 PWM', value: model.config.pinServoPwm.toDouble(),
                onChanged: (v) => model.config.pinServoPwm = v.round()),
            _NumField(label: 'Endstop', value: model.config.pinEndstop.toDouble(),
                onChanged: (v) => model.config.pinEndstop = v.round()),
            _NumField(label: 'Endstop 右', value: model.config.pinEndstopRight.toDouble(),
                onChanged: (v) => model.config.pinEndstopRight = v.round()),
            _NumField(label: '霍尔 B (料盘)', value: model.config.pinHallSpool.toDouble(),
                onChanged: (v) => model.config.pinHallSpool = v.round()),
          ]),
          _ParamCategory(title: '传感器参数', children: [
            _NumField(label: '料盘磁铁数', value: model.config.hallSpoolMagnets.toDouble(),
                onChanged: (v) => model.config.hallSpoolMagnets = v.round()),
            _NumField(label: '霍尔去抖 (us)', value: model.config.hallDebounceUs.toDouble(),
                onChanged: (v) => model.config.hallDebounceUs = v.round()),
            const Text(
              '去抖时间同时是脉冲间隔下限（25000us ≈ 8 磁铁最大 300RPM），'
              '用于滤除电机 PWM 耦合到霍尔线的噪声。若真实转速超过 300RPM 需调小。',
              style: TextStyle(fontSize: 12, color: Colors.grey, height: 1.5),
            ),
          ]),
          _ParamCategory(title: '料盘参数', children: [
            _NumField(label: '料盘外径 (mm)', value: model.config.spoolOuterDiameter,
                onChanged: (v) => model.config.spoolOuterDiameter = v),
            _NumField(label: '料盘宽度 (mm)', value: model.config.spoolWidth,
                onChanged: model.setSpoolWidth),
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
            _TraverseRangeFields(model: model),
            _NumField(label: '单圈移动距离 (mm)', value: model.config.traverseDistPerRev,
                onChanged: (v) => model.config.traverseDistPerRev = v),
            _NumField(label: '丝杆导程 (mm)', value: model.config.leadScrewPitch,
                onChanged: (v) => model.config.leadScrewPitch = v),
            _NumField(label: '限位间距 (mm)', value: model.config.travelRangeMm,
                onChanged: (v) => model.config.travelRangeMm = v),
            _NumField(label: model.config.driveMode == 1 ? '校准间隔 (来回, 停转触发)' : '校准间隔 (来回)',
                value: model.config.calIntervalRounds.toDouble(),
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
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(children: [
                    Icon(Icons.speed, color: Theme.of(context).colorScheme.primary),
                    const SizedBox(width: 8),
                    Text('舵机速度标定', style: Theme.of(context).textTheme.titleMedium),
                  ]),
                  const SizedBox(height: 8),
                  const Text(
                    '自动测量排线左右行速度（需双 Endstop）。标定时排线会满速左右运动 3 个来回。',
                    style: TextStyle(fontSize: 12, color: Colors.grey, height: 1.5),
                  ),
                  const SizedBox(height: 8),
                  if (model.config.servoTraverseSpeedRight > 0 || model.config.servoTraverseSpeedLeft > 0)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: Text(
                        '当前: 右行 ${model.config.servoTraverseSpeedRight.toStringAsFixed(1)} mm/s  左行 ${model.config.servoTraverseSpeedLeft.toStringAsFixed(1)} mm/s',
                        style: TextStyle(fontSize: 13, color: Colors.green.shade700),
                      ),
                    ),
                  FilledButton.icon(
                    onPressed: model.status.state == 'idle'
                        ? () {
                            showDialog(
                              context: context,
                              builder: (ctx) => AlertDialog(
                                title: const Text('舵机速度标定'),
                                content: const Text('排线将满速左右运动 3 个来回来自动测量速度。确认开始？'),
                                actions: [
                                  TextButton(
                                    onPressed: () => Navigator.pop(ctx),
                                    child: const Text('取消'),
                                  ),
                                  FilledButton(
                                    onPressed: () {
                                      Navigator.pop(ctx);
                                      model.sendCalibrateServo();
                                      ScaffoldMessenger.of(context).showSnackBar(
                                        const SnackBar(content: Text('标定已开始，请等待完成')),
                                      );
                                    },
                                    child: const Text('开始'),
                                  ),
                                ],
                              ),
                            );
                          }
                        : null,
                    icon: const Icon(Icons.play_arrow),
                    label: Text(model.status.state == 'servo_calib' ? '标定中...' : '开始标定'),
                  ),
                ],
              ),
            ),
          ),
          _ParamCategory(title: '排线编码器 (可选 AS5600)', children: [
            _DropdownField(label: '位置反馈', value: model.config.traverseEncoder,
              items: const [
                DropdownMenuItem(value: 0, child: Text('舵机开环估算（默认）')),
                DropdownMenuItem(value: 1, child: Text('AS5600 编码器闭环')),
              ],
              onChanged: (v) { if (v != null) { model.config.traverseEncoder = v; model.notifyListeners(); } },
            ),
            _NumField(label: '编码器 SDA', value: model.config.pinEncSda.toDouble(),
                onChanged: (v) => model.config.pinEncSda = v.round()),
            _NumField(label: '编码器 SCL', value: model.config.pinEncScl.toDouble(),
                onChanged: (v) => model.config.pinEncScl = v.round()),
            _NumField(label: '增速齿比 (丝杆/编码器)', value: model.config.encGearRatio,
                onChanged: (v) => model.config.encGearRatio = v),
            Text(
              model.config.traverseEncoder == 1
                ? '闭环模式：AS5600 装在舵机输出轴（未减速端）。齿比 = 丝杆转速 ÷ 编码器轴转速。'
                  '切换后需跑一次「舵机速度标定」，会自动测出每圈位移并写入。'
                : '未启用。装好 AS5600（SDA/SCL/3.3V/GND）后切到闭环可大幅提升排线定位精度。',
              style: const TextStyle(fontSize: 12, color: Colors.grey, height: 1.5),
            ),
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
              title: const Text('WiFi 断开停机'),
              value: model.config.wifiDisconnectStop,
              onChanged: (v) { model.config.wifiDisconnectStop = v; model.notifyListeners(); },
            ),
          ]),
          _LicenseSection(model: model),
          _FirmwareSection(model: model),
          _DangerZone(model: model),
        ],
      ),
    );
  }
}

// ============================================================================
//  固件在线升级（OTA）
// ============================================================================
class _FirmwareSection extends StatelessWidget {
  final AppModel model;
  const _FirmwareSection({required this.model});

  @override
  Widget build(BuildContext context) {
    final m = model;
    return Card(
      margin: const EdgeInsets.only(top: 16),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              const Expanded(child: Text('固件升级',
                  style: TextStyle(fontWeight: FontWeight.bold))),
              Icon(m.fwChecking ? Icons.sync : (m.fwUpdateAvailable ? Icons.system_update_alt : Icons.check_circle_outline),
                  color: m.fwChecking ? Colors.blue : (m.fwUpdateAvailable ? Colors.orange : Colors.green), size: 20),
              const SizedBox(width: 4),
              Text(m.fwChecking ? '检查中…' : (m.fwUpdateAvailable ? '有新版本' : '最新'),
                  style: TextStyle(
                      color: m.fwChecking ? Colors.blue : (m.fwUpdateAvailable ? Colors.orange : Colors.green),
                      fontWeight: FontWeight.bold)),
            ]),
            const SizedBox(height: 8),
            Text('当前版本: v${m.fwCurrent ?? "--"}',
                style: const TextStyle(fontSize: 13)),
            if (m.fwUpdateAvailable && m.fwLatest != null)
              Text('最新版本: v${m.fwLatest}',
                  style: const TextStyle(fontSize: 13, color: Colors.orange)),
            if (m.fwNotes != null && m.fwNotes!.isNotEmpty)
              Text(m.fwNotes!,
                  style: const TextStyle(fontSize: 12, color: Colors.grey)),
            if (m.fwMsg != null && m.fwMsg!.isNotEmpty)
              Text(m.fwMsg!,
                  style: const TextStyle(fontSize: 12, color: Colors.blueGrey)),
            const SizedBox(height: 10),
            Row(children: [
              OutlinedButton(
                onPressed: m.fwChecking
                    ? null
                    : () {
                        model.sendOtaCheck();
                      },
                child: m.fwChecking
                    ? const SizedBox(width: 16, height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2))
                    : const Text('检查更新'),
              ),
              const SizedBox(width: 8),
              FilledButton(
                onPressed: m.fwUpdateAvailable
                    ? () {
                        model.sendOtaStart();
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('开始升级，完成后设备将自动重启（约1-2分钟）')),
                        );
                      }
                    : null,
                child: const Text('立即升级'),
              ),
            ]),
          ],
        ),
      ),
    );
  }
}

// ============================================================================
//  设备授权（粘贴许可证激活）
// ============================================================================
class _LicenseSection extends StatefulWidget {
  final AppModel model;
  const _LicenseSection({required this.model});

  @override
  State<_LicenseSection> createState() => _LicenseSectionState();
}

class _LicenseSectionState extends State<_LicenseSection> {
  final _keyCtrl = TextEditingController();
  final _nameCtrl = TextEditingController();
  final _contactCtrl = TextEditingController();
  String? _lastAutoFill;

  @override
  void initState() {
    super.initState();
    // 进入页面时查询一次授权状态
    WidgetsBinding.instance.addPostFrameCallback((_) {
      widget.model.sendLicenseQuery();
    });
    // 服务器返回许可证时自动填入输入框（同一张许可证只填一次）
    widget.model.addListener(_autoFill);
  }

  void _autoFill() {
    final m = widget.model;
    if (m.serverLicense != null &&
        m.serverLicense != _lastAutoFill &&
        m.serverLicense!.isNotEmpty) {
      _lastAutoFill = m.serverLicense;
      _keyCtrl.text = m.serverLicense!;
    }
  }

  @override
  void dispose() {
    widget.model.removeListener(_autoFill);
    _keyCtrl.dispose();
    _nameCtrl.dispose();
    _contactCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final m = widget.model;
    return Card(
      margin: const EdgeInsets.only(top: 16),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              const Expanded(child: Text('设备授权', style: TextStyle(fontWeight: FontWeight.bold))),
              Icon(m.licenseOk ? Icons.verified : Icons.warning_amber_rounded,
                  color: m.licenseOk ? Colors.green : Colors.orange, size: 20),
              const SizedBox(width: 4),
              Text(m.licenseOk ? '已授权' : '未授权',
                  style: TextStyle(color: m.licenseOk ? Colors.green : Colors.orange,
                      fontWeight: FontWeight.bold)),
            ]),
            const SizedBox(height: 8),
            Text('设备ID: ${m.licenseDeviceId ?? "--"}',
                style: const TextStyle(fontSize: 13, fontFeatures: [FontFeature.tabularFigures()])),
            if (m.licenseOk && m.licenseExpiry != null)
              Text('有效期至: ${m.licenseExpiry}',
                  style: const TextStyle(fontSize: 13, color: Colors.grey)),
            const SizedBox(height: 12),
            TextField(
              controller: _keyCtrl,
              decoration: const InputDecoration(
                labelText: '许可证',
                hintText: '粘贴许可证字符串',
                isDense: true,
                border: OutlineInputBorder(),
              ),
              style: const TextStyle(fontSize: 12),
              maxLines: 2,
            ),
            const SizedBox(height: 8),
            TextField(
              controller: _nameCtrl,
              decoration: const InputDecoration(
                labelText: '昵称/QQ号（一键申请必填）',
                isDense: true,
                border: OutlineInputBorder(),
              ),
              style: const TextStyle(fontSize: 13),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: _contactCtrl,
              decoration: const InputDecoration(
                labelText: '联系方式（选填）',
                isDense: true,
                border: OutlineInputBorder(),
              ),
              style: const TextStyle(fontSize: 13),
            ),
            const SizedBox(height: 10),
            Row(children: [
              Expanded(
                child: FilledButton(
                  onPressed: _keyCtrl.text.trim().isEmpty
                      ? null
                      : () {
                          m.sendLicenseActivate(_keyCtrl.text.trim());
                          ScaffoldMessenger.of(context).showSnackBar(
                            const SnackBar(content: Text('激活请求已发送')),
                          );
                        },
                  child: const Text('激活'),
                ),
              ),
              const SizedBox(width: 8),
              OutlinedButton(
                onPressed: () {
                  if (_nameCtrl.text.trim().isEmpty) {
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('请先填写昵称/QQ号')),
                    );
                    return;
                  }
                  m.sendLicenseApply(
                      _nameCtrl.text.trim(), _contactCtrl.text.trim());
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('申请已提交，请稍候…')),
                  );
                },
                child: const Text('一键申请'),
              ),
              const SizedBox(width: 8),
              OutlinedButton(
                onPressed: () {
                  m.sendLicenseFetch();
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('正在查询…')),
                  );
                },
                child: const Text('查询'),
              ),
            ]),
            if (m.serverLicenseMsg != null) ...[
              const SizedBox(height: 8),
              Text(m.serverLicenseMsg!,
                  style: const TextStyle(fontSize: 12, color: Colors.blueGrey)),
            ],
            const SizedBox(height: 8),
            OutlinedButton(
              onPressed: () => m.sendLicenseQuery(),
              child: const Text('刷新状态'),
            ),
            const SizedBox(height: 8),
            const Text(
              '未授权时无法启动绕线。填昵称后点「一键申请」自动获取许可证（需设备联网），审核通过后点「查询」取回并激活。',
              style: TextStyle(fontSize: 12, color: Colors.grey, height: 1.5),
            ),
          ],
        ),
      ),
    );
  }
}

// ============================================================================
//  绕线范围（右基准 + 宽度推导左起始）
//  左起始 = 右终止 − 料盘宽度，自动推导；特殊安装可切手动指定。
// ============================================================================
class _TraverseRangeFields extends StatelessWidget {
  final AppModel model;
  const _TraverseRangeFields({required this.model});

  @override
  Widget build(BuildContext context) {
    final c = model.config;
    final width = c.traverseRightEnd - c.traverseLeftStart;
    final mismatch = (width - c.spoolWidth).abs() > 0.5;

    return Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
      _NumField(label: '右终止位置 (mm)', value: c.traverseRightEnd,
          onChanged: model.setTraverseRightEnd),
      if (model.leftAutoDerive)
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 8),
          child: Text('左起始位置 = ${(c.traverseLeftStart).toStringAsFixed(1)} mm（= 右终止 − 料盘宽度，自动）',
              style: const TextStyle(fontSize: 13, color: Colors.blueGrey)),
        )
      else
        _NumField(label: '左起始位置 (mm)', value: c.traverseLeftStart,
            onChanged: (v) => c.traverseLeftStart = v),
      SwitchListTile(
        dense: true,
        title: const Text('左起始手动指定'),
        subtitle: const Text('料盘左右位置都不固定时打开', style: TextStyle(fontSize: 11)),
        value: !model.leftAutoDerive,
        onChanged: (v) { model.leftAutoDerive = !v; model.notifyListeners(); },
      ),
      Text(
        mismatch
            ? '⚠ 绕线宽度 ${width.toStringAsFixed(1)}mm 与料盘宽度 ${c.spoolWidth.toStringAsFixed(1)}mm 不一致'
            : '绕线宽度 ${width.toStringAsFixed(1)}mm，与料盘宽度一致 ✓',
        style: TextStyle(
            fontSize: 12,
            color: mismatch ? Colors.orange.shade800 : Colors.grey,
            height: 1.5),
      ),
    ]);
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
