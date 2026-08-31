//
// Created by tgian on 26. 7. 31..
//

#include "WorkerDataSolver.hpp"

namespace oasis {
    float Kcal_per_min_solver::solve(
        uint32_t imu_sample_rate_hz,
        uint32_t pulse_sample_rate_hz,
        std::span<const IMUData> imu_samples,
        std::span<const PPGSample> pulse_samples) {
        // TODO: Implement filtering, FFT, heartbeat extraction, and kcal/min calculation.
        static_cast<void>(imu_sample_rate_hz);
        static_cast<void>(pulse_sample_rate_hz);
        static_cast<void>(imu_samples);
        static_cast<void>(pulse_samples);
        return 0.0f;
    }

    float WorkerDataSolver::solve_kcal_per_min(
        uint32_t imu_sample_rate_hz,
        uint32_t pulse_sample_rate_hz,
        std::span<const IMUData> imu_samples,
        std::span<const PPGSample> pulse_samples) const {
        return Kcal_per_min_solver::solve(
            imu_sample_rate_hz,
            pulse_sample_rate_hz,
            imu_samples,
            pulse_samples);
    }
}
