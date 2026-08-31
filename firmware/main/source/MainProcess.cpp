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
    namespace {
#if OASIS_ENABLE_PPG_SENSOR
        void log_pulse_sensor_failure(const char* operation, esp_err_t error) {
            const char* reason = "unexpected I2C or driver error";
            switch (error) {
                case ESP_ERR_NOT_FOUND:
                    reason = "no ACK; check sensor power, SDA/SCL wiring, pull-ups, and address 0x57";
                    break;
                case ESP_ERR_TIMEOUT:
                    reason = "I2C timeout; check a stuck bus, wiring, pull-ups, or clock speed";
                    break;
                case ESP_ERR_INVALID_RESPONSE:
                    reason = "a device answered at 0x57, but its MAX30102 identity was invalid";
                    break;
                case ESP_ERR_INVALID_STATE:
                    reason = "I2C driver or bus is not in a usable state";
                    break;
                default:
                    break;
            }
            ESP_LOGE("MainProcess", "MAX30102 %s failed: %s (0x%x); reason: %s",
                     operation, esp_err_to_name(error), static_cast<unsigned>(error), reason);
        }
#endif
    }

    MainProcess::MainProcess() {
    }

    MainProcess::~MainProcess() {
        if (m_extern_cond_timer != nullptr && xTimerDelete(m_extern_cond_timer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MainProcess", "environment timer deletion failed");
        }
        if (m_pulse_capture_timer != nullptr && xTimerDelete(m_pulse_capture_timer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse timer deletion failed");
        }
#if OASIS_ENABLE_PPG_SENSOR
        if (m_pulse_fifo_service_timer != nullptr &&
            xTimerDelete(m_pulse_fifo_service_timer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse FIFO service timer deletion failed");
        }
#endif
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

#if OASIS_ENABLE_NTC_SENSOR
        // ADC1 channel 1 is GPIO2 on ESP32-S3. ADCUnit stores it in slot 0.
        if (m_adc_unit_0.add_oneshot_gpio(NTC_ADC_GPIO) < 0) {
            ESP_LOGE("MainProcess", "NTC ADC GPIO registration failed");
            return false;
        }
#endif


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

#if OASIS_ENABLE_PPG_SENSOR
    void MainProcess::pulse_fifo_service_timer_callback(TimerHandle_t xtimer) {
        auto* self = static_cast<MainProcess*>(pvTimerGetTimerID(xtimer));
        if (xTaskNotify(self->m_task_handle, notify::TIMER_PULSE_FIFO_SERVICE, eSetBits) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse FIFO service notification failed");
        }
    }
#endif

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

        scan_sensor_i2c_bus();

#if !OASIS_ENABLE_NTC_SENSOR
        ESP_LOGI("MainProcess", "NTC sensor disabled by OASIS_ENABLE_NTC_SENSOR");
#endif

#if !OASIS_ENABLE_SHT31_SENSOR
        ESP_LOGI("MainProcess", "SHT31 sensor disabled by OASIS_ENABLE_SHT31_SENSOR");
#endif

#if OASIS_ENABLE_PPG_SENSOR
        ESP_LOGI("MainProcess", "MAX30102 30-second health monitoring enabled");
#else
        ESP_LOGI("MainProcess", "PPG sensor disabled by OASIS_ENABLE_PPG_SENSOR");
#endif

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
#if OASIS_ENABLE_PPG_SENSOR
        m_pulse_fifo_service_timer = xTimerCreate(
            "pulse FIFO service timer",
            pdMS_TO_TICKS(PULSE_FIFO_SERVICE_MS),
            pdTRUE,
            this,
            pulse_fifo_service_timer_callback
            );
#endif
        configASSERT(m_extern_cond_timer != nullptr);
        configASSERT(m_pulse_capture_timer != nullptr);
#if OASIS_ENABLE_PPG_SENSOR
        configASSERT(m_pulse_fifo_service_timer != nullptr);
#endif
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

#if OASIS_ENABLE_IMU_SENSOR
                if (notification_value & notify::ISR_IMU_BUFFER_FULL) {
                    //do IMU buffer flush.
                    //printf("Main Process, recv imu buffer\n");
                    imu_buffer_handle();
                    process_network_item();
                }
#endif

                if (notification_value & notify::TIMER_EXTREN_COND_CYCLE) {
                    read_extern_condition();
#if OASIS_ENABLE_PPG_SENSOR
                    check_pulse_sensor();
#endif
                    begin_pulse_capture();
                }

#if OASIS_ENABLE_PPG_SENSOR
                if (notification_value & notify::TIMER_PULSE_FIFO_SERVICE) {
                    service_pulse_capture();
                }
#endif

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
            if (m_pulse_capture_active) {
                constexpr size_t MAX_IMU_FIFO_SAMPLES = 24;
                for (size_t i = 0; i < MAX_IMU_FIFO_SAMPLES; ++i) {
                    if (m_kcal_imu_sample_count < m_kcal_imu_samples.size()) {
                        m_kcal_imu_samples[m_kcal_imu_sample_count++] = imu_data[i];
                    }
                    if (imu_data[i].is_last) break;
                }
            }
            imu_buffer.release_free(imu_data);
        }
    }

    void MainProcess::process_network_item() {
        
        //send to Network Queue
    }

    void MainProcess::read_extern_condition() {
#if OASIS_ENABLE_NTC_SENSOR
        m_last_ntc_temperature_c = m_ntc.read();
        printf("NTC : %f\n", m_last_ntc_temperature_c);
#endif
#if OASIS_ENABLE_SHT31_SENSOR
        const esp_err_t result = m_sht31.read_measurement(m_last_sht31);
        if (result == ESP_OK) {
            printf("SHT31 : %0.2f C, %0.2f %%RH\n", m_last_sht31.temperature_c,
                   m_last_sht31.humidity_percent);
        } else {
            printf("SHT31 read failed: %s\n", esp_err_to_name(result));
        }
#endif
    }

    void MainProcess::scan_sensor_i2c_bus() {
#if OASIS_ENABLE_SHT31_SENSOR || OASIS_ENABLE_PPG_SENSOR
        constexpr uint8_t FIRST_I2C_ADDRESS = 0x03;
        constexpr uint8_t LAST_I2C_ADDRESS = 0x77;
        bool found_device = false;

        ESP_LOGI("MainProcess", "I2C1 scan: SDA=%d SCL=%d", SENSOR_I2C_SDA_GPIO,
                 SENSOR_I2C_SCL_GPIO);
        for (uint8_t address = FIRST_I2C_ADDRESS; address <= LAST_I2C_ADDRESS; ++address) {
            const esp_err_t result = m_sensor_i2c.probe(address, 20);
            if (result == ESP_OK) {
                ESP_LOGI("MainProcess", "I2C1 device found at 0x%02X", address);
                found_device = true;
            } else if (result != ESP_ERR_NOT_FOUND) {
                ESP_LOGW("MainProcess", "I2C1 probe 0x%02X failed: %s", address,
                         esp_err_to_name(result));
            }
        }
        if (!found_device) {
            ESP_LOGW("MainProcess", "I2C1 scan found no device");
        }
#else
        ESP_LOGI("MainProcess", "I2C1 sensor bus disabled; SHT31 and PPG are both off");
#endif
    }

#if OASIS_ENABLE_PPG_SENSOR
    void MainProcess::check_pulse_sensor() {
        const bool was_ready = m_max30102_ready;
        const esp_err_t health_result = m_max30102.check_connection();
        if (health_result != ESP_OK) {
            m_max30102_ready = false;
            log_pulse_sensor_failure("30-second health check", health_result);
            if (was_ready) {
                ESP_LOGE("MainProcess", "MAX30102 connection lost; capture is disabled until recovery");
            }
            return;
        }

        // Refresh the configuration after every successful health check. This
        // also recovers a sensor that power-cycled between two 30-second checks
        // and is answering again with reset/default register values.
        const esp_err_t init_result = m_max30102.initialize();
        if (init_result != ESP_OK) {
            m_max30102_ready = false;
            log_pulse_sensor_failure("reinitialization", init_result);
            return;
        }

        m_max30102_ready = true;
        ESP_LOGI("MainProcess", "MAX30102 %s; configuration refreshed and ready for capture",
                 was_ready ? "is alive" : "detected/recovered");
    }
#endif

    void MainProcess::begin_pulse_capture() {
#if OASIS_ENABLE_PPG_SENSOR
        if (!m_max30102_ready) {
            ESP_LOGW("MainProcess", "MAX30102 capture skipped: sensor is not ready");
            return;
        }
        if (m_pulse_capture_active) return;

        m_ppg_sample_count = 0;
        m_kcal_imu_sample_count = 0;
        const esp_err_t result = m_max30102.start_measurement();
        if (result != ESP_OK) {
            m_max30102_ready = false;
            log_pulse_sensor_failure("measurement start", result);
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
            return;
        }
        if (xTimerStart(m_pulse_fifo_service_timer, 0) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse FIFO service timer start failed");
            if (xTimerStop(m_pulse_capture_timer, portMAX_DELAY) != pdPASS) {
                ESP_LOGE("MainProcess", "pulse timer stop after FIFO timer failure failed");
            }
            const esp_err_t stop_result = m_max30102.stop_measurement();
            if (stop_result != ESP_OK) {
                m_max30102_ready = false;
                log_pulse_sensor_failure("stop after FIFO timer failure", stop_result);
            }
            m_pulse_capture_active = false;
        }
#else
        // Preserve the 10-second cycle even without PPG hardware. finish_pulse_capture()
        // will enqueue a worker-data packet with BPM = 0.
        m_ppg_sample_count = 0;
        m_kcal_imu_sample_count = 0;
        m_pulse_capture_active = true;
        if (xTimerStart(m_pulse_capture_timer, 0) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse timer start failed while PPG is disabled");
            m_pulse_capture_active = false;
        }
#endif
    }

    void MainProcess::service_pulse_capture() {
#if OASIS_ENABLE_PPG_SENSOR
        if (!m_pulse_capture_active) return;

        size_t received = 0;
        const esp_err_t result = m_max30102.read_fifo(
            std::span<PPGSample>(m_ppg_samples).subspan(m_ppg_sample_count), received);
        if (result != ESP_OK) {
            m_max30102_ready = false;
            log_pulse_sensor_failure("FIFO read", result);
            if (xTaskNotify(m_task_handle, notify::TIMER_PULSE_CAPTURE_END, eSetBits) != pdPASS) {
                ESP_LOGE("MainProcess", "FIFO failure end notification failed");
            }
            return;
        }
        m_ppg_sample_count += received;
        if (m_ppg_sample_count == m_ppg_samples.size()) {
            // Preserve the samples already collected and stop before the local buffer overflows.
            if (xTaskNotify(m_task_handle, notify::TIMER_PULSE_CAPTURE_END, eSetBits) != pdPASS) {
                ESP_LOGE("MainProcess", "full-buffer notification failed");
            }
        }
#endif
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
#if OASIS_ENABLE_PPG_SENSOR
        if (xTimerStop(m_pulse_fifo_service_timer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE("MainProcess", "pulse FIFO service timer stop failed");
        }
        if (m_max30102_ready) {
            service_pulse_capture(); // Drain samples produced since the last 100 ms service tick.
        }
        const esp_err_t stop_result = m_max30102.stop_measurement();
        if (stop_result != ESP_OK) {
            m_max30102_ready = false;
            log_pulse_sensor_failure("measurement stop", stop_result);
        }
#endif
        m_pulse_capture_active = false;

        m_last_kcal_per_min = m_worker_data_solver.solve_kcal_per_min(
            IMU_SAMPLE_RATE_HZ,
            PULSE_SAMPLE_RATE_HZ,
            std::span<const IMUData>(m_kcal_imu_samples.data(), m_kcal_imu_sample_count),
            std::span<const PPGSample>(m_ppg_samples.data(), m_ppg_sample_count));

        const float bpm = calculate_bpm();
        printf("Pulse capture complete: %u samples, IMU: %u samples, BPM: %0.1f, kcal/min: %0.2f\n",
               static_cast<unsigned>(m_ppg_sample_count),
               static_cast<unsigned>(m_kcal_imu_sample_count), bpm, m_last_kcal_per_min);

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
        ESP_LOGI("MainProcess", "worker_data enqueue=%s temp=%u hum=%u bpm=%u battery=%u",
                 queued ? "true" : "false", item.raw_temp, item.raw_hum, item.raw_bpm,
                 item.battery_voltage);
#if OASIS_ENABLE_NTC_SENSOR
        ESP_LOGI("MainProcess", "NTC temperature=%.2f C", m_last_ntc_temperature_c);
#endif
    }

}
