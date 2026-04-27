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

typedef enum {
    OV2640_WS63_PIXFORMAT_RGB565 = 0,
    OV2640_WS63_PIXFORMAT_JPEG,
} ov2640_ws63_pixformat_t;

typedef enum {
    OV2640_WS63_FRAMESIZE_QQVGA = 0, /* 160x120 */
    OV2640_WS63_FRAMESIZE_QVGA,      /* 320x240 */
    OV2640_WS63_FRAMESIZE_CIF,       /* 352x288 */
    OV2640_WS63_FRAMESIZE_VGA,       /* 640x480 */
    OV2640_WS63_FRAMESIZE_SVGA,      /* 800x600 */
} ov2640_ws63_framesize_t;

typedef struct {
    i2c_bus_t i2c_bus;
    uint16_t sccb_addr_7bit;
    uint32_t i2c_baudrate;
    uint32_t sccb_delay_us;
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
    ov2640_ws63_pixformat_t pixformat;
    ov2640_ws63_framesize_t framesize;
    uint8_t jpeg_quality; /* 0-63, lower is better quality/larger frame. */
    uint8_t pclk_div;     /* OV2640 DVP PCLK divider, 0 means default. */
} ov2640_ws63_params_t;

typedef struct {
    uint16_t width;
    uint16_t height;
} ov2640_ws63_frame_info_t;

typedef struct {
    ov2640_ws63_config_t cfg;
    bool inited;
    ov2640_ws63_params_t params;
} ov2640_ws63_t;

errcode_t ov2640_ws63_init(ov2640_ws63_t *dev, const ov2640_ws63_config_t *cfg);
errcode_t ov2640_ws63_probe(ov2640_ws63_t *dev, uint8_t *pid, uint8_t *ver);
errcode_t ov2640_ws63_reset(ov2640_ws63_t *dev);
errcode_t ov2640_ws63_set_rgb565_cif(ov2640_ws63_t *dev);
errcode_t ov2640_ws63_configure(ov2640_ws63_t *dev, const ov2640_ws63_params_t *params);
errcode_t ov2640_ws63_set_jpeg_quality(ov2640_ws63_t *dev, uint8_t quality);
errcode_t ov2640_ws63_get_frame_info(ov2640_ws63_framesize_t framesize, ov2640_ws63_frame_info_t *info);
errcode_t ov2640_ws63_capture_frame(ov2640_ws63_t *dev, uint8_t *frame_buf, size_t buf_size, size_t *captured_len,
    uint32_t timeout_ms);
errcode_t ov2640_ws63_find_jpeg(const uint8_t *frame_buf, size_t frame_len, size_t *jpeg_offset, size_t *jpeg_len);

#ifdef __cplusplus
}
#endif

#endif
