#include "DummyDataGenerator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

namespace Dummy {

namespace {

// ---- 랜덤 엔진 ----
std::mt19937& Rng() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

int RandomInt(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(Rng());
}

double RandomDouble(double lo, double hi) {
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(Rng());
}

// ---- 시료 속성 값 범위 ----
constexpr double kAvgProductionTimeMinLow = 0.2;
constexpr double kAvgProductionTimeMinHigh = 1.0;
constexpr double kYieldRateLow = 0.7;
constexpr double kYieldRateHigh = 0.99;

// ---- 초기 재고 범위(고갈/부족/여유) ----
constexpr int64_t kDepletedStock = 0;
constexpr int kShortageStockLow = 1;
constexpr int kShortageStockHigh = 20;
constexpr int kAmpleStockLow = 200;
constexpr int kAmpleStockHigh = 1000;

// ---- 주문 수량 범위 ----
constexpr int kBasicOrderQuantityLow = 5;   // RESERVED/REJECTED 주문 수량
constexpr int kBasicOrderQuantityHigh = 80;
constexpr int kProducingExtraQuantityLow = 5;   // 재고 부족을 재현하기 위한 초과분
constexpr int kProducingExtraQuantityHigh = 60;
constexpr int kStockLimitedQuantityCap = 80;   // CONFIRMED/RELEASE에서 재고가 있을 때의 상한
constexpr int kNoStockQuantityLow = 1;         // CONFIRMED/RELEASE에서 재고가 없을 때의 범위
constexpr int kNoStockQuantityHigh = 10;

// ---- 생성 시각 경과(offset) 범위(분 단위) ----
constexpr int kDefaultOffsetMinLow = 5;      // RESERVED/REJECTED/CONFIRMED/RELEASE 생성 시각
constexpr int kDefaultOffsetMinHigh = 1440;
constexpr int kProducingRecentOffsetMinLow = 1;    // 방금 승인되어 아직 생산 중일 가능성이 높은 케이스
constexpr int kProducingRecentOffsetMinHigh = 20;
constexpr int kProducingOldOffsetMinLow = 600;     // 오래 전에 승인되어 이미 생산이 끝났을 가능성이 높은 케이스
constexpr int kProducingOldOffsetMinHigh = 3000;

// ---- 상태 갱신(updatedAt) 지연 범위(분 단위) ----
constexpr int kRejectedUpdateDelayMinLow = 1;
constexpr int kRejectedUpdateDelayMinHigh = 30;
constexpr int kProducingUpdateDelayMinLow = 1;
constexpr int kProducingUpdateDelayMinHigh = 10;
constexpr int kConfirmedUpdateDelayMinLow = 1;
constexpr int kConfirmedUpdateDelayMinHigh = 30;
constexpr int kReleaseUpdateDelayMinLow = 30;
constexpr int kReleaseUpdateDelayMinHigh = 120;

// ---- 시료 이름 조합 풀 ----
const std::vector<std::string>& NamePrefixes() {
    static const std::vector<std::string> prefixes = {
        "실리콘 웨이퍼", "GaN 에피택셜", "SiC 파워기판", "포토레지스트",
        "산화막 웨이퍼", "사파이어 기판", "게르마늄 기판", "InP 에피택셜",
        "GaAs 웨이퍼", "석영 기판", "질화막 웨이퍼", "폴리싱 웨이퍼"
    };
    return prefixes;
}

const std::vector<std::string>& NameSuffixes() {
    static const std::vector<std::string> suffixes = {
        "-4인치", "-6인치", "-8인치", "-12인치", "-PR7", "-PR9", "-EX1"
    };
    return suffixes;
}

std::string RandomSampleName() {
    const auto& prefixes = NamePrefixes();
    const auto& suffixes = NameSuffixes();
    const std::string& prefix = prefixes[static_cast<size_t>(RandomInt(0, static_cast<int>(prefixes.size()) - 1))];
    const std::string& suffix = suffixes[static_cast<size_t>(RandomInt(0, static_cast<int>(suffixes.size()) - 1))];
    return prefix + suffix;
}

// ---- 고객사 이름 풀 ----
const std::vector<std::string>& CustomerNames() {
    static const std::vector<std::string> customers = {
        "한빛파운드리", "대한반도체", "네오세미컴", "코스모테크",
        "선진팹리스", "위드시스템즈", "청람테크", "블루칩스퀘어"
    };
    return customers;
}

std::string RandomCustomerName() {
    const auto& customers = CustomerNames();
    return customers[static_cast<size_t>(RandomInt(0, static_cast<int>(customers.size()) - 1))];
}

// ---- 시간 포맷 ----
std::tm ToLocalTm(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmValue{};
#if defined(_MSC_VER)
    localtime_s(&tmValue, &t);
#else
    localtime_r(&t, &tmValue);
#endif
    return tmValue;
}

std::string FormatDateTime(std::chrono::system_clock::time_point tp) {
    std::tm tmValue = ToLocalTm(tp);
    std::ostringstream oss;
    oss << std::put_time(&tmValue, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string FormatDateOnly(std::chrono::system_clock::time_point tp) {
    std::tm tmValue = ToLocalTm(tp);
    std::ostringstream oss;
    oss << std::put_time(&tmValue, "%Y%m%d");
    return oss.str();
}

std::string MakeSampleId(int index) {
    std::ostringstream oss;
    oss << "S-" << std::setw(3) << std::setfill('0') << index;
    return oss.str();
}

// 초기 재고를 여유/부족/고갈 케이스가 골고루 섞이도록 분산 배치한다.
// category 0: 고갈(0), 1: 부족(소량), 2: 여유(대량)
int64_t RandomStockForCategory(int category) {
    switch (category % 3) {
        case 0: return kDepletedStock;                                    // 고갈
        case 1: return RandomInt(kShortageStockLow, kShortageStockHigh);  // 부족
        default: return RandomInt(kAmpleStockLow, kAmpleStockHigh);       // 여유
    }
}

// CONFIRMED/RELEASE 주문 수량: 재고가 있으면 그 범위 내에서, 없으면 소량으로 생성한다.
int64_t RandomQuantityWithinStock(int64_t stock) {
    if (stock > 0) {
        return RandomInt(1, static_cast<int>(std::min<int64_t>(stock, kStockLimitedQuantityCap)));
    }
    return RandomInt(kNoStockQuantityLow, kNoStockQuantityHigh);
}

} // namespace

DummyDataGenerator::DummyDataGenerator(Persistence::IDataStore& store) : store_(store) {}

void DummyDataGenerator::GenerateSamples(int count, bool reset) {
    if (count <= 0) return;

    std::vector<Model::Sample> samples;
    int startIndex = 1;
    if (reset) {
        samples.clear();
    } else {
        samples = store_.LoadSamples();
        startIndex = static_cast<int>(samples.size()) + 1;
    }

    for (int i = 0; i < count; ++i) {
        Model::Sample sample;
        sample.id = MakeSampleId(startIndex + i);
        sample.name = RandomSampleName();
        sample.avgProductionTimeMin = RandomDouble(kAvgProductionTimeMinLow, kAvgProductionTimeMinHigh);
        sample.yieldRate = RandomDouble(kYieldRateLow, kYieldRateHigh);
        sample.stock = RandomStockForCategory(i);
        samples.push_back(sample);
    }

    store_.SaveSamples(samples);
}

void DummyDataGenerator::GenerateOrders(int countPerStatus, bool reset) {
    if (countPerStatus <= 0) return;

    std::vector<Model::Sample> samples = store_.LoadSamples();
    if (samples.empty()) return; // 시료가 없으면 주문을 만들 수 없다.

    std::vector<Model::Order> orders;
    if (reset) {
        orders.clear();
        store_.SaveOrders(orders);
    } else {
        orders = store_.LoadOrders();
    }

    static const Model::OrderStatus kStatuses[] = {
        Model::OrderStatus::RESERVED,
        Model::OrderStatus::REJECTED,
        Model::OrderStatus::PRODUCING,
        Model::OrderStatus::CONFIRMED,
        Model::OrderStatus::RELEASE
    };

    const auto now = std::chrono::system_clock::now();

    for (Model::OrderStatus status : kStatuses) {
        for (int i = 0; i < countPerStatus; ++i) {
            const Model::Sample& sample = samples[static_cast<size_t>(RandomInt(0, static_cast<int>(samples.size()) - 1))];

            // 상태별로 생성 시각(경과 시간)을 다르게 분산시켜
            // PRODUCING의 경우 "이미 생산 완료 가능"/"아직 진행 중" 케이스를 함께 재현한다.
            int offsetMinutes;
            switch (status) {
                case Model::OrderStatus::PRODUCING:
                    // 짝수 index: 방금 승인되어 아직 생산 중일 가능성이 높은 케이스.
                    // 홀수 index: 오래 전에 승인되어 이미 생산이 끝났을 가능성이 높은 케이스.
                    offsetMinutes = (i % 2 == 0)
                        ? RandomInt(kProducingRecentOffsetMinLow, kProducingRecentOffsetMinHigh)
                        : RandomInt(kProducingOldOffsetMinLow, kProducingOldOffsetMinHigh);
                    break;
                default:
                    offsetMinutes = RandomInt(kDefaultOffsetMinLow, kDefaultOffsetMinHigh);
                    break;
            }

            const auto createdTime = now - std::chrono::minutes(offsetMinutes);
            const std::string createdAt = FormatDateTime(createdTime);
            const std::string yyyymmdd = FormatDateOnly(createdTime);

            Model::Order order;
            order.orderNo = store_.NextOrderNo(yyyymmdd);
            order.sampleId = sample.id;
            order.customerName = RandomCustomerName();
            order.status = status;
            order.createdAt = createdAt;

            switch (status) {
                case Model::OrderStatus::RESERVED: {
                    order.quantity = RandomInt(kBasicOrderQuantityLow, kBasicOrderQuantityHigh);
                    order.updatedAt = createdAt; // 아직 승인/거절 전
                    break;
                }
                case Model::OrderStatus::REJECTED: {
                    order.quantity = RandomInt(kBasicOrderQuantityLow, kBasicOrderQuantityHigh);
                    const auto updatedTime = createdTime + std::chrono::minutes(
                        RandomInt(kRejectedUpdateDelayMinLow, kRejectedUpdateDelayMinHigh));
                    order.updatedAt = FormatDateTime(std::min(updatedTime, now));
                    break;
                }
                case Model::OrderStatus::PRODUCING: {
                    // 재고 부족 상황을 재현하기 위해 재고보다 많은 수량으로 주문한다.
                    const int64_t extra = RandomInt(kProducingExtraQuantityLow, kProducingExtraQuantityHigh);
                    order.quantity = sample.stock + extra;
                    order.shortage = order.quantity - sample.stock;
                    order.actualProductionQty = static_cast<int64_t>(
                        std::ceil(static_cast<double>(order.shortage) / sample.yieldRate));
                    order.totalProductionTimeMin = sample.avgProductionTimeMin * static_cast<double>(order.actualProductionQty);

                    const auto updatedTime = createdTime + std::chrono::minutes(
                        RandomInt(kProducingUpdateDelayMinLow, kProducingUpdateDelayMinHigh));
                    order.updatedAt = FormatDateTime(std::min(updatedTime, now));
                    break;
                }
                case Model::OrderStatus::CONFIRMED: {
                    order.quantity = RandomQuantityWithinStock(sample.stock);
                    const auto updatedTime = createdTime + std::chrono::minutes(
                        RandomInt(kConfirmedUpdateDelayMinLow, kConfirmedUpdateDelayMinHigh));
                    order.updatedAt = FormatDateTime(std::min(updatedTime, now));
                    break;
                }
                case Model::OrderStatus::RELEASE: {
                    order.quantity = RandomQuantityWithinStock(sample.stock);
                    const auto updatedTime = createdTime + std::chrono::minutes(
                        RandomInt(kReleaseUpdateDelayMinLow, kReleaseUpdateDelayMinHigh));
                    order.updatedAt = FormatDateTime(std::min(updatedTime, now));
                    break;
                }
            }

            orders.push_back(order);
            // NextOrderNo가 다음 번호를 정확히 채번할 수 있도록 즉시 반영한다.
            store_.SaveOrders(orders);
        }
    }
}

void DummyDataGenerator::SeedDefaultDataset(bool reset) {
    static constexpr int kDefaultSampleCount = 12;
    static constexpr int kDefaultOrdersPerStatus = 3;

    GenerateSamples(kDefaultSampleCount, reset);
    // 시료를 방금 만들었으므로, 주문은 항상 그 위에 이어서 쌓는다(append).
    GenerateOrders(kDefaultOrdersPerStatus, false);
}

} // namespace Dummy
