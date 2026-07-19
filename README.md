## Setup
### Prerequisites
- C/C++
- CMake
- ESP-IDF
- Wokwi-CLI
- Python (maybe?)

### Emulation 
- ESP32-S3

### Procedural
1. clone OASIS
2. download VSCode ESP-IDF Extension 
3. setup ESP-IDF Extension in VSCode (download v6+ )
4. enter 
    $env:ESP_IDF_VERSION="6.0.0"
    idf.py set-target esp32s3
    idf.py build
5. check emulation wokwi.toml (change path if needed)
    
