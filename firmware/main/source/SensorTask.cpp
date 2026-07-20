#include "SensorTask.hpp"
namespace oasis {

    SensorTask::SensorTask() {

    }

    SensorTask::~SensorTask() {

    }

    void SensorTask::task(void* pvParameter) {
        static_cast<SensorTask*>(pvParameter)->run();
    }

    void SensorTask::run() {

    }

}