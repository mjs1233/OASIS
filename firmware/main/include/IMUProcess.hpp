//
// Created by tgian on 26. 7. 25..
//

#ifndef FIRMWARE_IMUPROCESS_HPP
#define FIRMWARE_IMUPROCESS_HPP

namespace oasis {

    class IMUProcess {
    public:
        IMUProcess() = default;
        ~IMUProcess() = default;

        IMUProcess(const IMUProcess&) = delete;
        IMUProcess(IMUProcess&&) = delete;
        IMUProcess& operator=(const IMUProcess&) = delete;
        IMUProcess& operator=(IMUProcess&&) = delete;

        bool create();

        static void run(void* pvParameter);
        static bool isr_callback(
            gptimer_handle_t timer,
            const gptimer_alarm_event_data_t *edata,
            void *user_ctx
            );

    private:
        bool create_impl();
        void init_impl();
        void update_impl();

        TaskHandle_t m_task_handle = nullptr;
        TaskHandle_t m_main_process_task_handle = nullptr;

        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 1024;
        static constexpr uint32_t PRIORITY = 6;
    };

} // namespace s
#endif //FIRMWARE_IMUPROCESS_HPP
