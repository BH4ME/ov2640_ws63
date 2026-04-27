#include "ov2640_ws63.h"

#include <string.h>

#include "common_def.h"
#include "gpio.h"
#include "i2c.h"
#include "soc_osal.h"
#include "tcxo.h"

typedef struct {
    uint8_t reg;
    uint8_t val;
} ov2640_regval_t;

/* OV2640 register definitions (subset). */
#define OV2640_REG_BANK_SEL 0xFF
#define OV2640_BANK_DSP 0x00
#define OV2640_BANK_SENSOR 0x01
#define OV2640_REG_COM7 0x12
#define OV2640_COM7_SRST 0x80
#define OV2640_REG_PID 0x0A
#define OV2640_REG_VER 0x0B
#define OV2640_REG_IMAGE_MODE 0xDA
#define OV2640_REG_RESET 0xE0
#define OV2640_REG_R_BYPASS 0x05
#define OV2640_REG_R_DVP_SP 0xD3
#define OV2640_REG_QS 0x44
#define OV2640_REG_CLKRC 0x11
#define OV2640_REG_HSIZE 0x51
#define OV2640_REG_VSIZE 0x52
#define OV2640_REG_XOFFL 0x53
#define OV2640_REG_YOFFL 0x54
#define OV2640_REG_VHYX 0x55
#define OV2640_REG_TEST 0x57
#define OV2640_REG_ZMOW 0x5A
#define OV2640_REG_ZMOH 0x5B
#define OV2640_REG_ZMHH 0x5C

#define OV2640_IMAGE_MODE_RGB565 0x08
#define OV2640_IMAGE_MODE_JPEG_EN 0x10
#define OV2640_IMAGE_MODE_HREF_VSYNC 0x02
#define OV2640_RESET_DVP 0x04
#define OV2640_RESET_JPEG 0x10
#define OV2640_R_BYPASS_DSP_EN 0x00
#define OV2640_R_DVP_SP_AUTO_MODE 0x80

#define OV2640_DEFAULT_SCCB_DELAY_US 50
#define OV2640_DEFAULT_JPEG_QUALITY 12
#define OV2640_DEFAULT_PCLK_DIV 8

typedef enum {
    OV2640_SENSOR_MODE_CIF = 0,
    OV2640_SENSOR_MODE_SVGA,
} ov2640_sensor_mode_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    ov2640_sensor_mode_t mode;
    uint16_t source_width;
    uint16_t source_height;
} ov2640_framesize_desc_t;

static const ov2640_framesize_desc_t g_framesize_desc[] = {
    [OV2640_WS63_FRAMESIZE_QQVGA] = {160, 120, OV2640_SENSOR_MODE_CIF, 400, 296},
    [OV2640_WS63_FRAMESIZE_QVGA] = {320, 240, OV2640_SENSOR_MODE_CIF, 400, 296},
    [OV2640_WS63_FRAMESIZE_CIF] = {352, 288, OV2640_SENSOR_MODE_CIF, 400, 296},
    [OV2640_WS63_FRAMESIZE_VGA] = {640, 480, OV2640_SENSOR_MODE_SVGA, 800, 600},
    [OV2640_WS63_FRAMESIZE_SVGA] = {800, 600, OV2640_SENSOR_MODE_SVGA, 800, 600},
};

/* Based on public OV2640 initialization settings (OpenMV/esp32-camera lineage). */
static const ov2640_regval_t g_ov2640_init_cif[] = {
    {OV2640_REG_BANK_SEL, OV2640_BANK_DSP},
    {0x2c, 0xff}, {0x2e, 0xdf},
    {OV2640_REG_BANK_SEL, OV2640_BANK_SENSOR},
    {0x3c, 0x32}, {0x11, 0x01}, {0x09, 0x02}, {0x04, 0x28}, {0x13, 0xe5}, {0x14, 0x48},
    {0x2c, 0x0c}, {0x33, 0x78}, {0x3a, 0x33}, {0x3b, 0xfb}, {0x3e, 0x00}, {0x43, 0x11},
    {0x16, 0x10}, {0x39, 0x92}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3}, {0x23, 0x00},
    {0x34, 0xc0}, {0x06, 0x88}, {0x07, 0xc0}, {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00},
    {0x4a, 0x81}, {0x21, 0x99}, {0x24, 0x40}, {0x25, 0x38}, {0x26, 0x82}, {0x5c, 0x00},
    {0x63, 0x00}, {0x61, 0x70}, {0x62, 0x80}, {0x7c, 0x05}, {0x20, 0x80}, {0x28, 0x30},
    {0x6c, 0x00}, {0x6d, 0x80}, {0x6e, 0x00}, {0x70, 0x02}, {0x71, 0x94}, {0x73, 0xc1},
    {0x3d, 0x34}, {0x5a, 0x57}, {0x4f, 0xbb}, {0x50, 0x9c}, {0x12, 0x20}, {0x17, 0x11},
    {0x18, 0x43}, {0x19, 0x00}, {0x1a, 0x25}, {0x32, 0x89}, {0x37, 0xc0}, {0x4f, 0xca},
    {0x50, 0xa8}, {0x6d, 0x00}, {0x3d, 0x38},
    {OV2640_REG_BANK_SEL, OV2640_BANK_DSP},
    {0xe5, 0x7f}, {0xf9, 0xc0}, {0x41, 0x24}, {OV2640_REG_RESET, 0x14}, {0x76, 0xff},
    {0x33, 0xa0}, {0x42, 0x20}, {0x43, 0x18}, {0x4c, 0x00}, {0x87, 0x50}, {0x88, 0x3f},
    {0xd7, 0x03}, {0xd9, 0x10}, {OV2640_REG_R_DVP_SP, 0x82}, {0xc8, 0x08}, {0xc9, 0x80},
    {0x7c, 0x00}, {0x7d, 0x00}, {0x7c, 0x03}, {0x7d, 0x48}, {0x7d, 0x48}, {0x7c, 0x08},
    {0x7d, 0x20}, {0x7d, 0x10}, {0x7d, 0x0e}, {0x90, 0x00}, {0x91, 0x0e}, {0x91, 0x1a},
    {0x91, 0x31}, {0x91, 0x5a}, {0x91, 0x69}, {0x91, 0x75}, {0x91, 0x7e}, {0x91, 0x88},
    {0x91, 0x8f}, {0x91, 0x96}, {0x91, 0xa3}, {0x91, 0xaf}, {0x91, 0xc4}, {0x91, 0xd7},
    {0x91, 0xe8}, {0x91, 0x20}, {0x92, 0x00}, {0x93, 0x06}, {0x93, 0xe3}, {0x93, 0x05},
    {0x93, 0x05}, {0x93, 0x00}, {0x93, 0x04}, {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00},
    {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00}, {0x93, 0x00}, {0x96, 0x00}, {0x97, 0x08},
    {0x97, 0x19}, {0x97, 0x02}, {0x97, 0x0c}, {0x97, 0x24}, {0x97, 0x30}, {0x97, 0x28},
    {0x97, 0x26}, {0x97, 0x02}, {0x97, 0x98}, {0x97, 0x80}, {0x97, 0x00}, {0x97, 0x00},
    {0xa4, 0x00}, {0xa8, 0x00}, {0xc5, 0x11}, {0xc6, 0x51}, {0xbf, 0x80}, {0xc7, 0x10},
    {0xb6, 0x66}, {0xb8, 0xa5}, {0xb7, 0x64}, {0xb9, 0x7c}, {0xb3, 0xaf}, {0xb4, 0x97},
    {0xb5, 0xff}, {0xb0, 0xc5}, {0xb1, 0x94}, {0xb2, 0x0f}, {0xc4, 0x5c}, {0xc3, 0xfd},
    {0x7f, 0x00}, {0xe5, 0x1f}, {0xe1, 0x67}, {0xdd, 0x7f}, {OV2640_REG_IMAGE_MODE, 0x00},
    {OV2640_REG_RESET, 0x00}, {OV2640_REG_R_BYPASS, OV2640_R_BYPASS_DSP_EN},
    {0x00, 0x00},
};

static const ov2640_regval_t g_ov2640_to_cif[] = {
    {OV2640_REG_BANK_SEL, OV2640_BANK_SENSOR},
    {0x12, 0x20}, {0x03, 0x0a}, {0x32, 0x89}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1a, 0x25}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23}, {0x6d, 0x00}, {0x3d, 0x38},
    {0x39, 0x92}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3}, {0x23, 0x00}, {0x34, 0xc0},
    {0x06, 0x88}, {0x07, 0xc0}, {0x0d, 0x87}, {0x0e, 0x41}, {0x4c, 0x00},
    {OV2640_REG_BANK_SEL, OV2640_BANK_DSP},
    {OV2640_REG_RESET, OV2640_RESET_DVP}, {0xc0, 0x32}, {0xc1, 0x25}, {0x8c, 0x00},
    {0x51, 0x64}, {0x52, 0x4a}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x86, 0x3d}, {0x50, 0x80}, {0x00, 0x00},
};

static const ov2640_regval_t g_ov2640_to_svga[] = {
    {OV2640_REG_BANK_SEL, OV2640_BANK_SENSOR},
    {0x12, 0x40}, {0x03, 0x0a}, {0x32, 0x09}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1a, 0x4b}, {0x37, 0xc0}, {0x4f, 0xca}, {0x50, 0xa8}, {0x5a, 0x23}, {0x6d, 0x00},
    {0x3d, 0x38}, {0x39, 0x92}, {0x35, 0xda}, {0x22, 0x1a}, {0x37, 0xc3}, {0x23, 0x00},
    {0x34, 0xc0}, {0x06, 0x88}, {0x07, 0xc0}, {0x0d, 0x87}, {0x0e, 0x41}, {0x42, 0x03},
    {0x4c, 0x00},
    {OV2640_REG_BANK_SEL, OV2640_BANK_DSP},
    {OV2640_REG_RESET, OV2640_RESET_DVP}, {0xc0, 0x64}, {0xc1, 0x4b}, {0x8c, 0x00},
    {0x51, 0xc8}, {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x86, 0x3d}, {0x50, 0x80}, {0x00, 0x00},
};

static const ov2640_regval_t g_ov2640_rgb565[] = {
    {OV2640_REG_BANK_SEL, OV2640_BANK_DSP},
    {OV2640_REG_RESET, OV2640_RESET_DVP},
    {OV2640_REG_IMAGE_MODE, OV2640_IMAGE_MODE_RGB565},
    {0xd7, 0x03},
    {0xe1, 0x77},
    {OV2640_REG_RESET, 0x00},
    {OV2640_REG_R_DVP_SP, OV2640_R_DVP_SP_AUTO_MODE | 0x02},
    {0x00, 0x00},
};

static const ov2640_regval_t g_ov2640_jpeg[] = {
    {OV2640_REG_BANK_SEL, OV2640_BANK_DSP},
    {OV2640_REG_RESET, OV2640_RESET_JPEG | OV2640_RESET_DVP},
    {OV2640_REG_IMAGE_MODE, OV2640_IMAGE_MODE_JPEG_EN | OV2640_IMAGE_MODE_HREF_VSYNC},
    {0xd7, 0x03}, {0xe1, 0x77}, {0xe5, 0x1f}, {0xd9, 0x10}, {0xdf, 0x80}, {0x33, 0x80},
    {0x3c, 0x10}, {0xeb, 0x30}, {0xdd, 0x7f}, {OV2640_REG_RESET, 0x00}, {0x00, 0x00},
};

static errcode_t ov2640_delay_after_sccb(ov2640_ws63_t *dev)
{
    uint32_t delay_us = dev->cfg.sccb_delay_us;
    if (delay_us == 0) {
        delay_us = OV2640_DEFAULT_SCCB_DELAY_US;
    }
    return uapi_tcxo_delay_us(delay_us);
}

static errcode_t ov2640_sccb_write_reg(ov2640_ws63_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    i2c_data_t data = {
        .send_buf = tx,
        .send_len = sizeof(tx),
        .receive_buf = NULL,
        .receive_len = 0,
    };
    errcode_t ret = uapi_i2c_master_write(dev->cfg.i2c_bus, dev->cfg.sccb_addr_7bit, &data);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return ov2640_delay_after_sccb(dev);
}

static errcode_t ov2640_sccb_read_reg(ov2640_ws63_t *dev, uint8_t reg, uint8_t *val)
{
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = val,
        .receive_len = 1,
    };
    errcode_t ret = uapi_i2c_master_writeread(dev->cfg.i2c_bus, dev->cfg.sccb_addr_7bit, &data);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return ov2640_delay_after_sccb(dev);
}

static errcode_t ov2640_write_table(ov2640_ws63_t *dev, const ov2640_regval_t *tbl)
{
    uint32_t i = 0;
    errcode_t ret;
    while ((tbl[i].reg != 0x00) || (tbl[i].val != 0x00)) {
        ret = ov2640_sccb_write_reg(dev, tbl[i].reg, tbl[i].val);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        i++;
    }
    return ERRCODE_SUCC;
}

static inline gpio_level_t ov2640_level_of(bool high)
{
    return high ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW;
}

static inline bool ov2640_pin_is_active(pin_t pin, bool active_high)
{
    gpio_level_t lv = uapi_gpio_get_val(pin);
    return active_high ? (lv == GPIO_LEVEL_HIGH) : (lv == GPIO_LEVEL_LOW);
}

static errcode_t ov2640_wait_pin_state(pin_t pin, bool high, uint32_t *budget)
{
    gpio_level_t expect = ov2640_level_of(high);
    while (uapi_gpio_get_val(pin) != expect) {
        if (*budget == 0) {
            return ERRCODE_I2C_TIMEOUT;
        }
        (*budget)--;
    }
    return ERRCODE_SUCC;
}

static errcode_t ov2640_wait_sample_edge(ov2640_ws63_t *dev, uint32_t *budget)
{
    if (dev->cfg.pclk_sample_rising) {
        errcode_t ret = ov2640_wait_pin_state(dev->cfg.pin_pclk, false, budget);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        return ov2640_wait_pin_state(dev->cfg.pin_pclk, true, budget);
    }

    errcode_t ret = ov2640_wait_pin_state(dev->cfg.pin_pclk, true, budget);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return ov2640_wait_pin_state(dev->cfg.pin_pclk, false, budget);
}

static uint8_t ov2640_read_data_bus(const ov2640_ws63_t *dev)
{
    uint8_t byte = 0;
    uint32_t i;
    for (i = 0; i < 8; i++) {
        if (uapi_gpio_get_val(dev->cfg.pin_d[i]) == GPIO_LEVEL_HIGH) {
            byte |= (uint8_t)(1U << i);
        }
    }
    return byte;
}

static errcode_t ov2640_write_reg(ov2640_ws63_t *dev, uint8_t bank, uint8_t reg, uint8_t val)
{
    errcode_t ret = ov2640_sccb_write_reg(dev, OV2640_REG_BANK_SEL, bank);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return ov2640_sccb_write_reg(dev, reg, val);
}

static errcode_t ov2640_set_window(ov2640_ws63_t *dev, const ov2640_framesize_desc_t *desc)
{
    uint16_t offset_x = (uint16_t)((desc->source_width - desc->width) / 2);
    uint16_t offset_y = (uint16_t)((desc->source_height - desc->height) / 2);
    uint16_t source_w = (uint16_t)(desc->source_width / 4);
    uint16_t source_h = (uint16_t)(desc->source_height / 4);
    uint16_t out_w = (uint16_t)(desc->width / 4);
    uint16_t out_h = (uint16_t)(desc->height / 4);
    uint16_t off_x = (uint16_t)(offset_x / 4);
    uint16_t off_y = (uint16_t)(offset_y / 4);
    errcode_t ret;

    ret = ov2640_sccb_write_reg(dev, OV2640_REG_BANK_SEL, OV2640_BANK_DSP);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_HSIZE, (uint8_t)(source_w & 0xff));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_VSIZE, (uint8_t)(source_h & 0xff));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_XOFFL, (uint8_t)(off_x & 0xff));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_YOFFL, (uint8_t)(off_y & 0xff));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_VHYX,
        (uint8_t)(((source_h >> 1) & 0x80) | ((off_y >> 4) & 0x70) |
        ((source_w >> 5) & 0x08) | ((off_x >> 8) & 0x07)));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_TEST, (uint8_t)((source_w >> 2) & 0x80));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_ZMOW, (uint8_t)(out_w & 0xff));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_ZMOH, (uint8_t)(out_h & 0xff));
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    return ov2640_sccb_write_reg(dev, OV2640_REG_ZMHH,
        (uint8_t)(((out_h >> 6) & 0x04) | ((out_w >> 8) & 0x03)));
}

errcode_t ov2640_ws63_init(ov2640_ws63_t *dev, const ov2640_ws63_config_t *cfg)
{
    uint32_t i;
    errcode_t ret;
    if ((dev == NULL) || (cfg == NULL)) {
        return ERRCODE_INVALID_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    memcpy(&dev->cfg, cfg, sizeof(dev->cfg));
    if (dev->cfg.sccb_addr_7bit == 0) {
        dev->cfg.sccb_addr_7bit = OV2640_WS63_SCCB_ADDR_7BIT;
    }
    if (dev->cfg.i2c_baudrate == 0) {
        dev->cfg.i2c_baudrate = 100000;
    }
    if (dev->cfg.sccb_delay_us == 0) {
        dev->cfg.sccb_delay_us = OV2640_DEFAULT_SCCB_DELAY_US;
    }

    uapi_pin_init();
    uapi_gpio_init();
    ret = uapi_tcxo_init();
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_pin_set_mode(dev->cfg.pin_scl, dev->cfg.i2c_pin_mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_mode(dev->cfg.pin_sda, dev->cfg.i2c_pin_mode);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    ret = uapi_i2c_master_init(dev->cfg.i2c_bus, dev->cfg.i2c_baudrate, 0);
    if ((ret != ERRCODE_SUCC) && (ret != ERRCODE_I2C_ALREADY_INIT)) {
        return ret;
    }

    if (dev->cfg.has_pwdn_pin) {
        ret = uapi_pin_set_mode(dev->cfg.pin_pwdn, HAL_PIO_FUNC_GPIO);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        ret = uapi_gpio_set_dir(dev->cfg.pin_pwdn, GPIO_DIRECTION_OUTPUT);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        uapi_gpio_set_val(dev->cfg.pin_pwdn, GPIO_LEVEL_LOW);
    }

    if (dev->cfg.has_reset_pin) {
        ret = uapi_pin_set_mode(dev->cfg.pin_reset, HAL_PIO_FUNC_GPIO);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        ret = uapi_gpio_set_dir(dev->cfg.pin_reset, GPIO_DIRECTION_OUTPUT);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        uapi_gpio_set_val(dev->cfg.pin_reset, GPIO_LEVEL_HIGH);
    }

    ret = uapi_pin_set_mode(dev->cfg.pin_pclk, HAL_PIO_FUNC_GPIO);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_mode(dev->cfg.pin_vsync, HAL_PIO_FUNC_GPIO);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_mode(dev->cfg.pin_href, HAL_PIO_FUNC_GPIO);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_pull(dev->cfg.pin_pclk, dev->cfg.dvp_pull);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_pull(dev->cfg.pin_vsync, dev->cfg.dvp_pull);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_pull(dev->cfg.pin_href, dev->cfg.dvp_pull);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_gpio_set_dir(dev->cfg.pin_pclk, GPIO_DIRECTION_INPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_gpio_set_dir(dev->cfg.pin_vsync, GPIO_DIRECTION_INPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_gpio_set_dir(dev->cfg.pin_href, GPIO_DIRECTION_INPUT);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    for (i = 0; i < 8; i++) {
        ret = uapi_pin_set_mode(dev->cfg.pin_d[i], HAL_PIO_FUNC_GPIO);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        ret = uapi_pin_set_pull(dev->cfg.pin_d[i], dev->cfg.dvp_pull);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        ret = uapi_gpio_set_dir(dev->cfg.pin_d[i], GPIO_DIRECTION_INPUT);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
    }

    dev->inited = true;
    dev->params.pixformat = OV2640_WS63_PIXFORMAT_RGB565;
    dev->params.framesize = OV2640_WS63_FRAMESIZE_CIF;
    dev->params.jpeg_quality = OV2640_DEFAULT_JPEG_QUALITY;
    dev->params.pclk_div = OV2640_DEFAULT_PCLK_DIV;
    return ERRCODE_SUCC;
}

errcode_t ov2640_ws63_probe(ov2640_ws63_t *dev, uint8_t *pid, uint8_t *ver)
{
    uint8_t p = 0;
    uint8_t v = 0;
    errcode_t ret;
    if ((dev == NULL) || (!dev->inited)) {
        return ERRCODE_INVALID_PARAM;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_BANK_SEL, OV2640_BANK_SENSOR);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_read_reg(dev, OV2640_REG_PID, &p);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_read_reg(dev, OV2640_REG_VER, &v);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    if (pid != NULL) {
        *pid = p;
    }
    if (ver != NULL) {
        *ver = v;
    }
    return ERRCODE_SUCC;
}

errcode_t ov2640_ws63_reset(ov2640_ws63_t *dev)
{
    errcode_t ret;
    if ((dev == NULL) || (!dev->inited)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (dev->cfg.has_pwdn_pin) {
        uapi_gpio_set_val(dev->cfg.pin_pwdn, GPIO_LEVEL_LOW);
    }
    if (dev->cfg.has_reset_pin) {
        uapi_gpio_set_val(dev->cfg.pin_reset, GPIO_LEVEL_LOW);
        osal_msleep(2);
        uapi_gpio_set_val(dev->cfg.pin_reset, GPIO_LEVEL_HIGH);
    }
    osal_msleep(5);

    ret = ov2640_sccb_write_reg(dev, OV2640_REG_BANK_SEL, OV2640_BANK_SENSOR);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_sccb_write_reg(dev, OV2640_REG_COM7, OV2640_COM7_SRST);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    osal_msleep(10);

    return ERRCODE_SUCC;
}

errcode_t ov2640_ws63_set_rgb565_cif(ov2640_ws63_t *dev)
{
    ov2640_ws63_params_t params = {
        .pixformat = OV2640_WS63_PIXFORMAT_RGB565,
        .framesize = OV2640_WS63_FRAMESIZE_CIF,
        .jpeg_quality = OV2640_DEFAULT_JPEG_QUALITY,
        .pclk_div = OV2640_DEFAULT_PCLK_DIV,
    };
    return ov2640_ws63_configure(dev, &params);
}

errcode_t ov2640_ws63_set_jpeg_quality(ov2640_ws63_t *dev, uint8_t quality)
{
    if ((dev == NULL) || (!dev->inited) || (quality > 63)) {
        return ERRCODE_INVALID_PARAM;
    }
    dev->params.jpeg_quality = quality;
    return ov2640_write_reg(dev, OV2640_BANK_DSP, OV2640_REG_QS, quality);
}

errcode_t ov2640_ws63_get_frame_info(ov2640_ws63_framesize_t framesize, ov2640_ws63_frame_info_t *info)
{
    if ((info == NULL) || (framesize >= (ov2640_ws63_framesize_t)array_size(g_framesize_desc))) {
        return ERRCODE_INVALID_PARAM;
    }
    info->width = g_framesize_desc[framesize].width;
    info->height = g_framesize_desc[framesize].height;
    return ERRCODE_SUCC;
}

errcode_t ov2640_ws63_configure(ov2640_ws63_t *dev, const ov2640_ws63_params_t *params)
{
    errcode_t ret;
    ov2640_ws63_params_t use_params;
    const ov2640_framesize_desc_t *desc;
    if ((dev == NULL) || (!dev->inited)) {
        return ERRCODE_INVALID_PARAM;
    }

    if (params == NULL) {
        use_params.pixformat = OV2640_WS63_PIXFORMAT_RGB565;
        use_params.framesize = OV2640_WS63_FRAMESIZE_CIF;
        use_params.jpeg_quality = OV2640_DEFAULT_JPEG_QUALITY;
        use_params.pclk_div = OV2640_DEFAULT_PCLK_DIV;
    } else {
        use_params = *params;
    }
    if ((use_params.pixformat > OV2640_WS63_PIXFORMAT_JPEG) ||
        (use_params.framesize >= (ov2640_ws63_framesize_t)array_size(g_framesize_desc)) ||
        (use_params.jpeg_quality > 63)) {
        return ERRCODE_INVALID_PARAM;
    }
    if (use_params.pclk_div == 0) {
        use_params.pclk_div = OV2640_DEFAULT_PCLK_DIV;
    }
    desc = &g_framesize_desc[use_params.framesize];

    ret = ov2640_ws63_reset(dev);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_table(dev, g_ov2640_init_cif);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_table(dev, desc->mode == OV2640_SENSOR_MODE_CIF ? g_ov2640_to_cif : g_ov2640_to_svga);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_set_window(dev, desc);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    if (use_params.pixformat == OV2640_WS63_PIXFORMAT_JPEG) {
        ret = ov2640_write_table(dev, g_ov2640_jpeg);
        if (ret != ERRCODE_SUCC) {
            return ret;
        }
        ret = ov2640_ws63_set_jpeg_quality(dev, use_params.jpeg_quality);
    } else {
        ret = ov2640_write_table(dev, g_ov2640_rgb565);
    }
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_reg(dev, OV2640_BANK_SENSOR, OV2640_REG_CLKRC, 0x00);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_reg(dev, OV2640_BANK_DSP, OV2640_REG_R_DVP_SP, use_params.pclk_div);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    dev->params = use_params;
    osal_msleep(10);
    return ERRCODE_SUCC;
}

errcode_t ov2640_ws63_capture_frame(ov2640_ws63_t *dev, uint8_t *frame_buf, size_t buf_size, size_t *captured_len,
    uint32_t timeout_ms)
{
    size_t out_idx = 0;
    uint32_t budget;
    errcode_t ret;

    if ((dev == NULL) || (!dev->inited) || (frame_buf == NULL) || (captured_len == NULL) || (buf_size == 0)) {
        return ERRCODE_INVALID_PARAM;
    }

    /*
     * Busy-loop timeout budget:
     * each unit is one polling iteration. 1ms -> about 800 loops gives decent margin
     * on WS63 while still guaranteeing timeout exit.
     */
    budget = (timeout_ms == 0 ? 500 : timeout_ms) * 800U;

    /* Sync to a frame boundary: wait for one VSYNC pulse (inactive -> active -> inactive). */
    ret = ov2640_wait_pin_state(dev->cfg.pin_vsync, !dev->cfg.vsync_active_high, &budget);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_wait_pin_state(dev->cfg.pin_vsync, dev->cfg.vsync_active_high, &budget);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_wait_pin_state(dev->cfg.pin_vsync, !dev->cfg.vsync_active_high, &budget);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }

    /* Capture the payload between two VSYNC pulses. */
    while (!ov2640_pin_is_active(dev->cfg.pin_vsync, dev->cfg.vsync_active_high)) {
        if (!ov2640_pin_is_active(dev->cfg.pin_href, dev->cfg.href_active_high)) {
            if (budget == 0) {
                return ERRCODE_I2C_TIMEOUT;
            }
            budget--;
            continue;
        }

        while (ov2640_pin_is_active(dev->cfg.pin_href, dev->cfg.href_active_high) &&
               !ov2640_pin_is_active(dev->cfg.pin_vsync, dev->cfg.vsync_active_high)) {
            ret = ov2640_wait_sample_edge(dev, &budget);
            if (ret != ERRCODE_SUCC) {
                return ret;
            }

            if (out_idx >= buf_size) {
                return ERRCODE_FAIL;
            }
            frame_buf[out_idx++] = ov2640_read_data_bus(dev);
        }
    }

    *captured_len = out_idx;
    return ERRCODE_SUCC;
}

errcode_t ov2640_ws63_find_jpeg(const uint8_t *frame_buf, size_t frame_len, size_t *jpeg_offset, size_t *jpeg_len)
{
    size_t start = 0;
    size_t end = 0;
    bool found_start = false;
    size_t i;

    if ((frame_buf == NULL) || (jpeg_offset == NULL) || (jpeg_len == NULL) || (frame_len < 4)) {
        return ERRCODE_INVALID_PARAM;
    }

    for (i = 0; i + 1 < frame_len; i++) {
        if ((frame_buf[i] == 0xff) && (frame_buf[i + 1] == 0xd8)) {
            start = i;
            found_start = true;
            break;
        }
    }
    if (!found_start) {
        return ERRCODE_FAIL;
    }

    for (i = start + 2; i + 1 < frame_len; i++) {
        if ((frame_buf[i] == 0xff) && (frame_buf[i + 1] == 0xd9)) {
            end = i + 2;
            *jpeg_offset = start;
            *jpeg_len = end - start;
            return ERRCODE_SUCC;
        }
    }

    return ERRCODE_FAIL;
}
