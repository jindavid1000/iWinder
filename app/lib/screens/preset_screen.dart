import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../services/app_model.dart';
import '../models/device_config.dart';
import 'preset_editor_screen.dart';

class PresetScreen extends StatelessWidget {
  const PresetScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final c = model.config;

    return Scaffold(
      appBar: AppBar(title: const Text('预设方案')),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => _createPreset(context, model),
        icon: const Icon(Icons.add),
        label: const Text('新建预设'),
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 80),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(children: [
                    Icon(Icons.tune, color: Theme.of(context).colorScheme.primary),
                    const SizedBox(width: 8),
                    Text('当前配置', style: Theme.of(context).textTheme.titleMedium),
                  ]),
                  const Divider(height: 16),
                  _InfoRow(label: '线径', value: '${c.filamentDiameter} mm'),
                  _InfoRow(label: '料盘宽度', value: '${c.spoolWidth} mm'),
                  _InfoRow(label: '纸筒', value: c.spoolHasCardboard ? '有 (${c.spoolCoreDiaWithCard} mm)' : '无 (${c.spoolCoreDiaNoCard} mm)'),
                  _InfoRow(label: '校准间隔', value: '${c.calIntervalRounds} 来回'),
                  _InfoRow(label: '默认速度', value: '${c.motorDefaultSpeed}%'),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),
          Text('已保存方案 (${model.presets.length})',
              style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          if (model.presets.isEmpty)
            Card(
              child: Container(
                width: double.infinity,
                padding: const EdgeInsets.symmetric(vertical: 32),
                child: const Column(
                  children: [
                    Icon(Icons.inbox_outlined, size: 48, color: Colors.grey),
                    SizedBox(height: 12),
                    Text('暂无预设方案', style: TextStyle(color: Colors.grey)),
                    SizedBox(height: 4),
                    Text('点击右下角按钮新建', style: TextStyle(fontSize: 12, color: Colors.grey)),
                  ],
                ),
              ),
            )
          else
            ...model.presets.map((name) => _PresetTile(
                  name: name,
                  onLoad: () {
                    if (model.previewMode) {
                      model.localLoadPreset(name);
                    } else {
                      model.sendLoadPreset(name);
                    }
                    ScaffoldMessenger.of(context).showSnackBar(
                      SnackBar(content: Text('已加载预设 $name')),
                    );
                  },
                  onDelete: () => _showDeleteDialog(context, model, name),
                )),
        ],
      ),
    );
  }

  void _showSaveDialog(BuildContext context, AppModel model) {
    final ctrl = TextEditingController();
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('新建预设'),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          decoration: const InputDecoration(
            labelText: '预设名称',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
          FilledButton(
            onPressed: () {
              if (ctrl.text.isNotEmpty) {
                model.sendSavePreset(ctrl.text);
                Navigator.pop(ctx);
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(content: Text('已保存预设 ${ctrl.text}')),
                );
              }
            },
            child: const Text('保存'),
          ),
        ],
      ),
    );
  }

  void _createPreset(BuildContext context, AppModel model) async {
    final ctrl = TextEditingController();
    final name = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('新建预设'),
        content: TextField(
          controller: ctrl,
          autofocus: true,
          decoration: const InputDecoration(
            labelText: '预设名称',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
          FilledButton(
            onPressed: () {
              if (ctrl.text.isNotEmpty) Navigator.pop(ctx, ctrl.text);
            },
            child: const Text('下一步'),
          ),
        ],
      ),
    );
    if (name == null || name.isEmpty) return;

    final result = await Navigator.push<DeviceConfig>(
      context,
      MaterialPageRoute(
        builder: (_) => PresetEditorScreen(
          title: '新建预设 $name',
          initialConfig: model.config.copy(),
        ),
      ),
    );
    if (result == null || !context.mounted) return;

    if (model.previewMode) {
      model.localSavePreset(name, result);
    } else {
      model.sendSetParams(result.toMap());
      Future.delayed(const Duration(milliseconds: 300), () {
        model.sendSavePreset(name);
      });
    }
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('已保存预设 $name')),
    );
  }

  void _showDeleteDialog(BuildContext context, AppModel model, String name) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text('删除预设 $name ?'),
        content: const Text('此操作不可撤销。'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('取消')),
          FilledButton(
            onPressed: () {
              if (model.previewMode) {
                model.localDeletePreset(name);
              } else {
                model.sendDeletePreset(name);
              }
              Navigator.pop(ctx);
            },
            style: FilledButton.styleFrom(backgroundColor: Colors.red),
            child: const Text('删除'),
          ),
        ],
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  final String label;
  final String value;
  const _InfoRow({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(color: Colors.grey, fontSize: 13)),
          Text(value, style: const TextStyle(fontWeight: FontWeight.w500)),
        ],
      ),
    );
  }
}

class _PresetTile extends StatelessWidget {
  final String name;
  final VoidCallback onLoad;
  final VoidCallback onDelete;
  const _PresetTile({required this.name, required this.onLoad, required this.onDelete});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: const Icon(Icons.bookmark_outline),
        title: Text(name),
        subtitle: const Text('点击加载到当前配置'),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            IconButton(
              icon: const Icon(Icons.download_for_offline_outlined),
              tooltip: '加载',
              onPressed: onLoad,
            ),
            IconButton(
              icon: const Icon(Icons.delete_outline, color: Colors.red),
              tooltip: '删除',
              onPressed: onDelete,
            ),
          ],
        ),
        onTap: onLoad,
      ),
    );
  }
}
