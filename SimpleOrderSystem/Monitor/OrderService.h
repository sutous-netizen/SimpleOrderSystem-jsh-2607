#pragma once
// 도메인 규칙(재고 계산, 상태 전이, 생산 큐 FIFO)의 단일 진실 공급원.
// docs/05-주문승인거절.md, docs/06-모니터링.md, docs/07-생산라인.md 기준.
// console/dummy 에이전트는 이 서비스를 호출만 하고 로직을 재구현하지 않는다.

#include <vector>
#include <string>
#include <cstdint>
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
    Persistence::IDataStore& store_;
};

} // namespace Monitor
