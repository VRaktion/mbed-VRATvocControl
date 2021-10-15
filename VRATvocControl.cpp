#include "VRATvocControl.h"

VRATvocControl::VRATvocControl(
    UUID *p_uuid, EventQueue *p_eq, StateChain *p_stateChain, I2C *p_i2c, PinName tvocInt, PinName tvocReset) : BLEService("tvocCtrl", p_uuid, p_eq, p_stateChain),
                                                                                                                eq(p_eq),
                                                                                                                i2c(p_i2c)
{
    this->interval = new IntervalEvent(this->eq, 15000, callback(this, &VRATvocControl::startTvocMeas));
    this->tvoc = new ZMOD4410(this->i2c, tvocReset);
    if (tvocInt != NC)
    {
        this->tvocInt = new InterruptIn(tvocInt);
        this->tvocInt->mode(PullDown); //PinMode::
        // this->tvocInt->rise(callback(this, &VRATvocControl::tvocRiseISR));
        this->tvocInt->fall(callback(this, &VRATvocControl::tvocFallISR));
        this->tvocInt->disable_irq();
    }
}

void VRATvocControl::init()
{
    printf("[tvocCtrl] init\r\n");
    this->initTvoc();
}

void VRATvocControl::initTvoc()
{
    printf("[tvocCtrl] init TVOC Sensor\r\n");
    this->tvoc->init();
    this->tvocInt->enable_irq();
}

void VRATvocControl::initCharacteristics()
{
    printf("[tvocCtrl] init Characteristics\r\n");
    // this->addCharacteristic(
    //     new BLENotifyCharacteristic(
    //         (uint16_t)VRATvocControl::Characteristics::TvocResults,
    //         16, //size
    //         this->eq,
    //         this->defaultInterval,
    //         this->minInterval,
    //         this->maxInterval,
    //         callback(this, &VRATvocControl::startTvocMeas)));

    this->addCharacteristic(
        new BLECharacteristic(
            (uint16_t)VRATvocControl::Characteristics::TvocResults,
            GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY,
            16));

    this->cleanCharacteristic = new BLECharacteristic(
        (uint16_t)VRATvocControl::Characteristics::TvocClean,
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE |
            GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY,
        1);

    this->addCharacteristic(this->cleanCharacteristic);

    this->cleanCharacteristic->setWriteCallback(
        new Callback<void(void)>(this, &VRATvocControl::cleanWriteCb));
}

void VRATvocControl::pastBleInit()
{
    printf("[tvocCtrl] pastBleInit\r\n");
    float tvocRes[4]{0};

    this->setGatt(
        (uint16_t)VRATvocControl::Characteristics::TvocResults,
        tvocRes, 4);

    this->interval->start();
}

void VRATvocControl::onStateOff()
{
    printf("[tvocCtrl] off\r\n");
}

void VRATvocControl::onStateStandby()
{
    printf("[tvocCtrl] standby\r\n");
}

void VRATvocControl::onStateOn()
{
    printf("[tvocCtrl] on\r\n");
}

void VRATvocControl::startTvocMeas()
{
    printf("[tvocCtrl] start tvoc meas\r\n");
    this->tvocRiseISR();
    this->tvoc->startMeasureInt();
    // this->tvoc->startMeasurePoll();
}

void VRATvocControl::tvocRiseISR() //this one is because TVOC meassurement somehow creates Spike on VCC
{
    this->eq->call(printf, "[tvocCtrl] tvoc rise ISR, block\r\n");
    this->blockMeassure = true;
}

void VRATvocControl::tvocFallISR()
{
    this->eq->call(printf, "[tvocCtrl] tvoc fall ISR\r\n");
    this->eq->call(callback(this, &VRATvocControl::getTvocMeas));
    this->eq->call_in(1200, callback(this, &VRATvocControl::releaseBlockMeassure));
}

void VRATvocControl::releaseBlockMeassure()
{
    printf("[tvocCtrl] block released\r\n");
    this->blockMeassure = false;
}

bool VRATvocControl::isMeassureBlocked()
{
    return this->blockMeassure;
}

void VRATvocControl::getTvocMeas()
{
    printf("[tvocCtrl] getTvocMeas\r\n");

    this->tvoc->readResults();
    this->tvocResStruct = this->tvoc->getResults();

    if (this->tvoc->isValid())
    {
        printf("[tvocCtrl] valid\r\n");

        float tvocRes[4]{
            this->tvocResStruct.etoh,
            this->tvocResStruct.tvoc,
            this->tvocResStruct.eco2,
            this->tvocResStruct.iaq};

        //TODO: check vor valid data
        printf("tvoc EtOH: %d TVOC: %d eCO2:%d IAQ:%d\r\n",
               (int)tvocRes[0], (int)tvocRes[1],
               (int)tvocRes[2], (int)tvocRes[3]);

        this->setGatt(
            (uint16_t)VRATvocControl::Characteristics::TvocResults,
            tvocRes, 4); //TEST
    }
    else
    {
        printf("[tvocCtrl] warmup\r\n");
    }
}

float VRATvocControl::getTvoc()
{
    if (this->tvoc->isValid())
    {
        return this->tvocResStruct.iaq;
    }
    else
    {
        return 10.0;
    }
}

void VRATvocControl::cleanWriteCb()
{
    BLE &ble = BLE::Instance(BLE::DEFAULT_INSTANCE);
    uint8_t value;
    uint16_t length = 1;
    ble_error_t err = ble.gattServer().read(
        this->cleanCharacteristic->getValueHandle(), &value, &length);

    if (value == 0x42)
    {
        this->eq->call(callback(this, &VRATvocControl::cleanSensor));
    }
}

void VRATvocControl::cleanSensor()
{
    if (this->isMeassureBlocked())
    {
        printf("[tvocCtrl] tvoc measuring, retry clean in 1 sec...\r\n");
        this->eq->call_in(1000, callback(this, &VRATvocControl::cleanSensor));
        return;
    }
    printf("[tvocCtrl] start tvoc clean\r\n");
    this->tvoc->clean();
    printf("[tvocCtrl] cleaning finished\r\n");
}
