# OV2640 on WS63 (GPIO-Emulated DVP)

[中文](#中文) | [English](#english)

## 中文

这个库把 `OV2640` 从常见 `STM32F103` Demo 迁移到了 `WS63` 标准库接口：

- SCCB(I2C): `uapi_i2c_master_*`
- GPIO/PIN: `uapi_gpio_*` + `uapi_pin_*`
- TCXO 微秒延时: `uapi_tcxo_init()` + `uapi_tcxo_delay_us()`
- 串口输出: `uapi_uart_init()` + `uapi_uart_write()`
- 采样方式: 用 GPIO 轮询模拟 DVP 并行口 (`PCLK + VSYNC + HREF + D0..D7`)
- 输出格式: `RGB565` 或 `JPEG`

### 目录

- `include/ov2640_ws63.h`: 对外 API
- `src/ov2640_ws63.c`: 驱动实现
- `demo/ov2640_ws63_demo.c`: JPEG 抓帧并通过 UART 连续输出给 XCAM 的示例任务
- `Kconfig` / `CMakeLists.txt`: 组件接入

### 快速集成

1. 将 `ov2640_ws63` 目录拷贝到你的 WS63 工程，常见放到 `application/samples/custom/`。
2. 在父级 `CMakeLists.txt` 中加入 `add_subdirectory_if_exist(ov2640_ws63)`。
3. 在父级 `Kconfig` 中加入 `source "ov2640_ws63/Kconfig"`。
4. 打开 `CONFIG_OV2640_WS63_DEMO`，并按你的板子修改 `demo/ov2640_ws63_demo.c` 里的引脚映射。

### API

- `ov2640_ws63_init()`: 初始化 I2C、DVP GPIO、控制脚
- `ov2640_ws63_probe()`: 读取 `PID/VER`
- `ov2640_ws63_set_rgb565_cif()`: 配置到 `RGB565 + CIF`
- `ov2640_ws63_configure()`: 配置格式、分辨率、JPEG 质量、PCLK 分频
- `ov2640_ws63_set_jpeg_quality()`: 运行时调整 JPEG 质量
- `ov2640_ws63_capture_frame()`: 采一帧原始字节流
- `ov2640_ws63_find_jpeg()`: 从 DVP 字节流中裁出 `0xffd8 ... 0xffd9` JPEG 数据

### JPEG + XCAM 串口输出

Demo 默认流程：

1. 初始化 OV2640。
2. 配置为 `JPEG + QVGA(320x240)`。
3. 采集一帧 DVP 字节流到 `g_frame_buf`。
4. 查找 JPEG 起止标记 `FFD8/FFD9`。
5. 通过 `XCAM_UART_BUS` 二进制发送 JPEG 原始数据。
6. 循环执行，电脑端 XCAM 按 JPEG 流实时显示。

默认参数在 `Kconfig` 中：

```text
CONFIG_OV2640_WS63_FRAMESIZE=1
CONFIG_OV2640_WS63_JPEG_QUALITY=12
CONFIG_OV2640_WS63_FRAME_BUF_SIZE=65536
CONFIG_OV2640_WS63_UART_BAUDRATE=921600
CONFIG_OV2640_WS63_PCLK_DIV=8
CONFIG_OV2640_WS63_TIMEOUT_MS=500
```

`CONFIG_OV2640_WS63_FRAMESIZE` 取值：

```text
0 = QQVGA 160x120
1 = QVGA  320x240
2 = CIF   352x288
3 = VGA   640x480
4 = SVGA  800x600
```

`CONFIG_OV2640_WS63_JPEG_QUALITY` 范围是 `0..63`，数值越小画质越好、JPEG 越大；数值越大画质越低、JPEG 越小。建议先用：

```text
QVGA: quality 10~20, buffer 65536
VGA:  quality 16~30, buffer 131072
SVGA: quality 20~35, buffer 196608
```

`CONFIG_OV2640_WS63_UART_BAUDRATE` 需要和 XCAM 电脑端串口设置一致。常用值：

```text
115200
460800
921600
1500000
2000000
```

高分辨率 JPEG 需要更高波特率，否则帧率会明显下降。

### SCCB 微秒延时

部分 OV2640 模组对 SCCB 时序比较挑剔。库在每次 SCCB 读写后默认调用：

```c
#include "tcxo.h"

uapi_tcxo_init();
uapi_tcxo_delay_us(50);
```

可通过 `ov2640_ws63_config_t.sccb_delay_us` 调整。建议范围：

```text
20us: 线路短、SCCB 稳定
50us: 默认值
100us: 探测失败或寄存器配置偶发失败时尝试
```

### 关键说明

1. `OV2640` 需要外部 `XCLK`，典型 6~24MHz。本库未在软件里生成高速 XCLK，需由外设时钟、PWM 或外部晶振提供。
2. GPIO 轮询采样属于“软 DVP”，速率上限受 CPU 和 GPIO 读取开销影响。建议先从低帧率、低分辨率验证。
3. XCAM 串口最好使用独立 UART。如果和系统日志 UART 共用，`osal_printk` 日志会混入 JPEG 字节流，导致电脑端显示异常。
4. Demo 发送的是 JPEG 原始二进制数据，不是十六进制文本；电脑端按 JPEG 流接收即可。

## English

This library ports an `OV2640` camera module from common `STM32F103` demos to the `WS63` standard SDK APIs:

- SCCB(I2C): `uapi_i2c_master_*`
- GPIO/PIN: `uapi_gpio_*` + `uapi_pin_*`
- TCXO microsecond delay: `uapi_tcxo_init()` + `uapi_tcxo_delay_us()`
- UART output: `uapi_uart_init()` + `uapi_uart_write()`
- Capture method: GPIO polling that emulates the DVP parallel bus (`PCLK + VSYNC + HREF + D0..D7`)
- Output formats: `RGB565` or `JPEG`

### Layout

- `include/ov2640_ws63.h`: Public API
- `src/ov2640_ws63.c`: Driver implementation
- `demo/ov2640_ws63_demo.c`: Demo task that captures JPEG frames and streams them to XCAM over UART
- `Kconfig` / `CMakeLists.txt`: Integration files

### Quick Integration

1. Copy the `ov2640_ws63` directory into your WS63 project, commonly under `application/samples/custom/`.
2. Add `add_subdirectory_if_exist(ov2640_ws63)` to the parent `CMakeLists.txt`.
3. Add `source "ov2640_ws63/Kconfig"` to the parent `Kconfig`.
4. Enable `CONFIG_OV2640_WS63_DEMO`, then update the pin map in `demo/ov2640_ws63_demo.c` for your board.

### API

- `ov2640_ws63_init()`: Initializes I2C, DVP GPIO pins, and control pins
- `ov2640_ws63_probe()`: Reads `PID/VER`
- `ov2640_ws63_set_rgb565_cif()`: Configures `RGB565 + CIF`
- `ov2640_ws63_configure()`: Configures format, framesize, JPEG quality, and PCLK divider
- `ov2640_ws63_set_jpeg_quality()`: Changes JPEG quality at runtime
- `ov2640_ws63_capture_frame()`: Captures one raw DVP byte stream frame
- `ov2640_ws63_find_jpeg()`: Extracts the `0xffd8 ... 0xffd9` JPEG payload from a DVP byte stream

### JPEG + XCAM UART Output

Default demo flow:

1. Initialize OV2640.
2. Configure `JPEG + QVGA(320x240)`.
3. Capture one DVP byte stream into `g_frame_buf`.
4. Locate the JPEG start/end markers `FFD8/FFD9`.
5. Send raw JPEG bytes through `XCAM_UART_BUS`.
6. Repeat continuously so the PC-side XCAM viewer can display the JPEG stream in real time.

Default `Kconfig` values:

```text
CONFIG_OV2640_WS63_FRAMESIZE=1
CONFIG_OV2640_WS63_JPEG_QUALITY=12
CONFIG_OV2640_WS63_FRAME_BUF_SIZE=65536
CONFIG_OV2640_WS63_UART_BAUDRATE=921600
CONFIG_OV2640_WS63_PCLK_DIV=8
CONFIG_OV2640_WS63_TIMEOUT_MS=500
```

`CONFIG_OV2640_WS63_FRAMESIZE` values:

```text
0 = QQVGA 160x120
1 = QVGA  320x240
2 = CIF   352x288
3 = VGA   640x480
4 = SVGA  800x600
```

`CONFIG_OV2640_WS63_JPEG_QUALITY` ranges from `0..63`. Lower values mean better image quality and larger JPEG frames; higher values mean lower quality and smaller JPEG frames. Suggested starting points:

```text
QVGA: quality 10~20, buffer 65536
VGA:  quality 16~30, buffer 131072
SVGA: quality 20~35, buffer 196608
```

`CONFIG_OV2640_WS63_UART_BAUDRATE` must match your PC-side XCAM serial setting. Common values:

```text
115200
460800
921600
1500000
2000000
```

Higher-resolution JPEG frames need a higher UART baudrate, otherwise the frame rate will drop noticeably.

### SCCB Microsecond Delay

Some OV2640 modules are sensitive to SCCB timing. The driver calls the following by default after each SCCB read/write:

```c
#include "tcxo.h"

uapi_tcxo_init();
uapi_tcxo_delay_us(50);
```

Tune this with `ov2640_ws63_config_t.sccb_delay_us`. Suggested values:

```text
20us: short wiring and stable SCCB
50us: default value
100us: try this when probing or register writes fail intermittently
```

### Important Notes

1. `OV2640` requires an external `XCLK`, typically 6~24MHz. This library does not generate high-speed XCLK in software; provide it via a peripheral clock, PWM, or an external oscillator.
2. GPIO polling is a software DVP implementation. Its maximum throughput depends on CPU speed and GPIO read overhead. Start with low resolution and low frame rate.
3. Use a dedicated UART for XCAM when possible. If XCAM shares the system log UART, `osal_printk` output can corrupt the binary JPEG stream.
4. The demo sends raw JPEG binary bytes, not hexadecimal text. Configure the PC-side viewer to receive a JPEG byte stream.
