#include "../include/MainProcess.hpp"
#include "InitialConfigData.hpp"
#include <atomic>
#include <cmath>
#include <algorithm>
#include "esp_err.h"
#include "esp_log.h"
#include "NetworkQueue.hpp"
#include "../include/NTC.hpp"

namespace oasis {

    MainProcess::MainProcess() {
    }

    MainProcess::~MainProcess() {
        if (m_extern_cond_timer != nullptr && xTimerDelete(m_extern_cond_timer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MainProcess", "environment timer deletion failed");
        }
        if (m_pulse_capture_timer != nullptr && xTimerDelete(m_pulse_capture_timer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse timer deletion failed");
        }
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

        // ADC1 channel 1 is GPIO2 on ESP32-S3. ADCUnit stores it in slot 0.
        if (m_adc_unit_0.add_oneshot_gpio(NTC_ADC_GPIO) < 0) {
            ESP_LOGE("MainProcess", "NTC ADC GPIO registration failed");
            return false;
        }
        //m_adc_unit_0.add_channel(ADC_BATTERY_CHANNEL_NUM);


        return true;

    }

    void MainProcess::extern_condition_timer_callback(TimerHandle_t xtimer) {
        auto* self = static_cast<MainProcess*>(pvTimerGetTimerID(xtimer));
        if (xTaskNotify(self->m_task_handle, notify::TIMER_EXTREN_COND_CYCLE, eSetBits) != pdPASS) {
            ESP_LOGE("MainProcess", "environment-cycle notification failed");
        }
    }

    void MainProcess::pulse_capture_timer_callback(TimerHandle_t xtimer) {
        auto* self = static_cast<MainProcess*>(pvTimerGetTimerID(xtimer));
        if (xTaskNotify(self->m_task_handle, notify::TIMER_PULSE_CAPTURE_END, eSetBits) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse-capture notification failed");
        }
    }

    void MainProcess::init_impl() {
        //startup seq.
        //1. init Peripheral

        //2. read ROM

        //3. sync with network core & recv config
        uint32_t notification_value = 0;
        if (xTaskNotifyWait(0x0, 0xFFFFFFFF,&notification_value, portMAX_DELAY) == true) {
            std::atomic_thread_fence(std::memory_order_acquire);
            //read config data
            printf("core 1 notify recv. init done\n");
        } else {
            //DO Restart Stuff.
            printf("init fail?\n");
        }

        //start network service

        const esp_err_t max_init_result = m_max30102.initialize();
        m_max30102_ready = max_init_result == ESP_OK;
        if (!m_max30102_ready) {
            ESP_LOGE("MainProcess", "MAX30102 initialization failed: %s", esp_err_to_name(max_init_result));
        }

        // One repeating 30-second environment cycle and one 10-second capture window.
        m_extern_cond_timer = xTimerCreate(
            "extern condition timer",
            pdMS_TO_TICKS(ENVIRONMENT_CYCLE_MS),
            pdTRUE,
            this,
            extern_condition_timer_callback
            );
        m_pulse_capture_timer = xTimerCreate(
            "pulse capture timer",
            pdMS_TO_TICKS(PULSE_CAPTURE_MS),
            pdFALSE,
            this,
            pulse_capture_timer_callback
            );
        configASSERT(m_extern_cond_timer != nullptr && m_pulse_capture_timer != nullptr);
        if (xTimerStart(m_extern_cond_timer, 0) != pdPASS) {
            ESP_LOGE("MainProcess", "environment timer start failed");
            return;
        }
        // Start the first measurement immediately instead of waiting 30 seconds.
        if (xTaskNotify(m_task_handle, notify::TIMER_EXTREN_COND_CYCLE, eSetBits) != pdPASS) {
            ESP_LOGE("MainProcess", "initial environment-cycle notification failed");
        }
        printf("INIT IMU\n");
        if (m_imu_process.create() == false) {
            ESP_LOGE("MainProcess", "IMU process creation failed");
        }
        //start interrupt

    }

    void MainProcess::update_impl() {

        while (true) {
            uint32_t notification_value = 0;
            if (xTaskNotifyWait(0x0, 0xFFFFFFFF,&notification_value, portMAX_DELAY) == true) {
                std::atomic_thread_fence(std::memory_order_acquire);

                if (notification_value & notify::ISR_IMU_BUFFER_FULL) {
                    //do IMU buffer flush.
                    //printf("Main Process, recv imu buffer\n");
                    imu_buffer_handle();
                    service_pulse_capture();
                    process_network_item();
                }

                if (notification_value & notify::TIMER_EXTREN_COND_CYCLE) {
                    read_extern_condition();
                    begin_pulse_capture();
                }

                if (notification_value & notify::TIMER_PULSE_CAPTURE_END) {
                    finish_pulse_capture();
                }
            }
        }

    }




    void MainProcess::imu_buffer_handle() {
        IMUProcess::IMUBuffer& imu_buffer = m_imu_process.get_buffer_pool();
        while (true) {
            IMUData* imu_data = imu_buffer.acquire_ready(0);
            if (imu_data == nullptr) {
                break;
            }
            //read other data.
            //release
            imu_buffer.release_free(imu_data);
        }
    }

    void MainProcess::process_network_item() {
        
        //send to Network Queue
    }

    void MainProcess::read_extern_condition() {
        m_last_ntc_temperature_c = m_ntc.read();
        printf("NTC : %f\n", m_last_ntc_temperature_c);
        const esp_err_t result = m_sht31.read_measurement(m_last_sht31);
        if (result == ESP_OK) {
            printf("SHT31 : %0.2f C, %0.2f %%RH\n", m_last_sht31.temperature_c,
                   m_last_sht31.humidity_percent);
        } else {
            printf("SHT31 read failed: %s\n", esp_err_to_name(result));
        }
    }

    void MainProcess::begin_pulse_capture() {
        if (!m_max30102_ready || m_pulse_capture_active) return;

        m_ppg_sample_count = 0;
        const esp_err_t result = m_max30102.start_measurement();
        if (result != ESP_OK) {
            printf("MAX30102 start failed: %s\n", esp_err_to_name(result));
            return;
        }
        m_pulse_capture_active = true;
        if (xTimerStart(m_pulse_capture_timer, 0) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse timer start failed");
            const esp_err_t stop_result = m_max30102.stop_measurement();
            if (stop_result != ESP_OK) {
                ESP_LOGE("MainProcess", "MAX30102 stop after timer failure failed: %s", esp_err_to_name(stop_result));
            }
            m_pulse_capture_active = false;
        }
    }

    void MainProcess::service_pulse_capture() {
        if (!m_pulse_capture_active) return;

        size_t received = 0;
        const esp_err_t result = m_max30102.read_fifo(
            std::span<PPGSample>(m_ppg_samples).subspan(m_ppg_sample_count), received);
        if (result != ESP_OK) {
            printf("MAX30102 FIFO read failed: %s\n", esp_err_to_name(result));
            return;
        }
        m_ppg_sample_count += received;
        if (m_ppg_sample_count == m_ppg_samples.size()) {
            // Preserve the samples already collected and stop before the local buffer overflows.
            if (xTaskNotify(m_task_handle, notify::TIMER_PULSE_CAPTURE_END, eSetBits) != pdPASS) {
                ESP_LOGE("MainProcess", "full-buffer notification failed");
            }
        }
    }

    float MainProcess::calculate_bpm() const {
        if (m_ppg_sample_count < 200) return 0.0f;

        // A lightweight time-domain estimate. FFT can replace this backend later
        // without changing capture ownership or the PPG buffer format.
        uint64_t sum = 0;
        for (size_t i = 0; i < m_ppg_sample_count; ++i) sum += m_ppg_samples[i].infrared;
        const float mean = static_cast<float>(sum) / m_ppg_sample_count;

        std::array<size_t, 32> peaks {};
        size_t peak_count = 0;
        constexpr size_t MIN_PEAK_DISTANCE_SAMPLES = 30; // 200 BPM at 100 SPS.
        for (size_t i = 1; i + 1 < m_ppg_sample_count && peak_count < peaks.size(); ++i) {
            const float value = static_cast<float>(m_ppg_samples[i].infrared);
            if (value <= mean || value < m_ppg_samples[i - 1].infrared || value <= m_ppg_samples[i + 1].infrared) continue;
            if (peak_count != 0 && i - peaks[peak_count - 1] < MIN_PEAK_DISTANCE_SAMPLES) continue;
            peaks[peak_count++] = i;
        }
        if (peak_count < 2) return 0.0f;

        size_t interval_sum = 0;
        for (size_t i = 1; i < peak_count; ++i) interval_sum += peaks[i] - peaks[i - 1];
        const float average_interval = static_cast<float>(interval_sum) / (peak_count - 1);
        return average_interval > 0.0f ? 6000.0f / average_interval : 0.0f;
    }

    void MainProcess::finish_pulse_capture() {
        if (!m_pulse_capture_active) return;
        service_pulse_capture(); // Drain samples produced since the previous IMU callback.
        const esp_err_t stop_result = m_max30102.stop_measurement();
        if (stop_result != ESP_OK) {
            ESP_LOGE("MainProcess", "MAX30102 stop failed: %s", esp_err_to_name(stop_result));
        }
        m_pulse_capture_active = false;

        const float bpm = calculate_bpm();
        printf("Pulse capture complete: %u samples, BPM: %0.1f\n",
               static_cast<unsigned>(m_ppg_sample_count), bpm);

        const auto to_u8 = [](float value) -> uint8_t {
            if (!std::isfinite(value)) return 0;
            return static_cast<uint8_t>(std::clamp(std::lround(value), 0l, 255l));
        };

        network_item::worker_data item {};
        // raw_temp uses SHT31 temperature when valid; NTC is retained for the
        // future worker-data protocol revision. Battery and other unavailable
        // calculations are explicitly encoded as zero.
        item.raw_temp = to_u8(m_last_sht31.temperature_c);
        item.raw_hum = to_u8(m_last_sht31.humidity_percent);
        item.raw_bpm = to_u8(bpm);
        item.battery_voltage = 0;

        const bool queued = NetworkQueue::instance().enqueue(item);
        ESP_LOGI("MainProcess", "worker_data enqueue=%s temp=%u hum=%u bpm=%u battery=%u ntc=%.2f",
                 queued ? "true" : "false", item.raw_temp, item.raw_hum, item.raw_bpm,
                 item.battery_voltage, m_last_ntc_temperature_c);
    }

}
