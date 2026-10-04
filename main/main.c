// main/main.c —— A股大行情的入口。
//
// 与家族离线应用的差异只有两点：多了 WiFi（esp-wifi-connect 组件，全权
// 管理凭据/重连/配网热点）与网络任务（raw socket 行情拉取）。LVGL 锁、
// 按键队列、栈深预算全部沿用家族已验证的模式。
//
// 三键语义（按下即响应，无双击）：
//   上/下    循环移动（菜单 / 自选行 / 数字轮盘位……见各页）
//   确定     进入 / 确认
//   长按     各页自定义（列表=刷新，列表长按下=删自选，添加=回退/取消，
//            配网=重配，标题=无）
//
// 任务清单：main_task（初始化）、LVGL 任务（BSP）、输入任务（24KB，按键
// 分发）、网络任务（16KB，行情拉取）。BLE 不使用（配网走热点），不初始化。
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "sq_app.h"
#include "sq_net.h"
#include "sq_sfx.h"
#include "sq_store.h"
#include "sq_wifi.h"

static const char *TAG = "astock";

#define INPUT_QUEUE_DEPTH 8
#define INPUT_TASK_STACK  24576
#define INPUT_TASK_PRIO   5

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static TaskHandle_t s_input_task;
static volatile bool s_input_ready;

// 按键回调运行在共享的 esp_timer 任务上：只做一次非阻塞入队，立刻返回。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

static bool translate(const input_event_t *input, sq_key_t *out)
{
    // 按下即响应（PRESS）：无单击/双击编组，快速连按永远等价于连续操作
    // （家族跳棋真机教训：双击编组会吃掉「选子→落子」这类快节奏连按）。
    // 长按映射为各页的自定义动作。
    if (input->event == BSP_BTN_PRESS) {
        switch (input->btn) {
            case BSP_BTN_UP:   *out = SQ_KEY_UP;   return true;
            case BSP_BTN_DOWN: *out = SQ_KEY_DOWN; return true;
            case BSP_BTN_OK:   *out = SQ_KEY_OK;   return true;
            default:           return false;
        }
    }
    if (input->event == BSP_BTN_LONG) {
        switch (input->btn) {
            case BSP_BTN_OK:   *out = SQ_KEY_REFRESH; return true;
            case BSP_BTN_UP:   *out = SQ_KEY_DETAIL;  return true;
            case BSP_BTN_DOWN: *out = SQ_KEY_REMOVE;  return true;
            default:           return false;
        }
    }
    return false; // CLICK/DOUBLE 忽略。
}

static void input_task(void *argument)
{
    (void)argument;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) != pdTRUE) continue;
        sq_key_t key;
        if (translate(&input, &key)) sq_app_key(key);
    }
}

static esp_err_t input_start(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreate(input_task, "sq_input", INPUT_TASK_STACK, NULL,
                    INPUT_TASK_PRIO, &s_input_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t err = bsp_button_init(on_key, NULL);
    if (err != ESP_OK) {
        vTaskDelete(s_input_task);
        s_input_task = NULL;
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return err;
    }
    return ESP_OK;
}

// WiFi 事件（esp-wifi-connect 上下文）：只翻状态旗，UI 各页自行轮询。
static void on_wifi_connected(void)
{
    sq_app_note_wifi(true);
}

static void on_wifi_disconnected(void)
{
    sq_app_note_wifi(false);
}

void app_main(void)
{
    ESP_LOGI(TAG, "A股大行情 启动");

    const esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒原因: %d", wakeup);
    }

    esp_err_t err = bsp_i2c_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "I2C 初始化失败: %s", esp_err_to_name(err));
    (void)bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。检查 SPI 接线"
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    err = sq_store_init();
    if (err != ESP_OK) ESP_LOGW(TAG, "存储不可用: %s", esp_err_to_name(err));

    if (bsp_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "音频编解码初始化失败，将以静音方式运行");
    } else if (sq_sfx_start() != ESP_OK) {
        ESP_LOGW(TAG, "音效服务启动失败，将以静音方式运行");
    }
    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "电量计不可用，顶栏会显示 --");
    }

    const esp_err_t input_err = input_start();
    if (input_err != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败: %s", esp_err_to_name(input_err));
    }

    // WiFi：有存档凭据自动连，无存档直接开配网热点（esp-wifi-connect 全权管理）。
    sq_wifi_start(on_wifi_connected, on_wifi_disconnected);

    // 网络任务：行情拉取与缓存。
    sq_net_start();

    if (bsp_lvgl_lock(1000)) {
        sq_app_start();
        bsp_lvgl_unlock();
        s_input_ready = true;
    }

    ESP_LOGI(TAG, "就绪:显示=OK 按键=OK");
}
