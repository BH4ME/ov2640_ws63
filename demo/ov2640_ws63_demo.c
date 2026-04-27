#include "ov2640_ws63.h"

#include <string.h>

#include "app_init.h"
#include "pinctrl.h"
#include "soc_osal.h"
#include "uart.h"

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

#define XCAM_UART_BUS UART_BUS_1
#define XCAM_UART_TX_PIN S_AGPIO12
#define XCAM_UART_RX_PIN S_AGPIO13
#define XCAM_UART_PIN_MODE 2

#ifndef CONFIG_OV2640_WS63_TIMEOUT_MS
#define CONFIG_OV2640_WS63_TIMEOUT_MS 500
#endif

#ifndef CONFIG_OV2640_WS63_FRAME_BUF_SIZE
#define CONFIG_OV2640_WS63_FRAME_BUF_SIZE 65536
#endif

#ifndef CONFIG_OV2640_WS63_JPEG_QUALITY
#define CONFIG_OV2640_WS63_JPEG_QUALITY 12
#endif

#ifndef CONFIG_OV2640_WS63_PCLK_DIV
#define CONFIG_OV2640_WS63_PCLK_DIV 8
#endif

#ifndef CONFIG_OV2640_WS63_UART_BAUDRATE
#define CONFIG_OV2640_WS63_UART_BAUDRATE 921600
#endif

#ifndef CONFIG_OV2640_WS63_FRAMESIZE
#define CONFIG_OV2640_WS63_FRAMESIZE 1
#endif

#define OV2640_TASK_STACK_SIZE 0x2000
#define OV2640_TASK_PRIO 24
#define XCAM_UART_CHUNK_SIZE 512
#define XCAM_UART_WRITE_TIMEOUT 0xffffffff

static uint8_t g_frame_buf[CONFIG_OV2640_WS63_FRAME_BUF_SIZE];
static uint8_t g_uart_rx_buf[1];

static ov2640_ws63_framesize_t ov2640_demo_framesize_from_config(void)
{
    switch (CONFIG_OV2640_WS63_FRAMESIZE) {
        case 0:
            return OV2640_WS63_FRAMESIZE_QQVGA;
        case 1:
            return OV2640_WS63_FRAMESIZE_QVGA;
        case 2:
            return OV2640_WS63_FRAMESIZE_CIF;
        case 3:
            return OV2640_WS63_FRAMESIZE_VGA;
        case 4:
            return OV2640_WS63_FRAMESIZE_SVGA;
        default:
            return OV2640_WS63_FRAMESIZE_QVGA;
    }
}

static errcode_t xcam_uart_init(void)
{
    errcode_t ret;
    uart_attr_t attr = {
        .baud_rate = CONFIG_OV2640_WS63_UART_BAUDRATE,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity = UART_PARITY_NONE,
        .flow_ctrl = UART_FLOW_CTRL_NONE,
    };
    uart_pin_config_t pin_config = {
        .tx_pin = XCAM_UART_TX_PIN,
        .rx_pin = XCAM_UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE,
    };
    uart_buffer_config_t uart_buffer_config = {
        .rx_buffer = g_uart_rx_buf,
        .rx_buffer_size = sizeof(g_uart_rx_buf),
    };

    ret = uapi_pin_set_mode(XCAM_UART_TX_PIN, XCAM_UART_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    ret = uapi_pin_set_mode(XCAM_UART_RX_PIN, XCAM_UART_PIN_MODE);
    if (ret != ERRCODE_SUCC) {
        return ret;
    }
    (void)uapi_uart_deinit(XCAM_UART_BUS);
    return uapi_uart_init(XCAM_UART_BUS, &pin_config, &attr, NULL, &uart_buffer_config);
}

static errcode_t xcam_uart_write_all(const uint8_t *buf, size_t len)
{
    size_t offset = 0;
    while (offset < len) {
        uint32_t chunk = (uint32_t)((len - offset) > XCAM_UART_CHUNK_SIZE ?
            XCAM_UART_CHUNK_SIZE : (len - offset));
        int32_t written = uapi_uart_write(XCAM_UART_BUS, buf + offset, chunk, XCAM_UART_WRITE_TIMEOUT);
        if (written <= 0) {
            return ERRCODE_FAIL;
        }
        offset += (size_t)written;
    }
    return ERRCODE_SUCC;
}

static void *ov2640_demo_task(const char *arg)
{
    errcode_t ret;
    size_t frame_len = 0;
    uint8_t pid = 0;
    uint8_t ver = 0;
    ov2640_ws63_t cam;
    ov2640_ws63_config_t cfg;
    ov2640_ws63_params_t params;
    ov2640_ws63_frame_info_t frame_info;
    size_t jpeg_offset = 0;
    size_t jpeg_len = 0;
    (void)arg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.i2c_bus = I2C_BUS_1;
    cfg.sccb_addr_7bit = OV2640_WS63_SCCB_ADDR_7BIT;
    cfg.i2c_baudrate = 100000;
    cfg.sccb_delay_us = 50;
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

    ret = xcam_uart_init();
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ov2640] xcam uart init fail: 0x%x\r\n", ret);
        return NULL;
    }

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

    params.pixformat = OV2640_WS63_PIXFORMAT_JPEG;
    params.framesize = ov2640_demo_framesize_from_config();
    params.jpeg_quality = CONFIG_OV2640_WS63_JPEG_QUALITY;
    params.pclk_div = CONFIG_OV2640_WS63_PCLK_DIV;

    ret = ov2640_ws63_get_frame_info(params.framesize, &frame_info);
    if (ret == ERRCODE_SUCC) {
        osal_printk("[ov2640] jpeg %ux%u quality=%u uart=%u\r\n",
            frame_info.width, frame_info.height, params.jpeg_quality, CONFIG_OV2640_WS63_UART_BAUDRATE);
    }

    ret = ov2640_ws63_configure(&cam, &params);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[ov2640] configure fail: 0x%x\r\n", ret);
        return NULL;
    }

    while (1) {
        ret = ov2640_ws63_capture_frame(&cam, g_frame_buf, sizeof(g_frame_buf),
            &frame_len, CONFIG_OV2640_WS63_TIMEOUT_MS);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[ov2640] capture fail: 0x%x\r\n", ret);
            osal_msleep(100);
            continue;
        }
        ret = ov2640_ws63_find_jpeg(g_frame_buf, frame_len, &jpeg_offset, &jpeg_len);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[ov2640] no jpeg in %u bytes\r\n", (unsigned int)frame_len);
            continue;
        }
        ret = xcam_uart_write_all(g_frame_buf + jpeg_offset, jpeg_len);
        if (ret != ERRCODE_SUCC) {
            osal_printk("[ov2640] uart send fail: 0x%x\r\n", ret);
        }
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
