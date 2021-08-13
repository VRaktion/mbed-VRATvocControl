#include "VRATvocControl.h"

VRATvocControl::VRATvocControl(
    UUID *p_uuid, EventQueue *p_eq, StateChain *p_stateChain, I2C *p_i2c, PinName tvocInt, PinName tvocReset) : BLEService("tvocCtrl", p_uuid, p_eq, p_stateChain),
                                                                                                                eq(p_eq),
                                                                                                                i2c(p_i2c)
{

    this->tvoc = new ZMOD4410(this->i2c, tvocReset);
    if (tvocInt != NC)
    {
        this->tvocInt = new InterruptIn(tvocInt);
        this->tvocInt->fall(callback(this, &VRATvocControl::tvocISR));
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
    this->addCharacteristic(
        new BLENotifyCharacteristic(
            (uint16_t)VRATvocControl::Characteristics::TvocResults,
            16, //size
            this->eq,
            this->defaultInterval,
            this->minInterval,
            this->maxInterval,
            callback(this, &VRATvocControl::startTvocMeas)));
}

void VRATvocControl::pastBleInit()
{
    printf("[tvocCtrl] pastBleInit\r\n");
    float tvocRes[4]{0};

    this->setGatt(
        (uint16_t)VRATvocControl::Characteristics::TvocResults,
        tvocRes, 4);
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
    this->tvoc->startMeasureInt();
}

void VRATvocControl::tvocISR()
{
    this->eq->call(printf, "[tvocCtrl] tvoc ISR\r\n");
    this->eq->call(callback(this, &VRATvocControl::getTvocMeas));
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
            tvocRes, 4);
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