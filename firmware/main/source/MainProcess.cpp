#include "../include/MainProcess.hpp"
#include "InitialConfigData.hpp"
#include <atomic>
#include "../include/NTC.hpp"  // 상대 경로로 강제 지정

namespace oasis {
    MainProcess::MainProcess() {
    }

    MainProcess::~MainProcess() {
        if (m_task_handle != nullptr)
            xTimerDelete(m_extern_cond_timer, portMAX_DELAY);
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
        BaseType_t ret =
            xTaskCreatePinnedToCore(
                   MainProcess::run,
                   "main process",
                   MainProcess::STACK_SIZE,
                   static_cast<void*>(this),
                   MainProcess::PRIORITY | portPRIVILEGE_BIT,
                   &m_task_handle,
                   MainProcess::CORE
                   );

        if (ret != pdPASS) {
            return false;
        }
        return true;
    }

    void MainProcess::extern_condition_timer_callback(TimerHandle_t xtimer) {
        auto* self = static_cast<MainProcess*>(pvTimerGetTimerID(xtimer));
        xTaskNotify(self->m_task_handle, notify::TIMER_EXTREN_COND_CYCLE, eSetBits);
    }

    void MainProcess::init_impl() {
        //startup seq.
        //1. init Peripheral
        //init ADC
        m_adc_unit_0.add_channel(ADC_TEMP_0_CHANNEL_NUM);
        m_adc_unit_0.add_channel(ADC_TEMP_1_CHANNEL_NUM);
        m_adc_unit_0.add_channel(ADC_BATTERY_CHANNEL_NUM);

        //2. read ROM

        //3. sync with network core & recv config
        uint32_t notification_value = 0;
        if (xTaskNotifyWait(0x0, 0xFFFFFFFF,&notification_value, portMAX_DELAY) == true) {
            std::atomic_thread_fence(std::memory_order_acquire);
            //read config data
            printf("core 1 notify recv. init done\n");
        }else {
            //DO Restart Stuff.
            printf("init fail?\n");
        }

        //start network service

        //start Timer
        m_extern_cond_timer = xTimerCreate(
            "extern condition timer",
            pdMS_TO_TICKS(5000),
            pdTRUE,
            this,
            extern_condition_timer_callback
            );
        xTimerStart(m_extern_cond_timer, 0);

        //start interrupt

    } // <--- 아까 이 닫는 괄호가 지워졌었습니다!

    // [추가 및 수정된 부분] 제대로 된 위치의 update_impl
    void MainProcess::update_impl() {
        // 1. NTC 센서 설정 및 객체 생성
   // 구조체 선언 순서와 정확히 일치시킨 초기화 코드
    oasis::NTCConfig ntc_cfg {
        .v_cc_mv     = 3300.0f,
        .r_fixed_ohm = 10000.0f,
        .r_0_ohm     = 10000.0f,
        .t_0_kelvin  = 298.15f,
        .beta        = 3950.0f
    };
        
        // 기존 멤버 변수인 m_adc_unit_0과 매크로를 사용하여 NTC 센서 주입
       oasis:: NTCSensor ntc_sensor(m_adc_unit_0, ADC_TEMP_0_CHANNEL_NUM, ntc_cfg);
        ntc_sensor.init();

        printf("Starting Main Loop...\n");

        // 2. 메인 이벤트 루프
        while (true) {
            uint32_t notify_value = 0;
            
            // 다른 태스크나 타이머에서 알림이 올 때까지 무한 대기 (CPU 점유율 안 먹음)
            if (xTaskNotifyWait(0x0, 0xFFFFFFFF, &notify_value, portMAX_DELAY) == pdTRUE) {
                
                // 5초마다 울리는 extern_condition_timer_callback의 알림인지 확인
                if (notify_value & notify::TIMER_EXTREN_COND_CYCLE) {
                    // NTC 온도 읽기 및 출력
                    float current_temp = ntc_sensor.read();
                    printf("[Timer 5s] Current NTC Temp (Ch 0): %.2f C\n", current_temp);
                }
            }
        }
    }

} // namespace oasis 닫기
