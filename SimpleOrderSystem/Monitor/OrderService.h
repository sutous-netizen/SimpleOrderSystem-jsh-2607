#pragma once
// 도메인 규칙(재고 계산, 상태 전이, 생산 큐 FIFO)의 단일 진실 공급원.
// docs/05-주문승인거절.md, docs/06-모니터링.md, docs/07-생산라인.md 기준.
// console/dummy 에이전트는 이 서비스를 호출만 하고 로직을 재구현하지 않는다.

#include <vector>
#include <string>
#include <cstdint>
#include <ctime>
#include "../Model/Types.h"
#include "../Persistence/IDataStore.h"

namespace Monitor {

struct InventoryStatus {
    Model::Sample sample;
    Model::StockState state;
};

struct OrderStatusCount {
    Model::OrderStatus status;
    int64_t count;
};

struct ProductionQueueItem {
    Model::Order order;
    Model::Sample sample;
    double elapsedMinutes = 0.0;        // 이 주문의 생산 경과 시간(분). 아직 라인에 진입 전이면 0.
    double progressPercent = 0.0;       // 0~100 사이로 clamp된 진행률(%).
    std::string estimatedCompletionAt;  // "YYYY-MM-DD HH:MM:SS" 형식의 예상 완료 시각.
};

class OrderService {
public:
    explicit OrderService(Persistence::IDataStore& store);

    Model::Order PlaceOrder(const std::string& sampleId, const std::string& customerName, int64_t quantity);
    std::vector<Model::Order> GetPendingOrders() const; // RESERVED 목록
    Model::Order ApproveOrder(const std::string& orderNo); // 재고 충분 -> CONFIRMED, 부족 -> PRODUCING(+생산큐 등록)
    Model::Order RejectOrder(const std::string& orderNo);  // -> REJECTED
    Model::Order ReleaseOrder(const std::string& orderNo); // CONFIRMED -> RELEASE
    std::vector<Model::Order> GetReleasableOrders() const; // CONFIRMED 목록

    std::vector<OrderStatusCount> GetOrderCountsByStatus() const; // REJECTED 제외한 상태별 건수
    std::vector<InventoryStatus> GetInventoryStatus() const; // 시료별 재고 + 여유/부족/고갈

    std::vector<ProductionQueueItem> GetProductionQueue() const; // PRODUCING 주문들을 FIFO(접수/등록 순)로 반환
    void CatchUpProduction(); // 프로그램 시작 시 1회 호출: 실제 경과 시간을 반영해 완료된 생산을 CONFIRMED로 전환하고 재고 반영

private:
    // 단일 생산 라인 + FIFO 누적 스케줄링 결과 한 건.
    // CatchUpProduction()과 GetProductionQueue()가 동일한 스케줄링 로직을 공유하기 위한 내부 표현.
    struct ScheduledProduction {
        Model::Order order;
        std::time_t startTime;   // 실제 생산 시작(라인 투입) 시각
        std::time_t finishTime;  // 생산 완료(예정/실제) 시각
    };

    // PRODUCING 주문들을 큐잉 시각(updatedAt) 오름차순으로 정렬한 뒤,
    // 단일 라인 누적 스케줄(cumulativeStart = max(이전 완료, 큐잉 시각), completion = start + 총생산시간)을 계산한다.
    std::vector<ScheduledProduction> BuildProductionSchedule() const;

    // 주문번호로 주문을 찾아 그 반복자를 반환한다. 존재하지 않으면 invalid_argument 예외를 던진다.
    // ApproveOrder/RejectOrder/ReleaseOrder에서 반복되는 "찾기 + 없으면 예외" 패턴을 공유한다.
    static std::vector<Model::Order>::iterator FindOrderOrThrow(std::vector<Model::Order>& orders, const std::string& orderNo);

    // 시료 ID로 시료를 조회한다. 존재하지 않으면 invalid_argument 예외를 던진다.
    Model::Sample FindSampleOrThrow(const std::string& sampleId) const;

    // 주문 상태가 기대한 상태와 다르면 logic_error 예외를 던진다.
    static void RequireStatus(const Model::Order& order, Model::OrderStatus expected, const std::string& errorMessage);

    Persistence::IDataStore& store_;
};

} // namespace Monitor
