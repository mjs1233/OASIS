//
// Created by tgian on 26. 7. 31..
//
#include "WorkerDataSolver.hpp"

namespace oasis {

    WorkerDataSolver::WorkerDataSolver(uint32_t weight) : m_worker_weight(weight) {}

    void WorkerDataSolver::update_imu_data(IMUData* data) {

        constexpr uint32_t max_imu_data_length = 40;
        uint32_t count = 0;
        while (count < max_imu_data_length) {
            //fill ringbuffer
            m_AccGyro_ring_buffer[m_AccGyro_head].acc_mag =
                sqrtf(data->accel_x * data->accel_x
                + data->accel_y * data->accel_y
                + data->accel_z * data->accel_z);

            m_AccGyro_ring_buffer[m_AccGyro_head].gyro_mag =
                sqrtf(data->gyro_x * data->gyro_x
                    + data->gyro_y * data->gyro_y
                    + data->gyro_z * data->gyro_z);

            m_AccGyro_head = (m_AccGyro_head + 1) % max_imu_data_length;
            count++;
            if (data->is_last) {
                break;
            }
        }
    }

    void WorkerDataSolver::update_pulse_data() {

    }


    //thread unsafe
    float WorkerDataSolver::solve_kcal_per_min() {
        if (AccGyro_buffer_size == 0) {
            return 0.f;
        }

        float inv_size = 1.0f / static_cast<float>(AccGyro_buffer_size);
        float acc_sum = 0.f;
        float gyro_sum = 0.f;

        for (int i = 0; i < AccGyro_buffer_size; ++i) {
            acc_sum += m_AccGyro_ring_buffer[i].acc_mag;
            gyro_sum += m_AccGyro_ring_buffer[i].gyro_mag;
        }

        float acc_mean = acc_sum * inv_size;
        float gyro_mean = gyro_sum * inv_size;

        float acc_var = 0.f;
        float gyro_var = 0.f;
        for (int i = 0; i < AccGyro_buffer_size; ++i) {
            float acc_diff = m_AccGyro_ring_buffer[i].acc_mag - acc_mean;
            float gyro_diff = m_AccGyro_ring_buffer[i].gyro_mag - gyro_mean;
            acc_var += acc_diff * acc_diff;
            gyro_var += gyro_diff * gyro_diff;
        }

        float acc_std = sqrtf(acc_var * inv_size);
        float gyro_std = sqrtf(gyro_var * inv_size);

        // 2. 행동 상태 판단
        if (acc_std > thrs_acc_walk) {
            m_activity_state = WorkerActivityState::walking;
        } else if (gyro_std > thrs_gyro_work) {
            m_activity_state = WorkerActivityState::working;
        } else {
            m_activity_state = WorkerActivityState::idle;
        }
        float hr = 60.f * calc_pulse_fundamental();

        float m_worker_age = 20;
        // Keytel 공식 적용 (kJ/min -> kcal/min 환산 logic 보정)
        // -55.0969 + (0.6309 * HR) + (0.1988 * Weight) + (0.2017 * Age)
        float kj_per_min = -55.0969f + (0.6309f * hr) + (0.1988f * m_worker_weight) + (0.2017f * m_worker_age);
        float hr_based_kcal = kj_per_min / 4.184f; // kJ을 kcal로 변환

        return (hr_based_kcal > 0.f) ? hr_based_kcal : 0.f;
    }

    [[nodiscard]]
        WorkerActivityState WorkerDataSolver::activity_state() const {
        return m_activity_state;
    }


    float WorkerDataSolver::calc_pulse_fundamental() {
        ///TODO) add algorithm that calculates fundamental of heart beat .
        return 100.f;
    }

}