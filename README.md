# deepseek-balance-screen

A tiny always-on desk display for your DeepSeek API balance, built on an
**ESP32-S3 N16R8** dev board and a **0.96" 128×64 I²C OLED (SSD1306)**.
It costs nothing to run — the balance endpoint is free — and everything it
needs is one Wi-Fi name and one API key.

Repository name: `deepseek-balance-screen`

| | |
|---|---|
| **Board** | ESP32-S3 N16R8 (16 MB flash, 8 MB PSRAM, dual Type-C) |
| **Display** | 0.96" OLED, 128×64, SSD1306, I²C, address `0x3C` |
| **LED** | on-board WS2812 on GPIO 48 |
| **Framework** | Arduino IDE + ESP32 core, libraries: Adafruit SSD1306, Adafruit NeoPixel |

Screen preview:

```
15:13:50              WiFi
已花 0.8000 CNY
  16.86  CNY
[▓▓▓▓▓░░░░░] 22...
```

*(top row: clock + Wi-Fi state · middle: today's spend · big number: balance ·
bottom: one-minute progress bar with the seconds and an animated dot count)*

---

## What it does

| Feature | Notes |
|---|---|
| Balance | `GET /user/balance`, refreshed every 60 s (free, no quota) |
| Today's spend | first balance of the day minus the current one; survives power loss |
| Top-up aware | if the balance jumps up, the baseline rises too — spent money is not erased |
| Clock | NTP synced, redrawn only when the second changes (no flicker) |
| Progress bar | 60 s ring with the seconds and three dots, all in step |
| On-board RGB LED | red during peak pricing hours, green off-peak, off before the clock syncs |
| Night dimming | panel dims itself between 22:00 and 07:00 |
| Wi-Fi reconnect | retries at 30 s → 60 s → 120 s → 300 s, with an animated "connecting" screen |

---

## Bill of materials

| Part | Exact spec | Qty | Notes |
|---|---|---|---|
| Dev board | **ESP32-S3 N16R8** — 16 MB flash, 8 MB octal PSRAM, CH340 + native USB | 1 | the "N16R8" marking matters: 16 MB flash / 8 MB PSRAM |
| OLED | **0.96" 128×64 SSD1306**, I²C, 4 pins (VCC GND SCL SDA) | 1 | address `0x3C`; the 7-pin SPI version will not fit this sketch |
| Jumper wires | female-to-female | 4 | SDA / SCL / 3V3 / GND |
| USB cable | Type-C, **data** cable | 1 | plug it into the **left** (CH340) port for upload + serial |

No resistors, no breadboard needed — the OLED runs straight off 3V3.

> Already have a **0.96" SSD1315** panel? The Adafruit SSD1306 library
> drives it fine in practice. If the screen stays blank, run an I²C scanner
> first — a `NO DEVICE FOUND` result means wiring or address, not the driver.

---

## Wiring

| Signal | ESP32-S3 pin | OLED pin |
|---|---|---|
| SDA | **GPIO 8** | SDA |
| SCL | **GPIO 9** | SCL |
| 3V3 | 3V3 | VCC |
| GND | GND | GND |

The on-board WS2812 is on **GPIO 48** — nothing to wire.
If your OLED is wired to other pins, change `SDA_PIN` / `SCL_PIN` in block ③.

---

## Setup

1. Install the Arduino IDE and the ESP32 board support package
   (Boards Manager → `esp32` by Espressif).
2. Library Manager → install **Adafruit SSD1306** (pulls in Adafruit GFX and
   BusIO) and **Adafruit NeoPixel**.
3. Open `BalanceScreen.ino`. The first ~100 lines are the only part you need
   to touch — three blocks, each with a Chinese placeholder:

```cpp
#define WIFI_SSID "＜这里填你的WiFi名称＞"
#define WIFI_PASS "＜这里填你的WiFi密码＞"
#define DEEPSEEK_KEY "＜这里填你自己的DeepSeek密钥＞"
```

   Everything inside `＜＞` is a placeholder — **replace the whole thing,
   brackets included**. Leaving the Chinese text in builds but never connects.
4. Board settings:

   | Setting | Value |
   |---|---|
   | Board | **ESP32S3 Dev Module** |
   | Flash Size | **16MB** |
   | PSRAM | **OPI PSRAM** |
   | Upload Speed | 921600 (drop to 115200 if it fails) |
   | Port | the **CH340** one (`COM6` on Windows, `/dev/ttyUSB0` on Linux/macOS) |

   Close the serial monitor before uploading, otherwise you get
   `No serial data received`.
5. Upload, open the serial monitor at 115200 and you should see
   `WiFi..` → `IP: …` → `time sync: OK` → `balance code=200 val=…`.

Nothing else to configure. Pin changes and peak-hour windows are optional
defines further down the file.

Useful links:

- DeepSeek API keys: <https://platform.deepseek.com/api_keys>
- Adafruit SSD1306 library: <https://github.com/adafruit/Adafruit_SSD1306>
- Adafruit NeoPixel library: <https://github.com/adafruit/Adafruit_NeoPixel>
- ESP32 Arduino core: <https://github.com/espressif/arduino-esp32>

---

## Notes

- **Keep your key out of the repo.** Type your own key into the sketch locally
  and never commit a file containing a real `sk-…`.
- The screen is 128 px wide and everything is positioned by hand — if you
  change the layout, check the right column (seconds + dots) still fits.
- The two Chinese glyphs (已花, "spent") are 16×16 bitmaps inside the sketch,
  so no font library or CJK font is needed. Swap them for any two characters
  by regenerating the bitmap arrays.
- HTTPS uses `setInsecure()`, which skips certificate validation. Fine for a
  balance read on a home network; add a root CA if you care.

---

## Licence

MIT.

---
---

# 中文说明 · deepseek-balance-screen

一块常亮的桌面小屏，用来显示你的 DeepSeek 账户余额。基于
**ESP32-S3 N16R8** 开发板和 **0.96 寸 128×64 I2C OLED（SSD1306）**。
运行不花钱——余额接口是免费的——需要的只有 Wi-Fi 和一把密钥。

仓库名：`deepseek-balance-screen`

| | |
|---|---|
| **开发板** | ESP32-S3 N16R8（16MB Flash、8MB PSRAM、双 Type-C 口） |
| **屏幕** | 0.96 寸 OLED，128×64，SSD1306，I2C，地址 `0x3C` |
| **彩灯** | 板载 WS2812，GPIO 48 |
| **开发环境** | Arduino IDE + ESP32 核心，库：Adafruit SSD1306、Adafruit NeoPixel |

屏幕效果：

```
15:13:50              WiFi
已花 0.8000 CNY
  16.86  CNY
[▓▓▓▓▓░░░░░] 22...
```

*（顶行：时间 + Wi-Fi 状态 · 中间：今日已花 · 大字：余额 ·
底部：一分钟进度条，带秒数和跳动的三个点）*

---

## 功能

| 功能 | 说明 |
|---|---|
| 余额 | `GET /user/balance`，每 60 秒刷新一次（免费，不占额度） |
| 今日已花 | 当天第一次查到的余额减去当前余额；断电不丢 |
| 充值识别 | 余额突然变大时同步抬高基准，已花的钱不会被冲掉 |
| 时间 | NTP 同步，只在秒数变化时重画（不闪屏） |
| 进度条 | 一分钟环形进度，秒数和三个点严格同步 |
| 板载彩灯 | 高峰时段亮红，低谷亮绿，时间没同步时灭 |
| 夜间调光 | 22:00 到 07:00 自动变暗 |
| Wi-Fi 重连 | 间隔 30 秒 → 60 秒 → 120 秒 → 300 秒递增，带连接动画 |

---

## 物料清单

| 部件 | 确切规格 | 数量 | 备注 |
|---|---|---|---|
| 开发板 | **ESP32-S3 N16R8**——16MB Flash、8MB 八线 PSRAM、CH340 + 原生 USB | 1 | 认准「N16R8」这个型号：16MB Flash / 8MB PSRAM |
| 屏幕 | **0.96 寸 128×64 SSD1306**，I2C，四针（VCC GND SCL SDA） | 1 | 地址 `0x3C`；七针 SPI 版本插不上这个程序 |
| 杜邦线 | 母对母 | 4 根 | SDA / SCL / 3V3 / GND |
| USB 线 | Type-C，**数据**线 | 1 根 | 插**左边**（CH340）那个口，上传和串口都用它 |

不需要电阻，也不需要面包板——屏幕直接吃 3.3V。

> 手里是 **0.96 寸 SSD1315** 屏？Adafruit 的 SSD1306 库实际能驱动。
> 如果屏幕一直黑，先跑一次 I2C 扫描——扫描结果是 `NO DEVICE FOUND`
> 说明是接线或地址问题，不是驱动问题。

---

## 接线

| 信号 | ESP32-S3 引脚 | 屏幕引脚 |
|---|---|---|
| SDA | **GPIO 8** | SDA |
| SCL | **GPIO 9** | SCL |
| 3V3 | 3V3 | VCC |
| GND | GND | GND |

板载彩灯是 **GPIO 48**，不用接线。
如果你的屏幕接在别的引脚，改程序里第 ③ 块中的 `SDA_PIN` / `SCL_PIN`。

---

## 使用步骤

1. 装好 Arduino IDE 和 ESP32 开发板支持包
   （开发板管理器里搜 `esp32`，选 Espressif 那个）。
2. 库管理器里装 **Adafruit SSD1306**（会连带装 Adafruit GFX 和 BusIO）
   和 **Adafruit NeoPixel**。
3. 打开 `BalanceScreen.ino`。最前面约一百行是唯一需要动的地方，
   一共三个方框，里面都是中文占位符：

```cpp
#define WIFI_SSID "＜这里填你的WiFi名称＞"
#define WIFI_PASS "＜这里填你的WiFi密码＞"
#define DEEPSEEK_KEY "＜这里填你自己的DeepSeek密钥＞"
```

   尖括号里的内容都是占位符——**整个换掉，连着尖括号一起**。
   中文留在里面能编译过，但永远连不上。
4. 开发板设置：

   | 设置项 | 值 |
   |---|---|
   | 开发板 | **ESP32S3 Dev Module** |
   | Flash Size | **16MB** |
   | PSRAM | **OPI PSRAM** |
   | 上传速度 | 921600（失败就降到 115200） |
   | 端口 | **CH340** 那个（Windows 一般是 `COM6`，Linux/macOS 是 `/dev/ttyUSB0`） |

   上传前先关掉串口监视器，否则会报 `No serial data received`。
5. 上传，打开 115200 的串口监视器，应该依次看到
   `WiFi..` → `IP: …` → `time sync: OK` → `balance code=200 val=…`。

其他什么都不用配。改引脚和改高峰时段是文件后半段的可选项。

常用链接：

- DeepSeek 密钥申请：<https://platform.deepseek.com/api_keys>
- Adafruit SSD1306 库：<https://github.com/adafruit/Adafruit_SSD1306>
- Adafruit NeoPixel 库：<https://github.com/adafruit/Adafruit_NeoPixel>
- ESP32 Arduino 核心：<https://github.com/espressif/arduino-esp32>

---

## 注意事项

- **别把密钥提交上去。** 把自己的密钥填在本地程序里，别把带真密钥的文件提交。
- 屏幕只有 128 像素宽，每个元素都是手动摆的位置——改布局时记得检查
  右边那列（秒数和三个点）还放不放得下。
- 屏幕上那两个字（已花）是 16×16 点阵，直接写在程序里，
  不需要任何字库或中文字体。想换成别的字，按同样流程重新生成点阵数组。
- HTTPS 用的是 `setInsecure()`，跳过证书校验。在家用网络读个余额够用；
  在意的话可以换成根证书。

---

## 许可证

MIT。
