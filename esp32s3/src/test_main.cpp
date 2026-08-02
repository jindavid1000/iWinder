//============================================================================
//  test_main.cpp — 硬件自检程序（7 项测试）
//
//  烧录: pio run -e test -t upload
//  每个测试之间等待串口输入回车继续。
//============================================================================
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <WiFi.h>

#define PIN_LED          48
#define PIN_MOTOR_PWM    4
#define PIN_SERVO_PWM    5
#define PIN_ENDSTOP      6
#define PIN_HALL_IDLER   7
#define PIN_HALL_SPOOL   8

#define BLE_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define BLE_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

Adafruit_NeoPixel led(1, PIN_LED, NEO_GRB + NEO_KHZ800);

void ledShow(uint8_t r, uint8_t g, uint8_t b) { led.setPixelColor(0, led.Color(r, g, b)); led.show(); }
void ledOff() { ledShow(0, 0, 0); }

volatile int idlerCount = 0, spoolCount = 0;
volatile unsigned long lastIdlerUs = 0, lastSpoolUs = 0;

void IRAM_ATTR isrIdler() { unsigned long n = micros(); if (n - lastIdlerUs < 5000) return; lastIdlerUs = n; idlerCount++; }
void IRAM_ATTR isrSpool() { unsigned long n = micros(); if (n - lastSpoolUs < 5000) return; lastSpoolUs = n; spoolCount++; }

void waitEnter(const char* msg) {
    Serial.println(msg);
    Serial.println(">>> 按回车继续 <<<");
    ledShow(0, 50, 0);
    while (true) { if (Serial.available() && Serial.read() == '\n') break; delay(10); }
    ledOff();
}

void setupLEDC(uint8_t pin, uint8_t ch, uint32_t freq, uint8_t res) {
    ledcSetup(ch, freq, res);
    ledcAttachPin(pin, ch);
}

void log(const char* m) { Serial.print("[TEST] "); Serial.println(m); }

// === 1. LED ===
void testLED() {
    log("=== 1/7 LED 测试 ===");
    ledShow(255,255,255); delay(400);
    ledShow(255,0,0);     delay(400);
    ledShow(0,255,0);     delay(400);
    ledShow(0,0,255);     delay(400);
    ledOff();
}

// === 2. WiFi AP 射频测试 ===
void testWiFiAP() {
    log("=== 2/7 WiFi AP 射频测试 ===");
    log("手机 WiFi 设置中搜索 'ESP-Winder-Test'");
    log("能搜到 = 射频正常; 搜不到 = 射频问题");
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP("ESP-Winder-Test");
    if (ok) {
        Serial.printf("  AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        log("15 秒内用手机查看...");
        ledShow(0, 80, 80);
        delay(15000);
        WiFi.softAPdisconnect(true);
    } else {
        log("[FAIL] WiFi AP 启动失败");
        ledShow(255, 0, 0); delay(3000);
    }
    ledOff();
    WiFi.mode(WIFI_OFF);
}

// === 3. BLE 广播测试 ===
BLECharacteristic* testTxChar = nullptr;
bool testBleConnected = false;

class TestBleCb : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        testBleConnected = true;
        ledShow(0, 255, 255);
        if (testTxChar) { testTxChar->setValue("{\"type\":\"test\",\"msg\":\"BLE OK\"}\n"); testTxChar->notify(); }
    }
    void onDisconnect(BLEServer* s) override { testBleConnected = false; s->getAdvertising()->start(); }
};
class TestRxCb : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* chr) override { Serial.print("[BLE RX] "); Serial.println(chr->getValue().c_str()); }
};

void testBLE() {
    log("=== 3/7 BLE 广播测试 ===");
    BLEDevice::init("ESP-Winder");
    BLEServer* srv = BLEDevice::createServer();
    srv->setCallbacks(new TestBleCb());
    BLEService* svc = srv->createService(BLE_SERVICE_UUID);
    BLECharacteristic* rx = svc->createCharacteristic(BLE_RX_UUID, BLECharacteristic::PROPERTY_WRITE);
    rx->setCallbacks(new TestRxCb());
    testTxChar = svc->createCharacteristic(BLE_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    testTxChar->addDescriptor(new BLE2902());
    svc->start();

    BLEAdvertising* adv = srv->getAdvertising();
    BLEAdvertisementData advData;
    advData.setName("ESP-Winder");
    adv->setAdvertisementData(advData);
    BLEAdvertisementData scanResp;
    scanResp.setName("ESP-Winder");
    scanResp.setCompleteServices(BLEUUID(BLE_SERVICE_UUID));
    adv->setScanResponseData(scanResp);
    adv->start();

    Serial.printf("[BLE] MAC: %s\n", BLEDevice::getAddress().toString().c_str());
    log("30 秒等待连接...");

    unsigned long start = millis();
    while (millis() - start < 30000) {
        if (testBleConnected) break;
        ledShow(0, 0, 50); delay(500);
        ledOff();           delay(500);
    }
    if (testBleConnected) log("[PASS] BLE 连接成功");
    else log("[INFO] 30 秒无连接");
    delay(1000);
    ledOff();
}

// === 4. 霍尔 ===
void testHall() {
    log("=== 4/7 霍尔传感器测试 ===");
    pinMode(PIN_HALL_IDLER, INPUT_PULLUP);
    pinMode(PIN_HALL_SPOOL, INPUT_PULLUP);
    attachInterrupt(PIN_HALL_IDLER, isrIdler, FALLING);
    attachInterrupt(PIN_HALL_SPOOL, isrSpool, FALLING);
    idlerCount = 0; spoolCount = 0;
    log("转动磁铁，15 秒...");
    unsigned long start = millis();
    int li = 0, ls = 0;
    while (millis() - start < 15000) {
        if (idlerCount != li) { Serial.printf("  [霍尔A] %d\n", idlerCount); li = idlerCount; ledShow(0,100,0); delay(50); ledOff(); }
        if (spoolCount != ls) { Serial.printf("  [霍尔B] %d\n", spoolCount); ls = spoolCount; ledShow(0,0,100); delay(50); ledOff(); }
        delay(10);
    }
    detachInterrupt(PIN_HALL_IDLER);
    detachInterrupt(PIN_HALL_SPOOL);
    Serial.printf("[结果] 从动轮: %d, 料盘: %d\n", idlerCount, spoolCount);
}

// === 5. Endstop ===
void testEndstop() {
    log("=== 5/7 Endstop 微动测试 ===");
    pinMode(PIN_ENDSTOP, INPUT_PULLUP);
    log("触碰开关，LED 变绿，15 秒...");
    unsigned long start = millis();
    bool triggered = false, last = false;
    while (millis() - start < 15000) {
        bool pressed = (digitalRead(PIN_ENDSTOP) == LOW);
        if (pressed != last) { Serial.printf("  Endstop: %s\n", pressed ? "ON" : "off"); last = pressed; if (pressed) triggered = true; }
        ledShow(0, pressed ? 255 : 0, 0);
        delay(10);
    }
    ledOff();
    if (triggered) log("[PASS] Endstop 正常"); else log("[WARN] 未触发");
}

// === 6. 舵机 ===
void testServo() {
    log("=== 6/7 舵机正反转测试 ===");
    uint8_t ch = 1;
    setupLEDC(PIN_SERVO_PWM, ch, 50, 16);
    auto wp = [&](uint16_t us) { ledcWrite(ch, (uint32_t)((float)us / 20000.0f * 65535)); };
    log("停止 1500us"); wp(1500); ledShow(0,0,50); delay(2000);
    log("左行 1000us 3秒"); wp(1000); ledShow(0,50,50); delay(3000);
    log("停止"); wp(1500); ledShow(0,0,50); delay(1000);
    log("右行 2000us 3秒"); wp(2000); ledShow(50,0,50); delay(3000);
    log("停止"); wp(1500); delay(1000);
    ledOff();
}

// === 7. 电机 ===
void testMotor() {
    log("=== 7/7 电机测试 ===");
    uint8_t ch = 0;
    setupLEDC(PIN_MOTOR_PWM, ch, 1000, 10);
    log("软启动渐快..."); ledShow(50,50,0);
    for (int d = 0; d <= 512; d += 10) { ledcWrite(ch, d); delay(20); }
    log("50% 运行 3秒"); delay(3000);
    log("渐慢停止");
    for (int d = 512; d >= 0; d -= 10) { ledcWrite(ch, d); delay(20); }
    ledcWrite(ch, 0); ledOff();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    led.begin(); led.setBrightness(40); ledShow(0,0,0);
    Serial.println("\n============================");
    Serial.println("  ESP32-S3 绕线器硬件自检 (7项)");
    Serial.println("============================\n");

    waitEnter("准备开始，连好线后按回车");
    testLED();      waitEnter("LED 完成，按回车");
    testWiFiAP();   waitEnter("WiFi AP 射频测试完成，按回车");
    testBLE();      waitEnter("BLE 完成，按回车");
    testHall();     waitEnter("霍尔 完成，按回车");
    testEndstop();  waitEnter("Endstop 完成，按回车");
    testServo();    waitEnter("舵机 完成，按回车");
    testMotor();

    Serial.println("\n============================");
    Serial.println("  全部测试完成!");
    Serial.println("============================\n");
    while (true) { ledShow(0,255,0); delay(500); ledOff(); delay(500); }
}

void loop() {}
