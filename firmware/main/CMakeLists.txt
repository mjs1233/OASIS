include(${CMAKE_CURRENT_LIST_DIR}/source/CmakeLists.txt)

idf_component_register(
        SRCS "${COMPONENT_SRCS}"
        INCLUDE_DIRS "include"
        REQUIRES esp_adc driver esp_driver_gptimer esp_driver_i2c esp_wifi esp_http_client nvs_flash
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
