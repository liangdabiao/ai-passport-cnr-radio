// main/sq_wifi.cc —— 见 sq_wifi.h。esp-wifi-connect 组件的 C 封装。
// 全部 C++ 细节（WifiManager 单例、事件回调、配网热点）住在这里。
#include "sq_wifi.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "ssid_manager.h"
#include "wifi_manager.h"

static const char *TAG = "sq_wifi";

static void (*s_on_connected)(void);
static void (*s_on_disconnected)(void);

void sq_wifi_start(void (*on_connected)(void), void (*on_disconnected)(void))
{
    s_on_connected = on_connected;
    s_on_disconnected = on_disconnected;

    auto &wifi = WifiManager::GetInstance();
    WifiManagerConfig config;
    config.ssid_prefix = "AStock";
    config.fixed_ap_ssid = "AStock";
    config.language = "zh-CN";
    config.station_hostname = "astock-quote";
    if (!wifi.Initialize(config)) {
        ESP_LOGW(TAG, "WiFi 组件初始化失败");
        return;
    }
    wifi.SetEventCallback([](WifiEvent event, const std::string &data) {
        ESP_LOGI(TAG, "WiFi 事件 %d: %s", (int)event, data.c_str());
        if (event == WifiEvent::Connected) {
            if (s_on_connected) s_on_connected();
        } else if (event == WifiEvent::Disconnected) {
            if (s_on_disconnected) s_on_disconnected();
        }
    });

    auto &ssid_manager = SsidManager::GetInstance();
    if (ssid_manager.GetSsidList().empty()) {
        wifi.StartConfigAp(); // 新设备：直接进配网
    } else {
        wifi.StartStation();  // 老设备：自动连
    }
}

void sq_wifi_open_config_ap(void)
{
    WifiManager::GetInstance().StartConfigAp();
}

void sq_wifi_reset_credentials(void)
{
    auto &ssid_manager = SsidManager::GetInstance();
    auto list = ssid_manager.GetSsidList();
    for (int i = (int)list.size() - 1; i >= 0; i--) {
        ssid_manager.RemoveSsid(i); // 只清凭据，不碰应用的其他 NVS
    }
    WifiManager::GetInstance().StartConfigAp();
}

bool sq_wifi_is_connected(void)
{
    return WifiManager::GetInstance().IsConnected();
}

void sq_wifi_ensure_dns(void)
{
    // hot-daily 真机教训：DHCP 拿到的 DNS 会让首次 HTTP 请求超时。
    // 显式把主 DNS 设为网关、备用设为公共 DNS，双保险。
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return;

    esp_netif_dns_info_t dns1 = {};
    dns1.ip.u_addr.ip4.addr = ipaddr_addr("192.168.1.1");
    dns1.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns1);

    esp_netif_dns_info_t dns2 = {};
    dns2.ip.u_addr.ip4.addr = ipaddr_addr("223.5.5.5"); // 阿里公共 DNS
    dns2.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns2);

    ESP_LOGI(TAG, "DNS 已设置：主=192.168.1.1 备=223.5.5.5");
}
