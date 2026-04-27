# OV2640 on WS63 (GPIO-Emulated DVP)

这个库把 `OV2640` 从常见 `STM32F103` Demo 迁移到了 `WS63` 标准库接口：

- SCCB(I2C): `uapi_i2c_master_*`
- GPIO/PIN: `uapi_gpio_*` + `uapi_pin_*`
- 采样方式: 用 GPIO 轮询模拟 DVP 并行口 (`PCLK + VSYNC + HREF + D0..D7`)

## 目录

- `include/ov2640_ws63.h`: 对外 API
- `src/ov2640_ws63.c`: 驱动实现
- `demo/ov2640_ws63_demo.c`: 可直接放进 WS63 工程的示例任务
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
- `ov2640_ws63_capture_frame()`: 采一帧原始字节流

## 关键说明

1. `OV2640` 需要外部 `XCLK`（典型 6~24MHz）。本库未在软件里生成高速 XCLK，需由外设时钟/PWM/外部晶振提供。
2. GPIO 轮询采样属于“软 DVP”，速率上限受 CPU 和 GPIO 读取开销影响。建议先从低帧率、低分辨率验证。
3. 本库输出的是原始并口字节流（RGB565 时每像素 2 字节），后处理（保存、传输、显示）由上层完成。
