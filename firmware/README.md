# 固件下载与烧录

## 文件说明

| 文件 | 烧录地址 |
|------|---------|
| `iwinder-vX.Y.Z.bin` | **0x10000**（应用固件） |
| `iwinder-vX.Y.Z-full.bin` | 0x0（四合一合并镜像，仅命令行用） |

## 方式一：网页烧录（推荐，无需装任何软件）

1. 用 **Chrome 或 Edge** 打开 **[esptool.spacehuhn.com](https://esptool.spacehuhn.com/)**
2. USB 线连接 ESP32 到电脑
3. 点 **Connect** → 浏览器弹出串口选择框 → 选择你的 ESP32 串口（Windows 是 `COMx`，Mac 是 `/dev/cu.usbserial-xxxx`）
   - 如果列表为空：安装 CH340/CP2102 驱动后刷新页面重试
4. **只需第一个槽**：
   - 把地址从 `0x1000` **改成 `0x0`**（地址栏可以编辑）
   - 选择 `iwinder-v0.2.2-full.bin`（合并镜像，已包含全部内容）
   - 其余槽位留空
5. 波特率保持默认，点 **Flash** → 等进度条走完（约 30~60 秒）
6. 出现 "Done" 后重新上电，ESP32 会启动热点 **ESP-Winder**

> 💡 **已运行 iWinder 固件、只想升级**：第一个槽地址填 **0x10000**，
> 选 `iwinder-v0.2.2.bin`（仅应用固件），其余留空——设备参数不会丢。

<details>
<summary>四文件分开烧（当合并镜像有问题时的备选）</summary>

| 槽位 | 选择文件 |
|------|---------|
| 0x1000 | `iwinder-v0.2.2-bootloader.bin` |
| 0x8000 | `iwinder-v0.2.2-partitions.bin` |
| 0xE000 | `iwinder-v0.2.2-boot_app0.bin` |
| 0x10000 | `iwinder-v0.2.2.bin` |

</details>

> 💡 浏览器烧录使用 Web Serial API，目前 Chrome/Edge 支持；
> Safari 和 Firefox 不支持，请换 Chrome 或 Edge。

## 方式二：esptool 命令行

```bash
pip install esptool

# 方法 A: 合并镜像一步烧录（等效四文件，适合全新芯片）
esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX \
    write_flash 0x0 iwinder-v0.2.2-full.bin

# 方法 B: 四文件分开烧（等效网页方式）
esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX write_flash \
    0x1000 iwinder-v0.2.2-bootloader.bin \
    0x8000 iwinder-v0.2.2-partitions.bin \
    0xE000  iwinder-v0.2.2-boot_app0.bin \
    0x10000 iwinder-v0.2.2.bin
```

（Windows 把 port 换成 `COM3` 之类的串口号）

## 首次启动

烧录后 ESP32 会启动热点 **ESP-Winder**，手机连上后浏览器打开
`http://192.168.4.1` 即可使用 Web 界面（含 WiFi 配网）。

> ⚠️ 授权说明：启动绕线需要设备授权。在 Web 界面「授权」页查看设备 ID，
> 按项目 README 的流程申请/激活许可证。
