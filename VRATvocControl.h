#ifndef VRA_TVOC_CONTROL_H
#define VRA_TVOC_CONTROL_H

#include "BLEService.h"

// #include "VRASettings.h"
// #include "VRAStorage.h"
#include "BLENotifyCharacteristic.h"

#include "ZMOD4410.h"

class VRATvocControl : public BLEService
{
public:
    enum class Characteristics: uint16_t
    {
        TvocResults = 0xFF00,
        TvocClean = 0xFF01
    };

    VRATvocControl(UUID *p_uuid, EventQueue *p_eq, StateChain *p_stateChain, I2C *p_i2c, PinName tvocInt = NC, PinName tvocReset = NC);

    void init();

    void initCharacteristics();
    void pastBleInit();

    float getTvoc();

    bool isMeassureBlocked();

private:
    void initTvoc();

    void onStateOff();
    void onStateStandby();
    void onStateOn();

    void startTvocMeas();
    void getTvocMeas();
    void tvocFallISR();

    void tvocRiseISR();

    void cleanWriteCb();
    void cleanSensor();

    void releaseBlockMeassure();

    EventQueue *eq;
    IntervalEvent *interval;
    // VRASettings *settings;
    // VRAStorage *storage;

    I2C *i2c;
    InterruptIn *tvocInt;
    PinName tvocReset;

    volatile bool blockMeassure {false};

    ZMOD4410 *tvoc;

    static constexpr int defaultInterval{3000};
    static constexpr int minInterval{3000};
    static constexpr int maxInterval{600000};

    BLECharacteristic *cleanCharacteristic;

    iaq_2nd_gen_results_t tvocResStruct;

    bool meassuring = false;
};

#endif //VRA_TVOC_CONTROL_H