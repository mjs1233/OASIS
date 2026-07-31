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
        float acc_sum = 0.f;
        float gyro_sum = 0.f;

        for (int i = 0; i < AccGyro_buffer_size; i++) {
            acc_sum += m_AccGyro_ring_buffer[i].acc_mag;
            gyro_sum += m_AccGyro_ring_buffer[i].gyro_mag;
        }
        float acc_mean = acc_sum / AccGyro_buffer_size;
        float gyro_mean = gyro_sum / AccGyro_buffer_size;

        float acc_var = 0.f;
        float gyro_var = 0.f;
        for (int i = 0; i < AccGyro_buffer_size; i++) {
            acc_var += (m_AccGyro_ring_buffer[i].acc_mag - acc_mean)
            * (m_AccGyro_ring_buffer[i].acc_mag - acc_mean);

            gyro_var += (m_AccGyro_ring_buffer[i].gyro_mag - gyro_mean)
            * (m_AccGyro_ring_buffer[i].gyro_mag - gyro_mean);
        }
        float acc_std = sqrtf(acc_var / AccGyro_buffer_size);
        float gyro_std = sqrtf(gyro_var / AccGyro_buffer_size);

        if (acc_std > thrs_acc_walk) {
            m_activity_state = WorkerActivityState::walking;
        } else {
            if (gyro_std > thrs_gyro_work) {
                m_activity_state = WorkerActivityState::working;
            } else {
                m_activity_state = WorkerActivityState::idle;
            }
        }

        uint32_t hr = 60.f * calc_pulse_fundamental();

        float hr_based_kcal = (-55.0969f + (0.6309f * hr) + (0.1988f * m_worker_weight)) / 251.04f;

        return hr_based_kcal;// * state_weight;
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