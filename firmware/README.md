# 固件下载

| 文件 | 用途 |
|------|------|
| `iwinder-vX.Y.Z-full.bin` | **完整镜像**（含 bootloader + 分区表 + 固件），从 0x0 烧写，普通用户用这个 |
| `iwinder-vX.Y.Z.bin` | 仅应用程序，已跑着固件的设备 OTA/地址 0x10000 更新用 |

## 烧录方法

**方式一：Web 一键烧录（推荐）**

Chrome/Edge 浏览器打开 [web.esptool.fun](https://web.esptool.fun)，选 `*-full.bin`，地址填 `0x0`，连上 ESP32 点安装。

**方式二：esptool 命令行**

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX \
    write_flash 0x0 iwinder-v0.2.0-full.bin
```

（Windows 把 port 换成 `COM3` 之类的串口号）

**方式三：PlatformIO（开发者）**

本仓库 `esp32/` 目录只含外围源码，完整源码不公开；
用 PlatformIO 烧录完整固件请直接使用上面的 bin 文件。

## 首次启动

烧录后 ESP32 会启动热点 **ESP-Winder**，手机连上后浏览器打开
`http://192.168.4.1` 即可使用 Web 界面（含 WiFi 配网）。

> ⚠️ 授权说明：启动绕线需要设备授权。在 Web 界面「授权」页查看设备 ID，
> 按项目 README 的流程申请/激活许可证。
