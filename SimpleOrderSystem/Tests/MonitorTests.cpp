// Monitor::OrderService 도메인 규칙 TDD 테스트.
// docs/05-주문승인거절.md, docs/06-모니터링.md, docs/07-생산라인.md 기준.

#include "TestFramework.h"
#include "../Monitor/OrderService.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <optional>
#include <sstream>
#include <vector>

namespace {

// 테스트 전용 인메모리 IDataStore 구현체.
class InMemoryDataStore : public Persistence::IDataStore {
public:
    std::vector<Model::Sample> LoadSamples() const override {
        return samples_;
    }

    void SaveSamples(const std::vector<Model::Sample>& samples) override {
        samples_ = samples;
    }

    std::optional<Model::Sample> FindSampleById(const std::string& sampleId) const override {
        for (auto& s : samples_) {
            if (s.id == sampleId) {
                return s;
            }
        }
        return std::nullopt;
    }

    std::vector<Model::Order> LoadOrders() const override {
        return orders_;
    }

    void SaveOrders(const std::vector<Model::Order>& orders) override {
        orders_ = orders;
    }

    std::optional<Model::Order> FindOrderByNo(const std::string& orderNo) const override {
        for (auto& o : orders_) {
            if (o.orderNo == orderNo) {
                return o;
            }
        }
        return std::nullopt;
    }

    std::string NextOrderNo(const std::string& yyyymmdd) override {
        ++counter_;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "ORD-%s-%04d", yyyymmdd.c_str(), counter_);
        return buf;
    }

    // ---- 테스트 헬퍼 ----
    std::vector<Model::Sample> samples_;
    std::vector<Model::Order> orders_;
    int counter_ = 0;
};

std::string FormatLocal(std::time_t t) {
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
        tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
        tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

std::string NowString() {
    return FormatLocal(std::time(nullptr));
}

std::string PastString(double minutesAgo) {
    std::time_t t = std::time(nullptr) - static_cast<std::time_t>(minutesAgo * 60.0);
    return FormatLocal(t);
}

Model::Sample MakeSample(const std::string& id, const std::string& name, double avgMin, double yieldRate, int64_t stock) {
    Model::Sample s;
    s.id = id;
    s.name = name;
    s.avgProductionTimeMin = avgMin;
    s.yieldRate = yieldRate;
    s.stock = stock;
    return s;
}

} // namespace

TEST(주문승인_재고충분시_CONFIRMED_전환및재고차감) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480));

    Monitor::OrderService service(store);
    Model::Order placed = service.PlaceOrder("S-001", "SK하이닉스", 150);
    ASSERT_TRUE(placed.status == Model::OrderStatus::RESERVED);

    Model::Order approved = service.ApproveOrder(placed.orderNo);

    ASSERT_TRUE(approved.status == Model::OrderStatus::CONFIRMED);
    ASSERT_EQ(approved.shortage, 0);
    ASSERT_EQ(approved.actualProductionQty, 0);

    auto sampleAfter = store.FindSampleById("S-001");
    ASSERT_TRUE(sampleAfter.has_value());
    ASSERT_EQ(sampleAfter->stock, 330); // 480 - 150
}

TEST(주문승인_재고부족시_PRODUCING_전환및실생산량_계산) {
    InMemoryDataStore store;
    // 주문 200, 재고 30 -> 부족분 170, 수율 0.92 -> ceil(170/0.92) = 185
    store.samples_.push_back(MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 30));

    Monitor::OrderService service(store);
    Model::Order placed = service.PlaceOrder("S-002", "삼성전자 파운드리", 200);
    Model::Order approved = service.ApproveOrder(placed.orderNo);

    ASSERT_TRUE(approved.status == Model::OrderStatus::PRODUCING);
    ASSERT_EQ(approved.shortage, 170);
    ASSERT_EQ(approved.actualProductionQty, 185); // ceil(170/0.92) = 184.78.. -> 185

    // 총 생산 시간 = 평균 생산시간(30) * 실생산량(185) = 5550
    double expectedTotal = 30.0 * 185.0;
    ASSERT_TRUE(std::abs(approved.totalProductionTimeMin - expectedTotal) < 0.0001);

    // 재고 부족 승인 시점에는 재고가 즉시 차감되지 않는다(생산 완료 시 반영).
    auto sampleAfter = store.FindSampleById("S-002");
    ASSERT_EQ(sampleAfter->stock, 30);
}

TEST(주문거절시_REJECTED_전환) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-003", "산화막 웨이퍼-SiO2", 10.0, 1.0, 0));

    Monitor::OrderService service(store);
    Model::Order placed = service.PlaceOrder("S-003", "LG이노텍", 300);
    Model::Order rejected = service.RejectOrder(placed.orderNo);

    ASSERT_TRUE(rejected.status == Model::OrderStatus::REJECTED);
}

TEST(상태별_주문건수_REJECTED_제외) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 500));
    store.samples_.push_back(MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 30));

    Monitor::OrderService service(store);

    Model::Order o1 = service.PlaceOrder("S-001", "A사", 100);
    service.ApproveOrder(o1.orderNo); // 재고 충분 -> CONFIRMED

    Model::Order o2 = service.PlaceOrder("S-002", "B사", 200);
    service.ApproveOrder(o2.orderNo); // 재고 부족 -> PRODUCING

    Model::Order o3 = service.PlaceOrder("S-001", "C사", 50);
    service.RejectOrder(o3.orderNo); // -> REJECTED (모니터링 제외 대상)

    Model::Order o4 = service.PlaceOrder("S-001", "D사", 10); // RESERVED 유지

    auto counts = service.GetOrderCountsByStatus();

    int64_t reserved = -1, confirmed = -1, producing = -1, release = -1;
    for (auto& c : counts) {
        if (c.status == Model::OrderStatus::RESERVED) reserved = c.count;
        if (c.status == Model::OrderStatus::CONFIRMED) confirmed = c.count;
        if (c.status == Model::OrderStatus::PRODUCING) producing = c.count;
        if (c.status == Model::OrderStatus::RELEASE) release = c.count;
        ASSERT_TRUE(c.status != Model::OrderStatus::REJECTED);
    }

    ASSERT_EQ(reserved, 1);
    ASSERT_EQ(confirmed, 1);
    ASSERT_EQ(producing, 1);
    ASSERT_EQ(release, 0);
}

TEST(생산완료_시간경과시_CatchUpProduction으로_CONFIRMED_전환및재고반영) {
    InMemoryDataStore store;
    // 부족분 50, 수율 1.0 -> 실생산량 50, 평균생산시간 1min -> 총생산시간 50min
    store.samples_.push_back(MakeSample("S-004", "GaN 에피택셜-4인치", 1.0, 1.0, 0));

    Model::Order order;
    order.orderNo = "ORD-TEST-0001";
    order.sampleId = "S-004";
    order.customerName = "테스트고객";
    order.quantity = 50;
    order.status = Model::OrderStatus::PRODUCING;
    order.shortage = 50;
    order.actualProductionQty = 50;
    order.totalProductionTimeMin = 50.0;
    order.createdAt = PastString(120.0);
    order.updatedAt = PastString(120.0); // 120분 전 큐잉 -> 총생산시간(50분) 이미 경과
    store.orders_.push_back(order);

    Monitor::OrderService service(store);
    service.CatchUpProduction();

    auto updatedOrder = store.FindOrderByNo("ORD-TEST-0001");
    ASSERT_TRUE(updatedOrder.has_value());
    ASSERT_TRUE(updatedOrder->status == Model::OrderStatus::CONFIRMED);

    auto sampleAfter = store.FindSampleById("S-004");
    ASSERT_TRUE(sampleAfter.has_value());
    ASSERT_EQ(sampleAfter->stock, 50); // 0 + 실생산량(50) 반영
}

TEST(생산미완료_시간미경과시_CatchUpProduction으로_상태유지) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-005", "포토레지스트-PR7", 10.0, 1.0, 0));

    Model::Order order;
    order.orderNo = "ORD-TEST-0002";
    order.sampleId = "S-005";
    order.customerName = "테스트고객2";
    order.quantity = 40;
    order.status = Model::OrderStatus::PRODUCING;
    order.shortage = 40;
    order.actualProductionQty = 40;
    order.totalProductionTimeMin = 400.0; // 400분 소요
    order.createdAt = NowString();
    order.updatedAt = NowString(); // 방금 큐잉 -> 아직 완료 안됨
    store.orders_.push_back(order);

    Monitor::OrderService service(store);
    service.CatchUpProduction();

    auto updatedOrder = store.FindOrderByNo("ORD-TEST-0002");
    ASSERT_TRUE(updatedOrder.has_value());
    ASSERT_TRUE(updatedOrder->status == Model::OrderStatus::PRODUCING);

    auto sampleAfter = store.FindSampleById("S-005");
    ASSERT_EQ(sampleAfter->stock, 0);
}

TEST(재고현황_여유부족고갈_판정) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480)); // 수요 없음 -> 여유
    store.samples_.push_back(MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 30));  // 수요 200 > 30 -> 부족
    store.samples_.push_back(MakeSample("S-003", "산화막 웨이퍼-SiO2", 10.0, 1.0, 0));    // 재고 0 -> 고갈

    Monitor::OrderService service(store);
    Model::Order o = service.PlaceOrder("S-002", "삼성전자 파운드리", 200); // RESERVED 상태로 수요 반영

    auto statuses = service.GetInventoryStatus();

    Model::StockState s1 = Model::StockState::DEPLETED, s2 = Model::StockState::DEPLETED, s3 = Model::StockState::ABUNDANT;
    for (auto& inv : statuses) {
        if (inv.sample.id == "S-001") s1 = inv.state;
        if (inv.sample.id == "S-002") s2 = inv.state;
        if (inv.sample.id == "S-003") s3 = inv.state;
    }

    ASSERT_TRUE(s1 == Model::StockState::ABUNDANT);
    ASSERT_TRUE(s2 == Model::StockState::SHORTAGE);
    ASSERT_TRUE(s3 == Model::StockState::DEPLETED);
}

TEST(생산큐_FIFO_순서_반환) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 0));

    Monitor::OrderService service(store);

    Model::Order first = service.PlaceOrder("S-002", "A사", 100);
    service.ApproveOrder(first.orderNo);

    Model::Order second = service.PlaceOrder("S-002", "B사", 50);
    service.ApproveOrder(second.orderNo);

    auto queue = service.GetProductionQueue();
    ASSERT_EQ(static_cast<int>(queue.size()), 2);
    ASSERT_TRUE(queue[0].order.orderNo == first.orderNo);
    ASSERT_TRUE(queue[1].order.orderNo == second.orderNo);
}

TEST(출고처리_CONFIRMED에서_RELEASE로_전환) {
    InMemoryDataStore store;
    store.samples_.push_back(MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 500));

    Monitor::OrderService service(store);
    Model::Order placed = service.PlaceOrder("S-001", "SK하이닉스", 150);
    service.ApproveOrder(placed.orderNo);

    Model::Order released = service.ReleaseOrder(placed.orderNo);
    ASSERT_TRUE(released.status == Model::OrderStatus::RELEASE);

    auto releasable = service.GetReleasableOrders();
    ASSERT_EQ(static_cast<int>(releasable.size()), 0);
}
