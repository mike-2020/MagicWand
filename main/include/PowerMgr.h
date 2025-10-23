#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_adc/adc_oneshot.h"

class PowerMgr{
private:
    TimerHandle_t m_hTimer;
    int m_nPowerPercentage;
    uint32_t m_nTimerRunCount;
    int64_t m_nLastInputActivityTime;
    bool m_bEnableNightLight;
    adc_oneshot_unit_handle_t m_adc1Handle;

    static void vTimerCallback( void *ctx );
    int getPowerPercentage();
    void tryDeepSleep(bool bForceSleep=false);
    void initADC1();
    int readBatteryVoltage();
    void runAfterWakeUp();
    void runBeforeDeepSleep(bool bEnableULP);
public:
    int init();
    int stop();
    int getPower() { return m_nPowerPercentage; };
    void updateInputActivityTime();
    int64_t getLastInputActivityTime() {return m_nLastInputActivityTime; };
    adc_oneshot_unit_handle_t* getADC1HandlePtr(){ return &m_adc1Handle;};
    void stopADC1();
public:
    static PowerMgr* getInstance();
    void enterSleep(bool bEnableULP = false);
    void enableNightLight() { m_bEnableNightLight = true; };
    void disableNightLight() { m_bEnableNightLight = false; };
    static void powerOff5V();
    static void disablePA();
    static void powerOn5V();
    static void enablePA();
    static void enableBatteryVoltageADC();
    static void disableBatteryVoltageADC();
    static void enableLEDStrip();
    static void disableLEDStrip();
    static void turnOnLEDLight();
    static void turnOffLEDLight();
    static void turnOnIRMPWR();
    static void turnOffIRMPWR();
    static void turnOnLDR();
    static void turnOffLDR();
    static void turnOnLCDPWR();
    static void turnOffLCDPWR();
    static void turnOnLCDBL();
    static void turnOffLCDBL();
};

