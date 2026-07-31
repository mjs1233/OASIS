#include <iostream>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include "Log.hpp"
#include "httplib.h"
#include "Server.hpp"


// core_temp_estimator.cpp
//
// Modified 2-Node Human Thermal Model 기반 Core Temperature / TTD 추정기
// 참고: Hamatani et al., "Estimating Core Body Temperature Based on Human
//       Thermal Model Using Wearable Sensors" (SAC'15) + OASIS 내부 문서
//
// 가정:
//  - 사용자 프로필(Abody, mskin, mcore, Mbasal, Weff, pr1=초기Tcore, pr2=초기Tskin)은
//    외부에서 이미 적절히 산출되어 주어진다고 가정 (본 파일에서는 그대로 struct에 채워 넣음)
//  - 개인화 파라미터 중 pr3, pr4, pr7 만 매트릭스 서치 대상으로 커스텀
//  - pr5는 논문 기본값(75)으로 고정, pr6는 논문/OASIS 문서 가정(pr2 < Tskin)에 따라 제거
//  - 센서 입력: Tskin(이마 체온), Tair(기온, 직사광 제외), phi_air(외부습도),
//               phi_helmet(헬멧 내부습도), Mex(활동량, kcal/min -> W/m^2 환산)
//
// 컴파일: g++ -O2 -std=c++17 core_temp_estimator.cpp -o core_temp_estimator

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <optional>
#include <string>
#include <vector>

// ------------------------------------------------------------------
// 1. 고정 모델 상수 (논문 Table 1)
// ------------------------------------------------------------------
struct ModelConstants {
    // 논문 Table 1의 c_skin/c_core(0.97)는 kcal/(kg*C) 스케일로 추정됨(인체 조직 비열과 근사).
    // m*c 항을 SI(J/C)로 맞추기 위해 4184 J/kcal 로 환산해서 사용한다.
    double c_skin = 0.97;   // Skin specific heat capacity [kcal/(kg*C)]
    double c_core = 0.97;   // Core specific heat capacity [kcal/(kg*C)]
    double c_blo  = 1.163;  // Blood specific heat capacity
    double K_min  = 5.28;   // Minimum skin thermal conductance
    double clo    = 0.6;    // Clothing insulation
    double h_conv = 4.3;    // Convective heat transfer coefficient
    double h_rad  = 5.23;   // Radiation heat exchange coefficient
    double F_cl   = 0.53;   // Efficiency for passage of dry heat (already solved for clo=0.6)
    double F_pcl  = 0.73;   // Efficiency for water vapor evaporation (already solved for clo=0.6)
};

// ------------------------------------------------------------------
// 2. 사용자 프로필 (외부에서 주어짐, 이미 적절히 도출되었다고 가정)
// ------------------------------------------------------------------
struct UserProfile {
    double A_body;      // 체표면적 [m^2] (DuBois식 등으로 외부 산출)
    double m_skin;      // 피부 노드 질량 [kg] (또는 등가 열용량 스케일)
    double m_core;      // 심부 노드 질량 [kg]
    double M_basal;     // 기초대사량 [W/m^2]
    double W_eff;       // 운동 효율 (보행 기준 0.4 등, 외부에서 활동유형 반영해 산출)
    double pr1_Tcore0;  // 개인화 파라미터 pr1: 초기 Tcore [C]
    double pr2_Tskin0;  // 개인화 파라미터 pr2: 초기 Tskin [C]
};

// ------------------------------------------------------------------
// 3. 센서 입력 (매 timestep)
// ------------------------------------------------------------------
struct SensorSample {
    double T_skin_meas;   // 이마 피부온도 실측값 [C] (모델의 Tskin 실측 기준)
    double T_air;         // 외부 기온 [C]
    double phi_air;       // 외부 상대습도 [0~1]
    double phi_helmet;    // 헬멧 내부 상대습도 [0~1]
    double Mex_kcal_min;  // 운동대사열, 활동량 센서 원값 [kcal/min]
};

// ------------------------------------------------------------------
// 4. 개인화(튜닝) 파라미터 - 이번 구현에서 매트릭스 서치 대상은 pr3, pr4, pr7 뿐
//    pr5는 기본값 고정, pr6는 제거(pr2 < Tskin 가정)
// ------------------------------------------------------------------
struct IndividualParams {
    double pr3;
    double pr4;
    double pr7;
    double pr5_fixed = 75.0; // 기본값 고정 (Table 2 default)
};

// 활동량(kcal/min) -> W/m^2 환산 (체표면적 나눔, 1kcal/min = 69.78 W)
inline double kcalPerMinToWm2(double kcal_min, double A_body) {
    constexpr double KCAL_MIN_TO_WATT = 69.78;
    return (kcal_min * KCAL_MIN_TO_WATT) / A_body;
}

// 포화 수증기압(kPa) 근사 - Buck equation (두 문서 모두 이 식은 명시하지 않아 표준식 채택)
inline double saturationVaporPressure_kPa(double T_celsius) {
    return 0.61121 * std::exp((18.678 - T_celsius / 234.5) * (T_celsius / (257.14 + T_celsius)));
}

// ------------------------------------------------------------------
// 5. 2-Node 모델 한 스텝 적분 (Euler, dt 분 단위)
// ------------------------------------------------------------------
struct NodeState {
    double Tcore;
    double Tskin;
};

class TwoNodeModel {
public:
    explicit TwoNodeModel(const ModelConstants& consts) : c_(consts) {}

    // 1 timestep(dt 분) 적분 -> 다음 상태 반환
    NodeState step(const NodeState& state,
                   const SensorSample& sample,
                   const UserProfile& profile,
                   const IndividualParams& p,
                   double dt_min) const {
        const double Tcore = state.Tcore;
        const double Tskin = state.Tskin;

        // --- 대사열 ---
        const double Mex = kcalPerMinToWm2(sample.Mex_kcal_min, profile.A_body);
        const double Mtotal = profile.M_basal + Mex;
        const double W = Mex * profile.W_eff;

        // --- 포화 수증기압 (kPa -> mmHg 스케일 정합을 위해 원 논문 상수 units 그대로 사용)
        // 논문 수식들은 Pskin, Pair를 mmHg 근사 스케일로 다루므로 kPa*7.50062 로 변환
        const double P_skin = saturationVaporPressure_kPa(Tskin) * 7.50062;
        const double P_air  = saturationVaporPressure_kPa(sample.T_air) * 7.50062;

        // --- 개인화 파라미터 기반 발한량 / 혈류량 ---
        const double dCore = std::max(0.0, Tcore - profile.pr1_Tcore0);
        const double dSkin = std::max(0.0, Tskin - profile.pr2_Tskin0);

        double m_rsw = p.pr7 * dCore + p.pr3 * dCore * dSkin / 1000.0;
        m_rsw = std::max(0.0, m_rsw) / 60.0; // 분당 -> 초당 스케일 정합 (논문 식 그대로)

        // Vblo = (pr4 + pr5*(Tcore-pr1)) / 60  (pr6 제거, pr2<Tskin 가정)
        double Vblo = (p.pr4 + p.pr5_fixed * dCore) / 60.0;
        Vblo = std::max(0.0, Vblo);

        // --- 열교환 항 ---
        const double q_blo  = c_.c_blo * Vblo * (Tcore - Tskin);
        const double q_cond = c_.K_min * (Tcore - Tskin);
        const double q_convrad = (c_.h_conv + c_.h_rad) * (Tskin - sample.T_air) * c_.F_cl;

        const double Emax = 2.2 * c_.h_conv * (P_skin - sample.phi_helmet * P_air) * c_.F_pcl;

        double q_rsw = 0.7 * m_rsw * std::pow(2.0, (Tskin - profile.pr2_Tskin0) / 3.0);
        double q_diff;
        if (q_rsw > Emax) {
            q_rsw = Emax;   // 땀이 다 증발 못함 -> Emax로 클램프
            q_diff = 0.0;   // 피부 완전히 젖음
        } else {
            q_diff = 0.06 * Emax;
        }

        const double q_res = 0.0023 * Mtotal * (44.0 - sample.phi_air * P_air);

        // --- 미분방정식 (Euler 적분) ---
        // 열유속(q_*, M_*)은 W/m^2 이므로 A_body를 곱하면 W(=J/s).
        // 왼쪽 열용량 항(m*c)은 kcal/C 단위이므로 4184 J/kcal 로 SI 환산해야
        // 오른쪽 Watt 항과 물리적으로 정합한다. dt는 분 단위 입력이므로 60을 곱해 초로 변환.
        constexpr double KCAL_TO_JOULE = 4184.0;
        const double dt_sec = dt_min * 60.0;

        const double thermalMassSkin = profile.m_skin * c_.c_skin * KCAL_TO_JOULE; // J/C
        const double thermalMassCore = profile.m_core * c_.c_core * KCAL_TO_JOULE; // J/C

        const double dTskin =
            (q_cond + q_blo - q_diff - q_rsw - q_convrad) * profile.A_body /
            thermalMassSkin * dt_sec;

        const double dTcore =
            (Mtotal - W - q_res - q_cond - q_blo) * profile.A_body /
            thermalMassCore * dt_sec;

        NodeState next;
        next.Tskin = Tskin + dTskin;
        next.Tcore = Tcore + dTcore;
        return next;
    }

private:
    ModelConstants c_;
};

// ------------------------------------------------------------------
// 6. 파라미터 매트릭스 (pr3, pr4, pr7만 변화, Table 2 값 기반)
//    ※ OCR 원본 표가 일부 불명확하여 대표 후보값으로 구성했습니다.
//      운영 투입 전 논문 Table 2 원본과 재대조를 권장합니다.
// ------------------------------------------------------------------
inline std::vector<IndividualParams> buildParamMatrix() {
    static const std::vector<double> pr3_cands = {100, 80, 60, 40, 20, 10, 5, 0.315, 0.1575, 0.07875};
    static const std::vector<double> pr4_cands = {12.6, 10.08, 7.56, 5.04, 2.52, 1.26, 0.63};
    static const std::vector<double> pr7_cands = {250, 200, 150, 100, 50, 25, 12.5};

    std::vector<IndividualParams> matrix;
    matrix.reserve(pr3_cands.size() * pr4_cands.size() * pr7_cands.size());
    for (double pr3 : pr3_cands)
        for (double pr4 : pr4_cands)
            for (double pr7 : pr7_cands)
                matrix.push_back(IndividualParams{pr3, pr4, pr7});
    return matrix;
}

// ------------------------------------------------------------------
// 7. 파라미터 세트별 추적 상태 + 필터링 + Core Temp 추정 + TTD
// ------------------------------------------------------------------
class CoreTempEstimator {
public:
    CoreTempEstimator(const ModelConstants& consts,
                       const UserProfile& profile,
                       double theta_init = 0.0012,
                       double dt_min = 1.0)
        : model_(consts), profile_(profile), theta_(theta_init), dt_min_(dt_min) {
        auto matrix = buildParamMatrix();
        tracks_.reserve(matrix.size());
        for (auto& p : matrix) {
            Track t;
            t.params = p;
            t.state = NodeState{profile.pr1_Tcore0, profile.pr2_Tskin0};
            tracks_.push_back(t);
        }
    }

    // 매 timestep 호출: 센서 샘플 하나 반영, 현재 추정 Core Temp / TTD 반환
    struct StepResult {
        double estimated_Tcore;
        int feasible_count;
        std::optional<double> rate_of_change; // C per minute
        std::optional<double> ttd_min;         // Time To Danger [분]
    };

    StepResult update(const SensorSample& sample, double T_critical) {
        // 실측 피부온 변화량 기록
        if (prev_meas_Tskin_.has_value()) {
            meas_diffs_.push_back(sample.T_skin_meas - *prev_meas_Tskin_);
        }
        prev_meas_Tskin_ = sample.T_skin_meas;

        // 모든 파라미터 세트에 대해 1스텝 시뮬레이션
        for (auto& t : tracks_) {
            NodeState prevState = t.state;
            t.state = model_.step(t.state, sample, profile_, t.params, dt_min_);
            t.sim_diffs.push_back(t.state.Tskin - prevState.Tskin);
        }

        // 거리(d) 계산 및 필터링 (윈도우 = 지금까지 누적된 전체 구간, 논문 정의 그대로)
        double theta = theta_;
        std::vector<const Track*> feasible;
        while (feasible.empty() && theta < 1.0) { // 논문처럼 세트가 없으면 theta를 완화
            feasible.clear();
            for (auto& t : tracks_) {
                double d = computeDistance(t.sim_diffs);
                if (d <= theta) feasible.push_back(&t);
            }
            if (feasible.empty()) theta += 0.0001;
        }

        // Core temp 추정 = feasible set 들의 평균
        double sum = 0.0;
        for (auto* t : feasible) sum += t->state.Tcore;
        double estimated = feasible.empty() ? averageAll() : sum / feasible.size();

        // TTD 계산
        StepResult result;
        result.estimated_Tcore = estimated;
        result.feasible_count = static_cast<int>(feasible.size());

        if (prev_estimated_.has_value()) {
            double rate = (estimated - *prev_estimated_) / dt_min_; // C per minute
            result.rate_of_change = rate;
            if (rate > 1e-6) { // 상승 중일 때만 TTD 의미 있음
                result.ttd_min = (T_critical - estimated) / rate;
            }
        }
        prev_estimated_ = estimated;

        return result;
    }

private:
    struct Track {
        IndividualParams params;
        NodeState state;
        std::vector<double> sim_diffs;
    };

    double computeDistance(const std::vector<double>& sim_diffs) const {
        // d = (1/t) * sum |meas_diff_i - sim_diff_i|
        size_t n = std::min(meas_diffs_.size(), sim_diffs.size());
        if (n == 0) return 0.0;
        double sum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            sum += std::fabs(meas_diffs_[i] - sim_diffs[i]);
        }
        return sum / static_cast<double>(n);
    }

    double averageAll() const {
        double sum = 0.0;
        for (auto& t : tracks_) sum += t.state.Tcore;
        return tracks_.empty() ? profile_.pr1_Tcore0 : sum / tracks_.size();
    }

    TwoNodeModel model_;
    UserProfile profile_;
    double theta_;
    double dt_min_;

    std::vector<Track> tracks_;
    std::vector<double> meas_diffs_;
    std::optional<double> prev_meas_Tskin_;
    std::optional<double> prev_estimated_;
};

// ------------------------------------------------------------------
// 8. 사용 예시 (main) - 실제로는 센서 스트림을 매 분 넣어주면 됨
// ------------------------------------------------------------------
int main() {
    ModelConstants consts;

    // 사용자 프로필: 실제로는 외부(앱/서버)에서 신장/체중/나이/성별 등으로부터
    // A_body, m_skin, m_core, M_basal, W_eff, pr1, pr2 를 산출해 전달한다고 가정
    UserProfile profile;
    profile.A_body = 1.8;      // m^2
    profile.m_skin = 4.5;      // kg (등가 스케일)
    profile.m_core = 30.0;     // kg (등가 스케일)
    profile.M_basal = 58.0;    // W/m^2 (안정시 대사량 근사)
    profile.W_eff = 0.4;       // 보행 기준 (Cavagna & Kaneko, 1977)
    profile.pr1_Tcore0 = 36.6; // 초기 Tcore (기본값)
    profile.pr2_Tskin0 = 34.1; // 초기 Tskin (기본값)

    CoreTempEstimator estimator(consts, profile);

    // 임계 심부온도 (안전 기준, 시스템 설정값)
    const double T_critical = 39.0;

    // --- 가상의 60분 보행 시나리오 (센서 스트림 예시) ---
    std::vector<SensorSample> stream;
    for (int t = 0; t < 60; ++t) {
        SensorSample s;
        s.T_skin_meas  = 34.1 + 0.02 * t;      // 서서히 상승하는 이마 온도 예시
        s.T_air        = 33.0;                  // 기온
        s.phi_air      = 0.55;                  // 외부습도
        s.phi_helmet   = 0.75;                  // 헬멧 내부습도 (환기 나쁨 가정)
        s.Mex_kcal_min = 5.0;                   // 활동량 (보행 예시)
        stream.push_back(s);
    }

    std::printf("t[min]\tTcore_est\tfeasible#\trate(C/min)\tTTD(min)\n");
    for (int t = 0; t < static_cast<int>(stream.size()); ++t) {
        auto r = estimator.update(stream[t], T_critical);
        std::printf("%d\t%.3f\t\t%d\t\t%s\t%s\n",
                    t,
                    r.estimated_Tcore,
                    r.feasible_count,
                    r.rate_of_change ? std::to_string(*r.rate_of_change).c_str() : "-",
                    r.ttd_min ? std::to_string(*r.ttd_min).c_str() : "-");
    }
    return 0;
}


int main(int argc, char** argv) {

    const char* default_server_config_path = "./server_config.json";
    std::filesystem::path server_config_path;
    if (argc < 2) {
       server_config_path = default_server_config_path;
    }
    else
        server_config_path = argv[1];

    if(!std::filesystem::exists(server_config_path)) {
        std::print("cannot find server config file\n");
        return 0;
    }
    oasis::ServerConfig server_config;
    if (!server_config.read_config(server_config_path)) {
        std::print("cannot read server config file\n");
        return 0;
    }

    oasis::Logger::create(server_config.log_path);
    oasis::Logger::instance().set_stdout(true);
    oasis::Logger::instance().write_info("global", "init start");

    try {
        oasis::Server server(server_config);
        server.start();
    } catch (const std::exception& exception) {
        oasis::Logger::instance().write_error("global",
            "server startup failed: " + std::string(exception.what()));
        return 1;
    }
    return 0;
}
