# OV2640 on WS63 (GPIO-Emulated DVP)

这个库把 `OV2640` 从常见 `STM32F103` Demo 迁移到了 `WS63` 标准库接口：

- SCCB(I2C): `uapi_i2c_master_*`
- GPIO/PIN: `uapi_gpio_*` + `uapi_pin_*`
- TCXO us delay: `uapi_tcxo_init()` + `uapi_tcxo_delay_us()`
- UART output: `uapi_uart_init()` + `uapi_uart_write()`
- 采样方式: 用 GPIO 轮询模拟 DVP 并行口 (`PCLK + VSYNC + HREF + D0..D7`)
- 输出格式: `RGB565` 或 `JPEG`

## 目录

- `include/ov2640_ws63.h`: 对外 API
- `src/ov2640_ws63.c`: 驱动实现
- `demo/ov2640_ws63_demo.c`: JPEG 抓帧并通过 UART 连续输出给 XCAM 的示例任务
- `Kconfig` / `CMakeLists.txt`: 组件接入

## 快速集成

1. 将 `ov2640_ws63` 目录拷贝到你的 WS63 工程（常见放到 `application/samples/custom/`）。
2. 在父级 `CMakeLists.txt` 中 `add_subdirectory_if_exist(ov2640_ws63)`。
3. 在父级 `Kconfig` 中 `source "ov2640_ws63/Kconfig"`。
4. 打开 `CONFIG_OV2640_WS63_DEMO`（可选），并按你的板子改 `demo/ov2640_ws63_demo.c` 里的引脚映射。

## API

- `ov2640_ws63_init()`: 初始化 I2C、DVP GPIO、控制脚
- `ov2640_ws63_probe()`: 读取 `PID/VER`
- `ov2640_ws63_set_rgb565_cif()`: 配置到 `RGB565 + CIF`
- `ov2640_ws63_configure()`: 配置格式、分辨率、JPEG 质量、PCLK 分频
- `ov2640_ws63_set_jpeg_quality()`: 运行时调整 JPEG 质量
- `ov2640_ws63_capture_frame()`: 采一帧原始字节流
- `ov2640_ws63_find_jpeg()`: 从 DVP 字节流中裁出 `0xffd8 ... 0xffd9` JPEG 数据

## JPEG + XCAM 串口输出

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

## SCCB 微秒延时

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

## 关键说明

1. `OV2640` 需要外部 `XCLK`（典型 6~24MHz）。本库未在软件里生成高速 XCLK，需由外设时钟/PWM/外部晶振提供。
2. GPIO 轮询采样属于“软 DVP”，速率上限受 CPU 和 GPIO 读取开销影响。建议先从低帧率、低分辨率验证。
3. XCAM 串口最好使用独立 UART。如果和系统日志 UART 共用，`osal_printk` 日志会混入 JPEG 字节流，导致电脑端显示异常。
4. Demo 发送的是 JPEG 原始二进制数据，不是十六进制文本；电脑端按 JPEG 流接收即可。
