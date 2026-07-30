set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(
    SRCS "main.cpp"
         "source/ADCUnit.cpp"
         "source/firmware.cpp"
         "source/IMUProcess.cpp"
         "source/MainProcess.cpp"
         "source/NetworkProcess.cpp"
         "source/NetworkQueue.cpp"
         "source/NTC.cpp"
    INCLUDE_DIRS "." "include" "source"
    REQUIRES esp_adc driver esp_driver_gptimer
)