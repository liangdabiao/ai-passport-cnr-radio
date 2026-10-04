idf_component_register(
    SRCS "main.c"
         # 应用与页面
         "sq_app.c"
         "sq_page_title.c"
         "sq_page_list.c"
         "sq_page_detail.c"
         "sq_page_add.c"
         "sq_page_wifi.c"
         "sq_page_settings.c"
         "sq_ui.c"
         # 纯逻辑：与 ESP-IDF/LVGL 解耦，宿主测试直接编译这些文件
         "sq_symbol.c"
         "sq_quote.c"
         "sq_market.c"
         "sq_watch.c"
         "sq_wrap.c"
         "sq_volume.c"
         # 服务与网络
         "sq_store.c"
         "sq_sfx.c"
         "sq_net.c"
         "sq_wifi.cc"
         # 音效合成（RTTTL）。含一处上游没有的扩展：play_loop 循环播放。
         "rtttl_player.c"
    INCLUDE_DIRS "."
    # 行情工具需要 WiFi/网络栈与 esp-wifi-connect 配网组件；BLE 不初始化。
    REQUIRES bsp esp_event esp_hw_support esp_timer nvs_flash esp_wifi
             esp_netif json lwip esp-wifi-connect
)

# 中文字库由 tools/astock-quote/gen_font.py 生成到 assets/fonts/。16px 字库
# 额外收 GB2312 全部常用字（约 3755 字）—— 服务器返回的股票名是动态文本，
# 只有 UI 子集会缺字形。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sq_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sq_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/sq_font_32.c"
)
