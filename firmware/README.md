# 最新固件与 APP（v0.2.10）

| 文件 | 用途 |
|------|------|
| `iWinder_full.bin` | **推荐**。完整镜像（bootloader+分区表+固件），一条命令烧录 |
| `firmware.bin` | 仅应用固件，刷过旧版的设备更新用（`write_flash 0x10000`） |
| `app-release.apk` | 安卓 APP |

完整镜像烧录：

```bash
pip install esptool
esptool.py --chip esp32 --port 串口号 write_flash 0x0 iWinder_full.bin
```

> 这里只保留最新版本；历史版本见 [Releases](../releases)。
> 已联网设备可直接在 Web 界面「授权」页或 APP 在线升级（OTA），无需连电脑。
