/*
 * ESP32-S3 ADC Continuous Mode Example
 * - ADC Unit 1, Channel 5 (GPIO6)
 * - 1024바이트 버퍼가 채워질 때마다 콜백(ISR) -> 태스크 알림 -> 데이터 처리
 *
 * ESP-IDF v5.x 기준 (esp_adc/adc_continuous.h)
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
#include "soc/soc_caps.h"

/* ------------------- 사용자 설정 ------------------- */
#define ADC_UNIT            ADC_UNIT_1
#define ADC_CHANNEL         ADC_CHANNEL_5      // ESP32-S3: ADC1_CH5 = GPIO6
#define ADC_ATTEN           ADC_ATTEN_DB_12    // 감쇠(측정 가능 전압 범위)
#define ADC_BIT_WIDTH       SOC_ADC_DIGI_MAX_BITWIDTH

#define CONV_FRAME_SIZE     1024               // 콜백이 호출되는 프레임 크기 (바이트)
#define READ_BUF_SIZE       1024               // adc_continuous_read()로 한 번에 읽을 크기
#define SAMPLE_FREQ_HZ      (20 * 1000)        // 샘플링 주파수 (필요에 맞게 조정)
/* ---------------------------------------------------- */

static const char *TAG = "ADC_CONTINUOUS";
static TaskHandle_t s_task_handle = NULL;
static adc_continuous_handle_t adc_handle = NULL;

/**
 * @brief ADC 변환 프레임(CONV_FRAME_SIZE)이 완성될 때마다 호출되는 콜백
 *        ISR 컨텍스트에서 실행되므로 무거운 작업은 여기서 하지 않고
 *        태스크에 알림만 전달한다.
 */
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle,
                                      const adc_continuous_evt_data_t *edata,
                                      void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

/**
 * @brief ADC 연속모드 드라이버 초기화
 */
static void adc_continuous_init(void)
{
    /*
     * 주의: 지정 초기화(designated initializer)에서 일부 멤버를 생략하면
     * 컴파일 옵션(-Werror=missing-field-initializers, C++ 빌드 등)에 따라
     * 컴파일 에러가 발생할 수 있다. 그래서 구조체를 먼저 {0}으로 완전히
     * 0-초기화한 뒤, 필요한 필드만 개별 대입하는 방식을 사용한다.
     */
    /*
     * adc_continuous_handle_cfg_t 필드 (esp_adc/adc_continuous.h 기준):
     *   - max_store_buf_size : 내부 풀(ring buffer) 최대 크기, 바이트 단위
     *   - conv_frame_size    : 콜백이 호출되는 변환 프레임 크기, 바이트 단위
     *   - flags.flush_pool   : 1이면 내부 풀이 가득 찼을 때 자동으로 비운다 (오래된 데이터 버림)
     */
    adc_continuous_handle_cfg_t adc_config = {};
    adc_config.max_store_buf_size = CONV_FRAME_SIZE * 4;  // 내부 링버퍼 크기 (프레임의 배수 권장)
    adc_config.conv_frame_size    = CONV_FRAME_SIZE;      // 콜백이 호출되는 단위 (바이트)
    adc_config.flags.flush_pool   = 0;                    // 0: 풀이 가득 차면 새 데이터 손실(기본), 1: 자동 flush
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_digi_pattern_config_t adc_pattern[1] = {};
    adc_pattern[0].atten     = ADC_ATTEN;
    adc_pattern[0].channel   = ADC_CHANNEL;
    adc_pattern[0].unit      = ADC_UNIT;
    adc_pattern[0].bit_width = ADC_BIT_WIDTH;

    /*
     * adc_continuous_config_t 필드:
     *   - pattern_num    : 사용할 ADC 채널 개수
     *   - adc_pattern    : 채널별 설정 배열 포인터
     *   - sample_freq_hz : 샘플링 주파수(Hz)
     *   - conv_mode      : ADC1/ADC2 단일 또는 동시 사용 모드
     *   - format         : DMA 출력 포맷 (최신 마스터 브랜치에서는 deprecated 표시가
     *                       있지만, 현재 릴리즈(v5.x) 대부분에서는 여전히 필요한 필드이다)
     */
    adc_continuous_config_t dig_cfg = {};
    dig_cfg.pattern_num    = 1;
    dig_cfg.adc_pattern    = adc_pattern;
    dig_cfg.sample_freq_hz = SAMPLE_FREQ_HZ;
    dig_cfg.conv_mode      = ADC_CONV_SINGLE_UNIT_1;      // ADC1만 사용
    dig_cfg.format         = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    /*
     * adc_continuous_evt_cbs_t 필드:
     *   - on_conv_done : 프레임(conv_frame_size) 하나가 완성될 때마다 호출
     *   - on_pool_ovf  : 내부 풀이 가득 찼을 때 호출 (여기서는 사용 안 함 -> NULL)
     */
    adc_continuous_evt_cbs_t cbs = {};
    cbs.on_conv_done = s_conv_done_cb;
    cbs.on_pool_ovf  = NULL;
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));
}

/**
 * @brief ADC 데이터를 읽고 처리하는 태스크
 */
static void adc_continuous_task(void *arg)
{
    uint8_t result[READ_BUF_SIZE] = {};
    uint32_t ret_num = 0;
    esp_err_t ret;

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_init();
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    ESP_LOGI(TAG, "ADC 연속모드 시작 (Unit1, Channel%d)", ADC_CHANNEL);

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1) {
            ret = adc_continuous_read(adc_handle, result, READ_BUF_SIZE, &ret_num, 0);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "===== %" PRIu32 " 바이트 수신 (콜백 트리거) =====", ret_num);

                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
                    uint32_t chan_num = p->type2.channel;
                    uint32_t data     = p->type2.data;

                    if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT)) {
                        ESP_LOGI(TAG, "Unit1, Ch:%" PRIu32 ", Value:%" PRIu32, chan_num, data);
                    } else {
                        ESP_LOGW(TAG, "잘못된 데이터 - Ch:%" PRIu32 ", Value:%" PRIu32, chan_num, data);
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(1));
            } else if (ret == ESP_ERR_TIMEOUT) {
                break;
            }
        }
    }
}

extern "C" {
void app_main(void)
{
    xTaskCreate(adc_continuous_task, "adc_continuous_task", 4096, NULL, 5, NULL);
}
}

/*
std::optional<oasis::MainProcess> g_main_process = std::nullopt;
std::optional<oasis::NetworkProcess> g_network_process = std::nullopt;
///sht 9,10 imu 11,12 ntc7 , pulse8
extern "C" {

void app_main(void) {
    oasis::startup();
}
}

void oasis::startup() {

    g_main_process.emplace();
    g_main_process->create();
    g_network_process.emplace();
    g_network_process->create();
}
*/ 
