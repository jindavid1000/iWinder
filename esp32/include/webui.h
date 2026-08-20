#ifndef WEBUI_H
#define WEBUI_H
//============================================================================
//  webui.h — ESP32 内嵌 Web 控制界面
//  手机/电脑浏览器直接访问（AP: http://192.168.4.1，STA: 设备 IP），
//  零安装零签名。API 与 TCP 协议共用同一套命令处理。
//============================================================================
#include <Arduino.h>

class WebUi {
public:
    void begin();   // WiFi 就绪后调用（AP/STA 均可服务）
    void update();  // 主循环轮询

private:
    bool _begun = false;
};

extern WebUi g_webui;

#endif // WEBUI_H
