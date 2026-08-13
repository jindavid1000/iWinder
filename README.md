# 耗材绕线器v0.1(本项目目前还无法正常使用，但模型部分基本完成，后面主要是更新固件和 app)

3D 打印耗材自动绕线器 —— 把散装耗材整齐地绕到料盘上。

ESP32 主控 + 连续旋转舵机排线 + 直流电机收线，手机 APP 无线控制，支持蓝牙和 WiFi 双模连接、实时监控、参数预设。

根据废改实验室的散料绕线器+手摇+电动进行改造，此绕线器几乎可以绕全部尺寸的料盘，且大部分参数支持自定义

https://makerworld.com.cn/zh/models/2379707-san-liao-rao-xian-qi-shou-yao-dian-dong?from=search#profileId-2700945

https://creativecommons.org/licenses/by-nc/4.0/

---

## 项目结构

```
├── app/            # Flutter 手机 APP（Android）
├── esp32/          # ESP32 固件（PlatformIO + Arduino）
├── 模型/           # 3D 打印模型文件（.3mf）
└── 设定/           # 开发文档
    ├── 描述.md     # 完整开发规范（硬件/协议/APP/配置）
    └── 重构方案.md # WiFi/LEDC 问题修复记录
```

---

## 组装

电机和主体的安装可前往原作者的视频查看
https://www.bilibili.com/video/BV1EedYBeEyY/?spm_id_from=333.788.recommend_more_video.-1&trackid=web_related_0.router-related-2589621-4gb82.1786498199511.912&vd_source=ed1acd861692f1d7a0d3436715a9e9e4
https://www.bilibili.com/video/BV1qpoMBfEwc/?spm_id_from=888.80997.embed_other.whitelist&t=635&bvid=BV1qpoMBfEwc
，核心的往复机构稍微等一等，有时间的话应该会做一个视频（并不复杂，也没几个零件，自己稍微琢磨一下应该就行）

### 硬件清单

| 部件 | 规格 | 说明 |
|------|------|------|
| 主控板 | ESP32-DevKitC（普通版） | |
| 收线盘电机 | 直流减速电机 6V 40RPM | 单向收线 |
| 电机驱动 | IRF520 MOS 模块 | PWM 调速 |
| 排线舵机 | 鑫辉 18KG 连续旋转舵机 | 驱动丝杆排线 |
| 丝杆 | T8 四头丝杆 + 螺母（导程 22mm，螺距 5.5mm） | |
| 霍尔传感器 | KY-003（A3144）× 1 | 料盘计圈（小心别买到没有 led 和电阻的，不知道是不是我个人问题，我在测试的时候没有的无法正常使用） |
| 磁铁 | N35 Φ6×2mm × 8 | 贴在料盘上配合霍尔 |
| 限位开关 | 摆杆式 Endstop（无滚轮）× 2 | |
| 轴承 | 608ZZ | |
| 电源 | 鹿小班 DCP3512 LM2596 可调降压模块 | 四路输出（3.3V / 5V / 12V / 可调），每路最大 3.5A |

### 接线

**ESP32 信号线**

| ESP32 引脚 | 连接 |
|-----------|------|
| GPIO 4 | 电机 PWM（IRF520 信号脚） |
| GPIO 5 | 舵机 PWM 信号线 |
| GPIO 14 | Endstop 左（原点校准）信号 |
| GPIO 32 | Endstop 右（硬限位保护）信号 |
| GPIO 27 | 霍尔传感器信号 |
| GPIO 2 | 板载 LED（状态指示，无需外接） |

**供电**

| 电源模块输出 | 连接 |
|-------------|------|
| 3.3V | ESP32 3V3 脚、霍尔传感器 VCC、Endstop 信号公共端 |
| 5V | 舵机 VCC（如舵机额定 5V） |
| 12V | IRF520 驱动板电机供电端、电机正极 |
| GND | 所有模块共地 |

> ⚠️ ESP32 的 GND 必须与电机/舵机电源的 GND 共地，否则 PWM 信号不稳定。
> 引脚全部可在 `esp32/include/config.h` 中修改，也可通过 APP 运行时调整。

### 3D 打印

`模型/` 目录下提供所有打印件。推荐使用 Bambu Studio 切片，PLA 或 PETG 均可（推荐使用 petg，因为我所有模型都在 petg 下测试）。

---

## 使用指南

### 第一步：烧录固件

**安装 PlatformIO**（VS Code 插件或命令行均可）

```bash
# 命令行安装
pip install platformio
```

**烧录**

1. USB 连接 ESP32 到电脑
2. 确认串口设备（`pio device list` 查看）
3. 编译并烧录：

```bash
cd esp32
pio run -e esp32 -t upload
```

4. 烧录完成后打开串口监视器确认启动成功：

```bash
pio device monitor
```

> 如果只需验证单个硬件模块，可用测试环境：`pio run -e test_motor -t upload`（可选：`test_led`、`test_servo`、`test_hall`、`test_endstop`、`test_wifi`）

---

### 第二步：安装 APP

**方式一：直接安装 APK（推荐）**

从 [Releases](../../releases) 下载最新的 `app-release.apk`，传到手机安装。

**方式二：自行编译**

```bash
cd app
flutter pub get
flutter build apk --release
# 产物：build/app/outputs/flutter-apk/app-release.apk
```

---

### 第三步：连接设备

ESP32 上电后会同时启动 **WiFi 热点（ESP-Winder）** 和尝试连接上次配过的 WiFi。

#### 首次配网（两种方式）

**方式 A — WiFi 热点配网**

1. 手机 WiFi 设置里连接热点 **ESP-Winder**
2. 打开 APP，进入「设备连接」页面
3. IP 填 `192.168.4.1`，端口 `8080`，点「连接」
4. 在「WiFi 配网」区域输入家庭 WiFi 名称和密码，点「发送配网」
5. ESP32 连上家庭 WiFi 后会回报局域网 IP
6. 手机切回家庭 WiFi，以后就能用局域网 IP 连接了

**方式 B — 局域网自动发现**

> 前提：ESP32 已经配过网，且和手机在同一 WiFi 下

1. 打开 APP，进入「设备连接」页面
2. 点「搜索设备」
3. APP 会自动搜索局域网内的绕线器，找到后一键连接

---

### 第四步：开始绕线

1. **设置参数**：在「预设」页面选择或编辑料盘参数（默认拓竹 1kg，可改线径、料盘尺寸等）
2. **调速**：在主控制页面拖动速度滑块设定速度
3. **启动**：点「启动」，收线盘软启动旋转，排线机构自动左右移动
4. **运行中**：APP 实时显示转速、圈数、绕线长度、排线位置等
5. **停止**：点「停止」，排线自动回原点归位

排线机构会每绕几个来回自动回限位校准一次，保证排线精度。

---

## 参数调校

首次使用需要在 APP 设置页面标定以下参数：

| 参数 | 说明 |
|------|------|
| 排线左/右行速度 | 实测排线移动速度（mm/s），点「标定」自动测量 |
| 电机最低转速 | 电机不堵转的最低稳定速度百分比 |
| 料盘参数 | 外径、宽度、芯轴直径（有/无纸筒可切换） |
| 排线范围 | 左起始位置、右终止位置（默认 = 料盘宽度） |
| 校准间隔 | 每几个来回回 Endstop 校准一次（默认 3） |

所有参数都支持保存为预设方案（最多 10 组），一键切换。ESP32 断电后参数保存在 NVS 中不会丢失。

---
## 可选配件

靴不会

耗材旋转支架

https://makerworld.com.cn/zh/models/1705115-hao-cai-xuan-zhuan-zhi-jia?from=search#profileId-1876547

（我在站内找到的最省料的耗材转盘，可以用来当散料从转盘，上面放散料）




LaphaeL

MG996R双轴舵机适配器

https://makerworld.com.cn/zh/models/538013-mg996rshuang-zhou-duo-ji-gua-pei-qi?from=search#profileId-466131

（可以把 mg946、mg995、mg996 改成标准的双轴舵机，本项目中我最开始使用的是鑫辉的 18kg 舵机，因为我本来就有，后来整理 BOM 的时候发现好贵，就觉得可以换成便宜的舵机，然后就看到了这个已有的项目，用这个就可以直接把舵机改成适配的双轴舵机，如果你买的是 mg946，那么就需要打印一份这个，文件在 模型/配置/mg946 改双轴.3mf 也有）

鑫辉最高 7.4v，mg946 6v，不要更高

## License

CC BY-NC 4.0（署名-非商业性使用 4.0 国际）

本项目硬件设计（3D 模型）和软件代码均可自由使用、修改和分享，但**不得用于商业目的**，使用时需注明出处。详见 [LICENSE](LICENSE)。

制作不易，助力我买个 fusion 会员吧，🙏
![图片描述](7f68057528a3c05cb0a61e4aa974e441.jpg)