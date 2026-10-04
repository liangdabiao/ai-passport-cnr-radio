// main/sq_store.c —— 见 sq_store.h。
#include "sq_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "sq_watch.h"

static const char *TAG = "sq_store";

// 命名空间与家族里其它应用刻意不同名：同机刷过别的应用时互不覆盖。
static const char *NAMESPACE = "astock";
static const char *KEY_WATCH = "watch";
static const char *KEY_REFRESH = "refresh";
static const char *KEY_CACHE = "qcache";

#define CACHE_MAX 2048

esp_err_t sq_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要重建（%s），擦除后重试", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err != ESP_OK) return err;
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t sq_store_load_watch(sq_watchlist_t *list)
{
    if (!list) return ESP_ERR_INVALID_ARG;
    sq_watch_reset(list); // 出厂默认：上证指数 + 深证成指。

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    uint8_t blob[SQ_WATCH_BLOB_SIZE];
    size_t size = sizeof(blob);
    err = nvs_get_blob(handle, KEY_WATCH, blob, &size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "首次开机，自选从出厂默认开始");
        return ESP_OK;
    }
    if (err != ESP_OK) return err;

    if (!sq_watch_deserialize(list, blob, (unsigned)size)) {
        ESP_LOGW(TAG, "自选名单不可用（%u 字节），回到出厂默认", (unsigned)size);
        sq_watch_reset(list);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t sq_store_save_watch(const sq_watchlist_t *list)
{
    if (!list) return ESP_ERR_INVALID_ARG;

    uint8_t blob[SQ_WATCH_BLOB_SIZE];
    if (!sq_watch_serialize(list, blob, sizeof(blob))) return ESP_ERR_INVALID_SIZE;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, KEY_WATCH, blob, sizeof(blob));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) ESP_LOGW(TAG, "自选写入失败：%s", esp_err_to_name(err));
    return err;
}

esp_err_t sq_store_load_refresh(uint16_t *seconds)
{
    if (!seconds) return ESP_ERR_INVALID_ARG;
    *seconds = 30;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    uint16_t value = 30;
    err = nvs_get_u16(handle, KEY_REFRESH, &value);
    nvs_close(handle);
    if (err != ESP_OK) return err;

    // 合法档位外的值回默认。
    if (value != 15 && value != 30 && value != 60) value = 30;
    *seconds = value;
    return ESP_OK;
}

esp_err_t sq_store_save_refresh(uint16_t seconds)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_u16(handle, KEY_REFRESH, seconds);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t sq_store_load_cache(uint8_t *blob, unsigned *size, unsigned capacity)
{
    if (!blob || !size) return ESP_ERR_INVALID_ARG;
    *size = 0;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t read = capacity;
    err = nvs_get_blob(handle, KEY_CACHE, blob, &read);
    nvs_close(handle);
    if (err != ESP_OK) return err;

    *size = (unsigned)read;
    return ESP_OK;
}

esp_err_t sq_store_save_cache(const uint8_t *blob, unsigned size)
{
    if (!blob || size == 0 || size > CACHE_MAX) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, KEY_CACHE, blob, size);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
