#include "OrderService.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace Monitor {

namespace {

std::string NowString() {
    // "YYYY-MM-DD HH:MM:SS" 포맷은 Console 계층(NowTimeString)과 공유하므로
    // Model::FormatLocalTimestamp에 위임한다(계층 간 중복 구현 방지).
    return Model::FormatLocalTimestamp(std::time(nullptr));
}

std::string TodayYyyymmdd() {
    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmv, "%Y%m%d");
    return oss.str();
}

// "YYYY-MM-DD HH:MM:SS" -> time_t (파싱 실패 시 현재 시각으로 대체)
std::time_t ParseTime(const std::string& text) {
    std::tm tmv{};
    std::istringstream iss(text);
    iss >> std::get_time(&tmv, "%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return std::time(nullptr);
    }
    tmv.tm_isdst = -1;
    return std::mktime(&tmv);
}

std::time_t AddMinutes(std::time_t base, double minutes) {
    double seconds = minutes * 60.0;
    return base + static_cast<std::time_t>(std::llround(seconds));
}

// 실 생산량 = ceil(부족분 / 수율)
int64_t CeilDivide(int64_t numerator, double rate) {
    if (rate <= 0.0) {
        rate = 1.0; // 방어적 처리: 수율은 0 초과 1 이하여야 함
    }
    if (numerator <= 0) {
        return 0;
    }
    double result = std::ceil(static_cast<double>(numerator) / rate);
    return static_cast<int64_t>(result);
}

} // namespace

std::vector<Model::Order>::iterator OrderService::FindOrderOrThrow(std::vector<Model::Order>& orders, const std::string& orderNo) {
    auto it = std::find_if(orders.begin(), orders.end(), [&](const Model::Order& o) { return o.orderNo == orderNo; });
    if (it == orders.end()) {
        throw std::invalid_argument("존재하지 않는 주문번호: " + orderNo);
    }
    return it;
}

Model::Sample OrderService::FindSampleOrThrow(const std::string& sampleId) const {
    auto sampleOpt = store_.FindSampleById(sampleId);
    if (!sampleOpt.has_value()) {
        throw std::invalid_argument("존재하지 않는 시료 ID: " + sampleId);
    }
    return sampleOpt.value();
}

void OrderService::RequireStatus(const Model::Order& order, Model::OrderStatus expected, const std::string& errorMessage) {
    if (order.status != expected) {
        throw std::logic_error(errorMessage);
    }
}

OrderService::OrderService(Persistence::IDataStore& store) : store_(store) {}

Model::Order OrderService::PlaceOrder(const std::string& sampleId, const std::string& customerName, int64_t quantity) {
    FindSampleOrThrow(sampleId); // 존재 여부만 확인(값은 이후 로직에서 사용하지 않음)
    if (quantity <= 0) {
        throw std::invalid_argument("주문 수량은 0보다 커야 합니다.");
    }

    Model::Order order;
    order.orderNo = store_.NextOrderNo(TodayYyyymmdd());
    order.sampleId = sampleId;
    order.customerName = customerName;
    order.quantity = quantity;
    order.status = Model::OrderStatus::RESERVED;
    order.createdAt = NowString();
    order.updatedAt = order.createdAt;

    auto orders = store_.LoadOrders();
    orders.push_back(order);
    store_.SaveOrders(orders);

    return order;
}

std::vector<Model::Order> OrderService::GetPendingOrders() const {
    std::vector<Model::Order> result;
    for (auto& o : store_.LoadOrders()) {
        if (o.status == Model::OrderStatus::RESERVED) {
            result.push_back(o);
        }
    }
    return result;
}

Model::Order OrderService::ApproveOrder(const std::string& orderNo) {
    auto orders = store_.LoadOrders();
    auto it = FindOrderOrThrow(orders, orderNo);
    RequireStatus(*it, Model::OrderStatus::RESERVED, "RESERVED 상태의 주문만 승인할 수 있습니다: " + orderNo);

    Model::Sample sample = FindSampleOrThrow(it->sampleId);

    // 두 분기(CONFIRMED/PRODUCING) 모두 공통으로 갱신되는 값.
    it->updatedAt = NowString(); // CONFIRMED 확정 시각 또는 생산 큐 등록(=큐잉) 시각

    if (sample.stock >= it->quantity) {
        // 재고 충분 -> 즉시 CONFIRMED, 이중 배정 방지 위해 재고 차감
        sample.stock -= it->quantity;
        it->status = Model::OrderStatus::CONFIRMED;
        it->shortage = 0;
        it->actualProductionQty = 0;
        it->totalProductionTimeMin = 0.0;
    } else {
        // 재고 부족 -> 생산 큐 등록(PRODUCING)
        int64_t shortage = it->quantity - sample.stock;
        int64_t actualProductionQty = CeilDivide(shortage, sample.yieldRate);
        double totalProductionTimeMin = sample.avgProductionTimeMin * static_cast<double>(actualProductionQty);

        it->status = Model::OrderStatus::PRODUCING;
        it->shortage = shortage;
        it->actualProductionQty = actualProductionQty;
        it->totalProductionTimeMin = totalProductionTimeMin;
    }

    Model::Order updated = *it;

    store_.SaveOrders(orders);

    // 재고 변경이 있었던 경우(승인 즉시 CONFIRMED)에만 저장
    if (updated.status == Model::OrderStatus::CONFIRMED) {
        auto samples = store_.LoadSamples();
        auto sIt = std::find_if(samples.begin(), samples.end(), [&](const Model::Sample& s) { return s.id == sample.id; });
        if (sIt != samples.end()) {
            sIt->stock = sample.stock;
        }
        store_.SaveSamples(samples);
    }

    return updated;
}

Model::Order OrderService::RejectOrder(const std::string& orderNo) {
    auto orders = store_.LoadOrders();
    auto it = FindOrderOrThrow(orders, orderNo);
    RequireStatus(*it, Model::OrderStatus::RESERVED, "RESERVED 상태의 주문만 거절할 수 있습니다: " + orderNo);

    it->status = Model::OrderStatus::REJECTED;
    it->updatedAt = NowString();

    Model::Order updated = *it;
    store_.SaveOrders(orders);
    return updated;
}

Model::Order OrderService::ReleaseOrder(const std::string& orderNo) {
    auto orders = store_.LoadOrders();
    auto it = FindOrderOrThrow(orders, orderNo);
    RequireStatus(*it, Model::OrderStatus::CONFIRMED, "CONFIRMED 상태의 주문만 출고할 수 있습니다: " + orderNo);

    it->status = Model::OrderStatus::RELEASE;
    it->updatedAt = NowString();

    Model::Order updated = *it;
    store_.SaveOrders(orders);
    return updated;
}

std::vector<Model::Order> OrderService::GetReleasableOrders() const {
    std::vector<Model::Order> result;
    for (auto& o : store_.LoadOrders()) {
        if (o.status == Model::OrderStatus::CONFIRMED) {
            result.push_back(o);
        }
    }
    return result;
}

std::vector<OrderStatusCount> OrderService::GetOrderCountsByStatus() const {
    static const std::vector<Model::OrderStatus> kMonitoredStatuses = {
        Model::OrderStatus::RESERVED,
        Model::OrderStatus::CONFIRMED,
        Model::OrderStatus::PRODUCING,
        Model::OrderStatus::RELEASE,
    };

    std::unordered_map<int, int64_t> counts;
    for (auto& o : store_.LoadOrders()) {
        if (o.status == Model::OrderStatus::REJECTED) {
            continue; // REJECTED는 모니터링에서 제외
        }
        counts[static_cast<int>(o.status)]++;
    }

    std::vector<OrderStatusCount> result;
    result.reserve(kMonitoredStatuses.size());
    for (auto status : kMonitoredStatuses) {
        int64_t count = 0;
        auto found = counts.find(static_cast<int>(status));
        if (found != counts.end()) {
            count = found->second;
        }
        result.push_back(OrderStatusCount{ status, count });
    }
    return result;
}

std::vector<InventoryStatus> OrderService::GetInventoryStatus() const {
    auto samples = store_.LoadSamples();
    auto orders = store_.LoadOrders();

    // 시료별 활성 주문 수요(RESERVED + PRODUCING) 합산
    std::unordered_map<std::string, int64_t> demandBySample;
    for (auto& o : orders) {
        if (o.status == Model::OrderStatus::RESERVED || o.status == Model::OrderStatus::PRODUCING) {
            demandBySample[o.sampleId] += o.quantity;
        }
    }

    std::vector<InventoryStatus> result;
    result.reserve(samples.size());
    for (auto& s : samples) {
        Model::StockState state;
        if (s.stock <= 0) {
            state = Model::StockState::DEPLETED;
        } else {
            int64_t demand = 0;
            auto found = demandBySample.find(s.id);
            if (found != demandBySample.end()) {
                demand = found->second;
            }
            state = (s.stock < demand) ? Model::StockState::SHORTAGE : Model::StockState::ABUNDANT;
        }
        result.push_back(InventoryStatus{ s, state });
    }
    return result;
}

std::vector<OrderService::ScheduledProduction> OrderService::BuildProductionSchedule() const {
    auto orders = store_.LoadOrders();

    std::vector<Model::Order> producing;
    for (auto& o : orders) {
        if (o.status == Model::OrderStatus::PRODUCING) {
            producing.push_back(o);
        }
    }

    // FIFO: 생산 큐 등록(=PRODUCING 전환) 시각(updatedAt) 순서로 정렬.
    // stable_sort를 사용해, 같은 초에 등록된 주문들은 저장소에 기록된(=승인 처리된) 원래 순서를
    // 그대로 유지한다(std::sort는 동률 시 순서를 보장하지 않아 FIFO가 실행마다 달라질 수 있었다).
    std::stable_sort(producing.begin(), producing.end(), [](const Model::Order& a, const Model::Order& b) {
        return a.updatedAt < b.updatedAt;
    });

    std::vector<ScheduledProduction> schedule;
    schedule.reserve(producing.size());

    std::time_t lineFreeTime = 0;
    bool lineInitialized = false;

    for (auto& order : producing) {
        std::time_t queuedAt = ParseTime(order.updatedAt);
        std::time_t startTime = lineInitialized ? std::max(lineFreeTime, queuedAt) : queuedAt;
        std::time_t finishTime = AddMinutes(startTime, order.totalProductionTimeMin);

        lineFreeTime = finishTime;
        lineInitialized = true;

        schedule.push_back(ScheduledProduction{ order, startTime, finishTime });
    }

    return schedule;
}

std::vector<ProductionQueueItem> OrderService::GetProductionQueue() const {
    auto schedule = BuildProductionSchedule();
    const std::time_t now = std::time(nullptr);

    std::vector<ProductionQueueItem> result;
    result.reserve(schedule.size());

    for (auto& entry : schedule) {
        auto sampleOpt = store_.FindSampleById(entry.order.sampleId);
        Model::Sample sample = sampleOpt.has_value() ? sampleOpt.value() : Model::Sample{};

        const double totalProductionTimeMin = entry.order.totalProductionTimeMin;
        double elapsedMinutes;
        double progressPercent;
        if (totalProductionTimeMin <= 0.0) {
            // 0으로 나누기 방지: 생산 시간이 없는(=즉시 완료 취급) 경우 완료 처리.
            elapsedMinutes = 0.0;
            progressPercent = 100.0;
        } else {
            double rawElapsedMinutes = std::difftime(now, entry.startTime) / 60.0;
            elapsedMinutes = std::clamp(rawElapsedMinutes, 0.0, totalProductionTimeMin);
            progressPercent = elapsedMinutes / totalProductionTimeMin * 100.0;
        }

        ProductionQueueItem item;
        item.order = entry.order;
        item.sample = sample;
        item.elapsedMinutes = elapsedMinutes;
        item.progressPercent = progressPercent;
        item.estimatedCompletionAt = Model::FormatLocalTimestamp(entry.finishTime);
        result.push_back(std::move(item));
    }

    return result;
}

void OrderService::CatchUpProduction() {
    auto schedule = BuildProductionSchedule();
    if (schedule.empty()) {
        return;
    }

    auto orders = store_.LoadOrders();
    auto samples = store_.LoadSamples();

    const std::time_t now = std::time(nullptr);

    bool ordersChanged = false;
    bool samplesChanged = false;

    for (auto& entry : schedule) {
        if (entry.finishTime > now) {
            // 단일 FIFO 라인: 이 주문이 아직 안 끝났으면 뒤 순서 주문들도 아직 끝나지 않았다고 판단하고 중단.
            break;
        }

        auto oIt = std::find_if(orders.begin(), orders.end(), [&](const Model::Order& o) {
            return o.orderNo == entry.order.orderNo;
        });
        if (oIt == orders.end()) {
            continue;
        }

        // 생산 완료 처리: PRODUCING -> CONFIRMED.
        // 재고에는 실 생산량(수율 반영 총 생산량)을 더하고, 이 주문이 확정(CONFIRMED)되며
        // 소비하는 주문 수량만큼을 차감한다(즉시 CONFIRMED 경로의 "재고 차감"과 동일한 회계 처리 —
        // 그렇지 않으면 이 주문의 수요가 재고에서 빠지지 않아 재고가 이중으로 잡혀 다른 주문이
        // 실제로는 존재하지 않는 재고에 대해 이중 배정될 수 있다).
        auto sIt = std::find_if(samples.begin(), samples.end(), [&](const Model::Sample& s) {
            return s.id == oIt->sampleId;
        });
        if (sIt != samples.end()) {
            sIt->stock += oIt->actualProductionQty - oIt->quantity;
            samplesChanged = true;
        }

        oIt->status = Model::OrderStatus::CONFIRMED;
        oIt->updatedAt = Model::FormatLocalTimestamp(entry.finishTime);
        ordersChanged = true;
    }

    if (ordersChanged) {
        store_.SaveOrders(orders);
    }
    if (samplesChanged) {
        store_.SaveSamples(samples);
    }
}

} // namespace Monitor
