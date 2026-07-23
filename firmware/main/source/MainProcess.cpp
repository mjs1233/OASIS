#include "../include/MainProcess.hpp"
//
// Created by tgian on 26. 7. 23..
//
namespace oasis {
    MainProcess::MainProcess() {
    }

    MainProcess::~MainProcess() {
    }

    bool MainProcess::create() {
        return create_impl();
    }

    void MainProcess::run(void* pvParameter) {
        auto pMainProcess = static_cast<MainProcess*>(pvParameter);
        pMainProcess->init_impl();
        pMainProcess->update_impl();
        vTaskDelete(NULL);
    }

    bool MainProcess::create_impl() {
        xTaskCreatePinnedToCore(
                   MainProcess::run,
                   "main process",
                   MainProcess::STACK_SIZE,
                   static_cast<void*>(this),
                   MainProcess::PRIORITY | portPRIVILEGE_BIT,
                   &m_task_handle,
                   MainProcess::CORE
                   );
        return true;
    }

    void MainProcess::init_impl() {
        //startup seq.
        //1. init Peripheral
        //init ADC
        m_adc_unit_0.add_channel(ADC_TEMP_0_CHANNEL_NUM);
        m_adc_unit_0.add_channel(ADC_TEMP_1_CHANNEL_NUM);
        m_adc_unit_0.add_channel(ADC_BATTERY_CHANNEL_NUM);

        //2. read ROM
    }

    void MainProcess::update_impl() {
    }
}
