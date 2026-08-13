import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'services/app_model.dart';
import 'screens/connect_screen.dart';
import 'screens/control_screen.dart';
import 'screens/preset_screen.dart';
import 'screens/settings_screen.dart';

void main() {
  runApp(const WinderApp());
}

class WinderApp extends StatelessWidget {
  const WinderApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => AppModel(),
      child: MaterialApp(
        title: '绕线器',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          colorScheme: ColorScheme.fromSeed(
            seedColor: const Color(0xFF006970),
            brightness: Brightness.light,
          ),
          useMaterial3: true,
          appBarTheme: const AppBarTheme(centerTitle: false),
        ),
        home: const HomePage(),
      ),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  int _index = 0;

  @override
  Widget build(BuildContext context) {
    final model = context.watch<AppModel>();
    final connected = model.isConnected;

    final screens = [
      ConnectScreen(),
      if (connected) const ControlScreen() else const _NotConnected(),
      if (connected) const PresetScreen() else const _NotConnected(),
      if (connected) const SettingsScreen() else const _NotConnected(),
    ];

    return Scaffold(
      body: screens[_index],
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: (i) => setState(() => _index = i),
        destinations: [
          const NavigationDestination(
            icon: Icon(Icons.wifi),
            selectedIcon: Icon(Icons.wifi),
            label: '连接',
          ),
          NavigationDestination(
            icon: const Icon(Icons.play_circle_outline),
            selectedIcon: const Icon(Icons.play_circle),
            label: '控制',
            enabled: connected,
          ),
          NavigationDestination(
            icon: const Icon(Icons.bookmark_border),
            selectedIcon: const Icon(Icons.bookmark),
            label: '预设',
            enabled: connected,
          ),
          NavigationDestination(
            icon: const Icon(Icons.settings_outlined),
            selectedIcon: const Icon(Icons.settings),
            label: '设置',
            enabled: connected,
          ),
        ],
      ),
    );
  }
}

class _NotConnected extends StatelessWidget {
  const _NotConnected();

  @override
  Widget build(BuildContext context) {
    return const Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.wifi_off, size: 64, color: Colors.grey),
          SizedBox(height: 16),
          Text('请先连接设备', style: TextStyle(fontSize: 16, color: Colors.grey)),
        ],
      ),
    );
  }
}
