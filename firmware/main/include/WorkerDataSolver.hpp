//
// Created by tgian on 26. 7. 31..
//

#ifndef FIRMWARE_WORKERDATASOLVER_HPP
#define FIRMWARE_WORKERDATASOLVER_HPP
#include <cstdint>
#include <span>

#include "IMUData.hpp"
#include "MAX30102.hpp"

namespace oasis {
    class Kcal_per_min_solver {
    public:
        [[nodiscard]] static float solve(
            uint32_t imu_sample_rate_hz,
            uint32_t pulse_sample_rate_hz,
            std::span<const IMUData> imu_samples,
            std::span<const PPGSample> pulse_samples);
    };

    class WorkerDataSolver {
    public:
        [[nodiscard]] float solve_kcal_per_min(
            uint32_t imu_sample_rate_hz,
            uint32_t pulse_sample_rate_hz,
            std::span<const IMUData> imu_samples,
            std::span<const PPGSample> pulse_samples) const;
    };
}
#endif // FIRMWARE_WORKERDATASOLVER_HPP
