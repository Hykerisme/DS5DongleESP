以下是可直接用于仓库 README 或社区介绍的项目说明文档草稿，目标读者同时覆盖**人类开发者**和**LLM 代码助手**。

---

# DS5Dongle-ESP：ESP32-S3 + ESP32 双芯片 DualSense 无线适配器

## 一句话定义

将 Pico 2W 上的 DS5Dongle 移植到 **ESP32-S3（USB OTG）+ 原版 ESP32（Classic BT）双芯片架构**，并增加一个仅调试模式激活的 **USB 上位机**，用于以波形/状态机方式深度调试 DualSense 的自适应扳机和 HD 震动。

**注意，考虑到移植工作的复杂程度，上位机暂时不做实现。**

---

## 0. 为什么是双芯片，而不是单芯片

这是整个项目最容易被误解的设计决策，必须最先说明。

原版 DS5Dongle 跑在 Pico 2W（RP2350 + CYW43439）上，依赖的是 CYW43439 的 **Bluetooth Classic（BR/EDR）** 能力来连接 DualSense。DualSense、Switch Pro 等手柄使用的是 **BR/EDR**，而不是 BLE。

**ESP32-S3 不支持 Bluetooth Classic**。Bluepad32 的官方 FAQ 明确指出：只有**原版 ESP32** 支持 BR/EDR；ESP32-S3、C3、C5、C6、H2 均不支持。

因此本项目采用：

| 芯片                 | 角色                                                                 | 关键依据                                                                   |
| -------------------- | -------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| **ESP32-S3**   | USB Device（TinyUSB），对 PC 枚举为**HID + UAC1 音频复合设备** | S3 有 USB OTG FS，TinyUSB 是 DS5Dongle 原生 USB 栈，描述符和回调可直接复用 |
| **原版 ESP32** | **Classic BT HID Host**，连接 DualSense/DualSense Edge         | 唯一支持 BR/EDR 的 ESP32 系列芯片                                          |
| **芯片间**     | UART（起步，当前按 TX/RX + GND 设计）或 SPI slave + GPIO IRQ            | 报告频率远低于链路带宽，UART 足够；当前两块板的引脚图未标出可直接使用的 RTS/CTS，硬件流控暂不作为前提 |

这一分工把**最难搬的 Classic BT 栈**留在原生支持的芯片上，把**最容易复用的 USB 描述符和报告翻译层**放在 S3 上，是当前约束下的最合理路径。

## 0.1 当前硬件与引脚图记录

以下内容来自项目目录中的引脚图图片识读，图片本身是当前硬件连接判断的依据；如果文字识读与实物丝印、板卡原理图或后续测量不一致，应以原图和实物为准。

图片文件：

- `ESP32.png`：原版 ESP32 开发板引脚图。
- `esp32s3.png`：ESP32-S3 开发板引脚图。

### 原版 ESP32 图片中明确标注的内容

根据 `ESP32.png`：

- `GPIO16` 标注为 `UART2_RXD`。
- `GPIO17` 标注为 `UART2_TXD`。
- 图片没有看到 `RTS`、`CTS` 或其他 UART 硬件流控专用标注。
- 因此当前不能把 ESP32 侧的 RTS/CTS 当作已经确认存在的板级引脚；即使 ESP32 UART 外设/矩阵可能支持相关功能，也需要进一步查芯片手册、板卡原理图或实测确认。

### ESP32-S3 图片中明确标注的内容

根据 `esp32s3.png`：

- `GPIO13` 和 `GPIO14` 均作为普通 `GP13`、`GP14` 引出。
- 图片没有把 GPIO13/14 标为固定 UART TX/RX，但 ESP32-S3 的 GPIO Matrix 可用于将 UART 信号映射到这些 GPIO；具体 UART 控制器编号和方向由固件配置决定。
- 图片顶部另行标出了 `D_P`、`D_N`，因此 USB D+/D− 与当前 GPIO13/14 UART 连接应视为不同信号。

### 当前已接线关系

两块板分别通过 USB 供电，并通过 GND 共地；不连接两块板的 3V3 输出。当前已接出的信号线为：

```text
ESP32-S3 GPIO13 (TX)  ->  ESP32 GPIO16 (UART2_RXD)
ESP32-S3 GPIO14 (RX)  <-  ESP32 GPIO17 (UART2_TXD)
ESP32-S3 GPIO15       ->  ESP32 GPIO4
ESP32-S3 GPIO16       ->  ESP32 GPIO15
ESP32-S3 GPIO17       ->  ESP32 GPIO2
GND                   ---  GND
```

本项目暂定后三对线的逻辑定义为：

```text
ESP32-S3 GPIO15 (RTS)    ->  ESP32 GPIO4  (CTS)
ESP32-S3 GPIO16 (CTS)    <-  ESP32 GPIO15 (RTS)
ESP32-S3 GPIO17 (READY)  ->  ESP32 GPIO2  (LINK_READY)
```

这里的 TX/RX 与 RTS/CTS 方向均以信号名称为准：发送端连接对端接收端。两颗芯片都支持 GPIO Matrix，因此 UART2 和硬件流控信号可以映射到上述 GPIO；具体使用哪个 UART 控制器由各自固件配置决定。

注意：原版 ESP32 的 GPIO15 和 GPIO2 都属于启动绑定位相关 GPIO。GPIO15 当前作为 ESP32 RTS 输入，GPIO2 当前作为 ESP32 LINK_READY 输入，不能在对端复位期间被 S3 主动驱动到不安全电平。初始固件必须先将 S3 GPIO15/16/17 配置为高阻/安全状态，再逐步启用 UART 和握手；若实测影响 ESP32 下载模式或正常启动，应停用 READY/硬件流控并回到三线 TX/RX/GND 方案，不能用软件强行覆盖启动绑定位约束。

当前 POC-3 可按六线链路实现：TX、RX、RTS、CTS、READY、GND。协议层仍需保留帧同步、长度、序号和 CRC，即使启用 RTS/CTS 也不能把硬件流控当作数据完整性校验。

---


### 目标 1：将 DS5Dongle 移植到 ESP32-S3 + ESP32，稳定作为 USB Dongle 工作

对 PC 侧的行为与原版一致：**PC 认为 DualSense 是以有线方式插入的**，VID/PID 为 `054C:0CE6`（标准版）或 `054C:0DF2`（Edge）。蓝牙链路对 PC 完全透明。

移植的重点不是“重写一切”，而是**分层迁移**：

| 层级                                                         | 迁移策略                                                                      |
| ------------------------------------------------------------ | ----------------------------------------------------------------------------- |
| USB 描述符 + TinyUSB 回调                                    | **直接复用**上游 Pico 代码（ESP-IDF 的 `esp_tinyusb` 底层即 TinyUSB） |
| DualSense 报告翻译层（BT `0x31` ↔ USB `0x01`/`0x02`） | **直接复用**，纯数据重组，与平台无关                                    |
| Classic BT HID Host                                          | **重写**：Pico 用 BTstack，ESP32 用 Bluedroid `esp_hidh` 或 Bluepad32 |
| 音频 UAC1 透传                                               | 视上游实现而定，S3 的 TinyUSB 支持 UAC1                                       |

上游代码的参考价值集中在**报告翻译层**和**USB 描述符**。`sqlCRT/ds5dongle-bl618-opensource` 是一个已经把 BT 栈从 BTstack 迁移到另一套 Zephyr-based 栈、USB 栈从 TinyUSB 迁移到 CherryUSB 的移植案例，可以作为“哪些代码必须重写、哪些可以保留”的参照。

### 目标 2：USB 调试上位机（仅调试模式激活）

这是本项目相对上游的**核心增量**。上位机不参与正常游戏流程，仅在 dongle 进入**调试模式**时枚举为一个**独立的 USB 接口**（vendor-specific HID 或 CDC），供开发者以波形和状态机的视角操控 DualSense 的高级功能。

#### 2.1 基础功能覆盖（对齐 daidr/dualsense-tester）

以下功能作为调试上位机的基础能力，行为对齐 `ds.daidr.me`：

- 读取手柄出厂信息（序列号、固件版本、主板型号等）
- 测试所有按键和轴
- 触摸板测试
- 陀螺仪 / 加速度计可视化
- 振动马达测试
- 灯条 / 玩家 LED 测试
- 自适应扳机测试
- 多手柄支持

daidr/dualsense-tester 基于 **WebHID**，这意味着它的协议层逻辑（报告格式、Feature Report 读写、输出报告构造）可以作为上位机**协议参考实现**直接借鉴。

#### 2.2 扩展：自适应扳机波形状态机

这是调试上位机区别于普通 tester 的核心功能。

**问题**：DualSense 的自适应扳机不是简单的“开/关”或“力度等级”，而是一个**力-位置关系**。上游社区已经解析出多种触发效果（feedback、weapon、vibration、slope 等），每种效果由一组参数描述“扳机在位置 x 处应提供多大的阻力”。

**本项目提供的调试模型**：

开发者不直接填写原始参数，而是通过**波形编辑器**定义扳机行程上的力反馈曲线：

- **横轴**：扳机按下深度（0–100%）
- **纵轴**：反馈力度 / 阻力类型
- **控制点**：可拖拽的曲线编辑，实时下发到手柄

在此基础上叠加一个**简单状态机**，支持至少两种到达状态：

| 状态                                   | 语义                                                           | 示例                                               |
| -------------------------------------- | -------------------------------------------------------------- | -------------------------------------------------- |
| **首次到达（First Reach）**      | 扳机第一次到达某个深度区间时激活的波形                         | 扣动扳机到 70% 时出现“段落感”                    |
| **后续到达（Subsequent Reach）** | 扳机已经过一次触发、再次到达同一区间时激活的**不同**波形 | 同一位置第二次扣动时变为“持续阻力”或“振动提示” |

状态机的输入是**扳机深度的时间序列**（来自 DualSense 输入报告中的扳机位置字段），输出是对应的扳机效果配置。这使开发者可以在浏览器里定义“第一次扣动和第二次扣动手感不同”的交互逻辑，而不需要在固件里硬编码。

#### 2.3 扩展：HD 震动波形生成器与相位调试

**问题**：DualSense 的 HD 震动由音圈电机（voice coil motor）驱动，接收的是 **PCM 数据**或通过蓝牙的特殊 HID 包。上游已知 DualSense 的触觉输出涉及 `0x36` 等报告格式，携带触觉样本和音频帧。

**本项目提供的调试模型**：

- **波形生成器**：在浏览器中生成任意波形（正弦、方波、噪声、包络调制、导入短音频片段），实时下发为触觉流。
- **双通道相位调试**：DualSense 的左右触觉执行器可以接收**独立通道**。调试上位机暴露**相位差**控制（0–360°），让开发者直观感受两个执行器在“同相叠加”和“反相/相位偏移”下的空间感差异——类似于空间音频中通过相位差营造方向感，但作用对象是**触觉**而非听觉。
- 这对后续“空间触觉”或“方向性反馈”的实验是基础工具。

#### 2.4 调试模式的 USB 拓扑

调试模式激活时，dongle 对 PC 的枚举从“纯 DualSense 模拟”切换为：

```
Interface 0: HID (DualSense emulation，保持正常功能)
Interface 1: Vendor-specific / CDC (调试通道)
```

调试通道承载：

- 上位机 → dongle：波形定义、状态机规则、实时触觉 PCM
- dongle → 上位机：手柄原始输入报告、扳机位置时间序列、触觉执行状态

调试模式通过**固件中的编译开关或启动时的按键组合**激活，不影响正常游戏使用。

---

## 2. 非目标（当前阶段明确不做）

为了避免范围蔓延，以下内容在当前阶段**不作为交付目标**：

- **音频→振动叠加**：上游 `loteran/DS5Dongle` 已经实现了 Audio Auto Haptics（将游戏音频实时映射为 rumble），本项目当前不重复造轮子，但保留后续接入的接口。
- **PC → NSPro / Switch → DualSense 模式**：这些是后续拓展目标，依赖当前 USB Dongle 基座跑通后再推进。
- **SPI/UART 裸机桥接**：与 USB 调试平台并行但独立的输出后端，当前不实现。
- **NSPro2 音频协议**：协议尚未公开，不做逆向承诺。

---

## 3. 参考资源（给开发者与 LLM 的导航）

| 资源                                                     | 用途                                                                                                    |
| -------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| `awalol/DS5Dongle`                                     | **上游原项目**。协议翻译层和 USB 描述符的主要参考                                                 |
| `sqlCRT/ds5dongle-bl618-opensource`                    | **跨平台移植参考**。展示 BT 栈和 USB 栈替换的边界，确认哪些代码与平台无关                         |
| `MarcelineVPQ/DS5Dongle-OLED-Edition` 的 `CLAUDE.md` | **LLM 友好的架构说明**。明确列出 USB HID + UAC1 复合设备、报告路径、haptics decimation 等关键设计 |
| `loteran/DS5Dongle`                                    | Audio Auto Haptics 参考实现，涉及 48kHz→3kHz 触觉重采样和 BT poll loop 的实时性权衡                    |
| `daidr/dualsense-tester`                               | **调试上位机的协议参考**。WebHID 实现，覆盖 Feature Report、输出报告构造、触觉/扳机测试逻辑       |
| Bluepad32 FAQ                                            | 确认**ESP32-S3 无 Classic BT**，双芯片架构的硬件依据                                              |
| `mdrv-ds-core` (crates.io)                             | DualSense 输出报告 builder 的 Rust 实现，`0x31`/`0x35`/`0x36`/`0x39`/`0x02` 的字节级参考      |

---

## 4. 当前状态与下一步

- [ ] **POC-1**：ESP32-S3 单独跑 TinyUSB DualSense HID 描述符，PC 识别为有线 DualSense
- [ ] **POC-2**：原版 ESP32 跑 Classic BT HID Host，稳定连接 DualSense，串口输出原始报告
- [ ] **POC-3**：两芯片 UART 链路打通，端到端按键/摇杆/扳机数据流可用
- [ ] （暂时不实现）**POC-4**：调试模式 USB 接口 + 基础 tester 功能（按键、轴、陀螺仪、灯条）
- [ ] （暂时不实现）**POC-5**：自适应扳机波形状态机调试
- [ ] （暂时不实现）**POC-6**：HD 震动波形生成器 + 双通道相位调试

每一步独立可验证，避免一次性全量移植后难以定位问题。

---

**关键词**：DualSense、Dongle、ESP32-S3、TinyUSB、Classic Bluetooth、BR/EDR、Adaptive Trigger、HD Haptics、DS5Dongle、WebHID、波形调试、状态机。
