//
// Created by tgian on 26. 7. 31..
//

#ifndef FIRMWARE_WORKERDATASOLVER_HPP
#define FIRMWARE_WORKERDATASOLVER_HPP
#include "IMUData.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace oasis {
    enum class WorkerActivityState {
        idle,
        walking,
        working
    };

    class WorkerDataSolver {
    public:

        WorkerDataSolver(uint32_t weight);
        void update_imu_data(IMUData* data);
        void update_pulse_data();

        //thread unsafe
        float solve_kcal_per_min();
        [[nodiscard]]
        WorkerActivityState activity_state() const;

    private:
        struct AccGyro {
            float acc_mag;
            float gyro_mag;
        };


        float calc_pulse_fundamental();

        static constexpr uint32_t AccGyro_buffer_size = 40;
        static constexpr float thrs_acc_walk = 1.2f;
        static constexpr float thrs_gyro_work = 12.f;

        std::array<AccGyro, 40> m_AccGyro_ring_buffer {};
        uint32_t m_AccGyro_head = 0;

        uint32_t m_worker_weight = 0;
        WorkerActivityState m_activity_state = WorkerActivityState::idle;
    };
}
#endif //FIRMWARE_WORKERDATASOLVER_HPP
