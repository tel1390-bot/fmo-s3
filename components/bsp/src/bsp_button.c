// components/bsp/src/bsp_button.c
// FMO-S3 port: 单按键 = 开发板板载 BOOT 键(GPIO0,按下为低)。
// 语义映射为"确定键"(BSP_BTN_OK):
//   短按 = 开关音频;长按 = 切换台站/子页面返回;配网页短按 = 翻页。
// 上/下键(音量调节、长按上键进网络菜单)本板无硬件,不触发。
// 若想外接三键分压电路,可把本文件换回原 ADC 版本并改 bsp_pins.h。
#include "bsp_button.h"
#include "bsp_pins.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "esp_log.h"

static const char *TAG = "bsp_btn";

static bsp_btn_cb_t  s_cb;
static void         *s_user;
static button_handle_t s_btn;

static void on_event(void *arg, void *usr_data, bsp_btn_ev_t ev)
{
    (void)arg; (void)usr_data;
    if (s_cb) s_cb(BSP_BTN_OK, ev, s_user);
}
static void cb_press (void *a, void *u) { on_event(a, u, BSP_BTN_PRESS);  }
static void cb_click (void *a, void *u) { on_event(a, u, BSP_BTN_CLICK);  }
static void cb_double(void *a, void *u) { on_event(a, u, BSP_BTN_DOUBLE); }
static void cb_long  (void *a, void *u) { on_event(a, u, BSP_BTN_LONG);   }

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user)
{
    s_cb = cb; s_user = user;

    const button_config_t bc = { .long_press_time = 700 }; // 700ms 触发长按
    const button_gpio_config_t gpio_cfg = {
        .gpio_num          = BSP_BTN_GPIO,
        .active_level      = 0,                     // BOOT 键按下为低
        .enable_power_save = true,
        .disable_pull      = false,                 // 使用内部上拉
    };
    esp_err_t e = iot_button_new_gpio_device(&bc, &gpio_cfg, &s_btn);
    if (e != ESP_OK || !s_btn) {
        ESP_LOGE(TAG, "按键创建失败 (%s) —— 检查 GPIO%d", esp_err_to_name(e), BSP_BTN_GPIO);
        return e == ESP_OK ? ESP_FAIL : e;
    }

    iot_button_register_cb(s_btn, BUTTON_PRESS_DOWN,       NULL, cb_press,  NULL);
    iot_button_register_cb(s_btn, BUTTON_SINGLE_CLICK,     NULL, cb_click,  NULL);
    iot_button_register_cb(s_btn, BUTTON_DOUBLE_CLICK,     NULL, cb_double, NULL);
    iot_button_register_cb(s_btn, BUTTON_LONG_PRESS_START, NULL, cb_long,   NULL);

    ESP_LOGI(TAG, "按键就绪:GPIO%d(单键=确定/开关音频)", BSP_BTN_GPIO);
    return ESP_OK;
}

// ADC 电压读取在单 GPIO 方案下无意义,返回 -1。
int bsp_button_read_mv(void)
{
    return -1;
}
