#include "ov2640_ws63.h"

#include <string.h>

#include "common_def.h"
#include "gpio.h"
#include "i2c.h"
#include "soc_osal.h"

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

#define OV2640_IMAGE_MODE_RGB565 0x08
#define OV2640_RESET_DVP 0x04
#define OV2640_R_BYPASS_DSP_EN 0x00
#define OV2640_R_DVP_SP_AUTO_MODE 0x80

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

static errcode_t ov2640_sccb_write_reg(ov2640_ws63_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    i2c_data_t data = {
        .send_buf = tx,
        .send_len = sizeof(tx),
        .receive_buf = NULL,
        .receive_len = 0,
    };
    return uapi_i2c_master_write(dev->cfg.i2c_bus, dev->cfg.sccb_addr_7bit, &data);
}

static errcode_t ov2640_sccb_read_reg(ov2640_ws63_t *dev, uint8_t reg, uint8_t *val)
{
    i2c_data_t data = {
        .send_buf = &reg,
        .send_len = 1,
        .receive_buf = val,
        .receive_len = 1,
    };
    return uapi_i2c_master_writeread(dev->cfg.i2c_bus, dev->cfg.sccb_addr_7bit, &data);
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

    uapi_pin_init();
    uapi_gpio_init();

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
    errcode_t ret;
    if ((dev == NULL) || (!dev->inited)) {
        return ERRCODE_INVALID_PARAM;
    }

    ret = ov2640_ws63_reset(dev);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_table(dev, g_ov2640_init_cif);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_table(dev, g_ov2640_to_cif);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = ov2640_write_table(dev, g_ov2640_rgb565);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
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
