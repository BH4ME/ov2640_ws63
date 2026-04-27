#include "ov2640_ws63.h"

#include <string.h>

#include "app_init.h"
#include "soc_osal.h"

/*
 * Pin map here is only a template.
 * Replace with your real board wiring before running.
 */
#define CAM_SCL_PIN S_MGPIO16
#define CAM_SDA_PIN S_MGPIO15
#define CAM_I2C_PIN_MODE 2

#define CAM_PWDN_PIN S_MGPIO14
#define CAM_RESET_PIN S_MGPIO13

#define CAM_PCLK_PIN S_MGPIO0
#define CAM_VSYNC_PIN S_MGPIO1
#define CAM_HREF_PIN S_MGPIO2
#define CAM_D0_PIN S_MGPIO3
#define CAM_D1_PIN S_MGPIO4
#define CAM_D2_PIN S_MGPIO5
#define CAM_D3_PIN S_MGPIO6
#define CAM_D4_PIN S_MGPIO7
#define CAM_D5_PIN S_MGPIO8
#define CAM_D6_PIN S_MGPIO9
#define CAM_D7_PIN S_MGPIO10

#ifndef CONFIG_OV2640_WS63_TIMEOUT_MS
#define CONFIG_OV2640_WS63_TIMEOUT_MS 200
#endif

#ifndef CONFIG_OV2640_WS63_FRAME_BUF_SIZE
#define CONFIG_OV2640_WS63_FRAME_BUF_SIZE 202752
#endif

#define OV2640_TASK_STACK_SIZE 0x2000
#define OV2640_TASK_PRIO 24

static uint8_t g_frame_buf[CONFIG_OV2640_WS63_FRAME_BUF_SIZE];

static void *ov2640_demo_task(const char *arg)
{
    errcode_t ret;
    size_t frame_len = 0;
    uint8_t pid = 0;
    uint8_t ver = 0;
    ov2640_ws63_t cam;
    ov2640_ws63_config_t cfg;
    (void)arg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.i2c_bus = I2C_BUS_1;
    cfg.sccb_addr_7bit = OV2640_WS63_SCCB_ADDR_7BIT;
    cfg.i2c_baudrate = 100000;
    cfg.pin_scl = CAM_SCL_PIN;
    cfg.pin_sda = CAM_SDA_PIN;
    cfg.i2c_pin_mode = CAM_I2C_PIN_MODE;
    cfg.has_pwdn_pin = true;
    cfg.has_reset_pin = true;
    cfg.pin_pwdn = CAM_PWDN_PIN;
    cfg.pin_reset = CAM_RESET_PIN;
    cfg.pin_pclk = CAM_PCLK_PIN;
    cfg.pin_vsync = CAM_VSYNC_PIN;
    cfg.pin_href = CAM_HREF_PIN;
    cfg.pin_d[0] = CAM_D0_PIN;
    cfg.pin_d[1] = CAM_D1_PIN;
    cfg.pin_d[2] = CAM_D2_PIN;
    cfg.pin_d[3] = CAM_D3_PIN;
    cfg.pin_d[4] = CAM_D4_PIN;
    cfg.pin_d[5] = CAM_D5_PIN;
    cfg.pin_d[6] = CAM_D6_PIN;
    cfg.pin_d[7] = CAM_D7_PIN;
    cfg.pclk_sample_rising = true;
    cfg.vsync_active_high = true;
    cfg.href_active_high = true;
    cfg.dvp_pull = PIN_PULL_TYPE_DISABLE;

    ret = ov2640_ws63_init(&cam, &cfg);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ov2640] init fail: 0x%x\r\n", ret);
        return NULL;
    }

    ret = ov2640_ws63_probe(&cam, &pid, &ver);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ov2640] probe fail: 0x%x\r\n", ret);
        return NULL;
    }
    osal_printk("[ov2640] pid=0x%02x ver=0x%02x\r\n", pid, ver);

    ret = ov2640_ws63_set_rgb565_cif(&cam);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ov2640] configure fail: 0x%x\r\n", ret);
        return NULL;
    }

    ret = ov2640_ws63_capture_frame(&cam, g_frame_buf, sizeof(g_frame_buf), &frame_len, CONFIG_OV2640_WS63_TIMEOUT_MS);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ov2640] capture fail: 0x%x\r\n", ret);
        return NULL;
    }
    osal_printk("[ov2640] captured %u bytes\r\n", (unsigned int)frame_len);

    while (1) {
        osal_msleep(1000);
    }

    return NULL;
}

static void ov2640_demo_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)ov2640_demo_task, 0,
        "Ov2640Task", OV2640_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, OV2640_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

app_run(ov2640_demo_entry);
