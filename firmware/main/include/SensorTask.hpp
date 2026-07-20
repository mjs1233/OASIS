//
// Created by tgian on 26. 7. 21..
//

#ifndef FIRMWARE_SENSORTASK_HPP
#define FIRMWARE_SENSORTASK_HPP

namespace oasis {
    class SensorTask {
    public:
        SensorTask();
        ~SensorTask();

        static void task(void* pvParameter);

    private:
        void run();

    public:



    };
}

#endif //FIRMWARE_SENSORTASK_HPP
