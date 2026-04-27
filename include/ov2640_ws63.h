#ifndef OV2640_WS63_H
#define OV2640_WS63_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "errcode.h"
#include "platform_core.h"
#include "pinctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV2640_WS63_SCCB_ADDR_7BIT 0x30

typedef struct {
    i2c_bus_t i2c_bus;
    uint16_t sccb_addr_7bit;
    uint32_t i2c_baudrate;
    pin_t pin_scl;
    pin_t pin_sda;
    pin_mode_t i2c_pin_mode;

    pin_t pin_pwdn;
    pin_t pin_reset;
    bool has_pwdn_pin;
    bool has_reset_pin;

    pin_t pin_pclk;
    pin_t pin_vsync;
    pin_t pin_href;
    pin_t pin_d[8];

    bool pclk_sample_rising;
    bool vsync_active_high;
    bool href_active_high;
    pin_pull_t dvp_pull;
} ov2640_ws63_config_t;

typedef struct {
    ov2640_ws63_config_t cfg;
    bool inited;
} ov2640_ws63_t;

errcode_t ov2640_ws63_init(ov2640_ws63_t *dev, const ov2640_ws63_config_t *cfg);
errcode_t ov2640_ws63_probe(ov2640_ws63_t *dev, uint8_t *pid, uint8_t *ver);
errcode_t ov2640_ws63_reset(ov2640_ws63_t *dev);
errcode_t ov2640_ws63_set_rgb565_cif(ov2640_ws63_t *dev);
errcode_t ov2640_ws63_capture_frame(ov2640_ws63_t *dev, uint8_t *frame_buf, size_t buf_size, size_t *captured_len,
    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
