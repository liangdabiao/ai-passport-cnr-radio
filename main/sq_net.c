// main/sq_net.c —— 见 sq_net.h。
//
// HTTP 用 raw socket（hot-daily 真机教训：esp_http_client 在 C3+lwip 上
// TCP 都通但组件内部失败；socket 直连验证过完全正常）。东财接口是纯 HTTP，
// 不碰 TLS —— 省 ~45KB 堆与全部证书负担。
#include "sq_net.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include "sq_market.h"
#include "sq_store.h"
#include "sq_wifi.h"

static const char *TAG = "sq_net";

#define SQ_NET_STACK      16384
#define SQ_NET_PRIO       4
#define SQ_BODY_MAX       8192
#define SQ_HOST           "push2.eastmoney.com"
#define SQ_PORT           80
#define SQ_URL_HEAD       "/api/qt/ulist.np/get?fltt=2&invt=2&fields=f2,f3,f4,f12,f13,f14,f15,f16,f17,f18,f124&secids="

static TaskHandle_t s_task;
static volatile bool s_refresh_requested;
static volatile bool s_last_ok;
static sq_watchlist_t s_watch;              // 自选副本（net 任务私有）
static void (*s_on_result)(bool ok, const sq_quote_batch_t *, void *);
static void *s_user;
static bool s_sntp_started;

// ---------------------------------------------------------------- HTTP ----
// raw socket GET，返回 malloc 的 body（调用方 free），失败返回 NULL。
static char *http_get(const char *host, int port, const char *path)
{
    const struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) {
        ESP_LOGW(TAG, "DNS 解析失败：%s", host);
        return NULL;
    }
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    struct timeval tv = { .tv_sec = 8, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
        .sin_addr = *(struct in_addr *)he->h_addr_list[0],
    };
    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGW(TAG, "TCP 连接失败 %s:%d", host, port);
        close(sock);
        return NULL;
    }

    char request[512];
    const int req_len = snprintf(request, sizeof(request),
                                 "GET %s HTTP/1.0\r\n"
                                 "Host: %s\r\n"
                                 "User-Agent: astock-quote/1.0\r\n"
                                 "Accept: */*\r\n"
                                 "Connection: close\r\n\r\n",
                                 path, host);
    if (req_len <= 0 || write(sock, request, (size_t)req_len) < 0) {
        close(sock);
        return NULL;
    }

    char *body = (char *)malloc(SQ_BODY_MAX);
    if (!body) {
        close(sock);
        return NULL;
    }
    size_t received = 0;
    char chunk[1024];
    for (;;) {
        const int n = read(sock, chunk, sizeof(chunk));
        if (n <= 0) break;
        // 先整段缓存原始流，读完后再找 "\r\n\r\n" 切 body。
        if (received + (size_t)n < SQ_BODY_MAX) {
            memcpy(body + received, chunk, (size_t)n);
            received += (size_t)n;
        } else {
            break; // 超过容量：截断（行情 JSON 远小于 8KB）。
        }
    }
    close(sock);

    // 切出 body。
    char *payload = strstr(body, "\r\n\r\n");
    if (!payload) {
        free(body);
        return NULL;
    }
    payload += 4;
    // 原地把 payload 前移（长度小于原串，安全）。
    memmove(body, payload, received - (size_t)(payload - body));
    body[received - (size_t)(payload - body)] = '\0';
    return body;
}

// -------------------------------------------------------------- 快照存取 ----
// ---------------------------------------------------------------- 任务 ----
static void start_sntp_once(void)
{
    if (s_sntp_started) return;
    s_sntp_started = true;
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_init();
    // 北京时间。
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "SNTP 已启动（阿里 / cn.pool）");
}

static void net_task(void *arg)
{
    (void)arg;
    char secids[SQ_WATCH_CAP * SQ_SECID_MAX];
    char url[256];

    for (;;) {
        if (!s_refresh_requested) {
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }
        s_refresh_requested = false;

        if (!sq_wifi_is_connected()) {
            ESP_LOGW(TAG, "未联网，刷新放弃");
            continue;
        }
        sq_wifi_ensure_dns();
        start_sntp_once();

        // 自选 → secids。
        unsigned joined = 0;
        secids[0] = '\0';
        for (uint8_t i = 0; i < s_watch.count; i++) {
            char one[SQ_SECID_MAX];
            if (!sq_watch_secid(&s_watch, i, one, sizeof(one))) continue;
            if (joined > 0) secids[joined++] = ',';
            const size_t len = strlen(one);
            if (joined + len + 1 > sizeof(secids)) break;
            memcpy(secids + joined, one, len + 1);
            joined += len;
        }
        if (joined == 0) {
            ESP_LOGW(TAG, "自选列表为空，刷新放弃");
            continue;
        }
        snprintf(url, sizeof(url), "%s%s", SQ_URL_HEAD, secids);

        ESP_LOGI(TAG, "拉取行情：%s", url);
        char *body = http_get(SQ_HOST, SQ_PORT, url);
        if (!body) {
            ESP_LOGW(TAG, "拉取失败");
            s_last_ok = false;
            if (s_on_result) s_on_result(false, NULL, s_user);
            continue;
        }

        sq_quote_batch_t batch;
        const bool parsed = sq_quote_parse(body, &batch);
        free(body);
        if (!parsed) {
            ESP_LOGW(TAG, "解析失败");
            s_last_ok = false;
            if (s_on_result) s_on_result(false, NULL, s_user);
            continue;
        }

        // 缓存 NVS（断网兜底）。
        uint8_t blob[sq_quote_serialize_size()];
        if (sq_quote_serialize(&batch, blob, sizeof(blob))) {
            (void)sq_store_save_cache(blob, sizeof(blob));
        }

        s_last_ok = true;
        ESP_LOGI(TAG, "行情到手：%u 条，epoch=%u", batch.count, batch.epoch);
        if (s_on_result) s_on_result(true, &batch, s_user);
    }
}

void sq_net_start(void)
{
    if (s_task) return;
    sq_watch_reset(&s_watch); // 占位：app 随后用 set_watchlist 覆盖。
    if (xTaskCreate(net_task, "sq_net", SQ_NET_STACK, NULL, SQ_NET_PRIO, &s_task) !=
        pdPASS) {
        ESP_LOGE(TAG, "网络任务创建失败");
    }
}

void sq_net_set_callback(void (*cb)(bool ok, const sq_quote_batch_t *, void *),
                         void *user)
{
    s_on_result = cb;
    s_user = user;
}

void sq_net_set_watchlist(const sq_watchlist_t *list)
{
    if (!list) return;
    s_watch = *list;
}

void sq_net_request_refresh(void)
{
    s_refresh_requested = true;
}

bool sq_net_last_ok(void)
{
    return s_last_ok;
}
