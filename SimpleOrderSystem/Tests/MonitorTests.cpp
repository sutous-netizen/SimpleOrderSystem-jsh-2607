// Monitor::OrderService 도메인 규칙 gtest/gmock 테스트.
// docs/05-주문승인거절.md, docs/06-모니터링.md, docs/07-생산라인.md 기준.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Mocks/MockDataStore.h"
#include "../Monitor/OrderService.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <optional>
#include <sstream>
#include <vector>

namespace {

using ::testing::NiceMock;
using TestMocks::InMemoryDataStore;
using TestMocks::MockDataStore;

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

// 매 테스트마다 mock/fake 조합을 준비해주는 공용 픽스처.
class OrderServiceTest : public ::testing::Test {
protected:
    NiceMock<MockDataStore> mock;
    InMemoryDataStore fake;

    void SetUp() override {
        TestMocks::DelegateToFake(mock, fake);
    }
};

} // namespace

// 재고가 충분하면 승인 즉시 CONFIRMED로 전환되고 재고가 차감된다.
TEST_F(OrderServiceTest, ApproveWithSufficientStock_TransitionsToConfirmedAndDeductsStock) {
    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480) });

    Monitor::OrderService service(mock);
    Model::Order placed = service.PlaceOrder("S-001", "SK하이닉스", 150);
    ASSERT_TRUE(placed.status == Model::OrderStatus::RESERVED);

    Model::Order approved = service.ApproveOrder(placed.orderNo);

    EXPECT_TRUE(approved.status == Model::OrderStatus::CONFIRMED);
    EXPECT_EQ(approved.shortage, 0);
    EXPECT_EQ(approved.actualProductionQty, 0);

    auto sampleAfter = fake.FindSampleById("S-001");
    ASSERT_TRUE(sampleAfter.has_value());
    EXPECT_EQ(sampleAfter->stock, 330); // 480 - 150
}

// 재고가 부족하면 승인 시 PRODUCING으로 전환되고 실생산량이 ceil(부족분/수율)로 계산된다.
TEST_F(OrderServiceTest, ApproveWithInsufficientStock_TransitionsToProducingAndComputesActualQty) {
    // 주문 200, 재고 30 -> 부족분 170, 수율 0.92 -> ceil(170/0.92) = 185
    fake.SaveSamples({ MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 30) });

    Monitor::OrderService service(mock);
    Model::Order placed = service.PlaceOrder("S-002", "삼성전자 파운드리", 200);
    Model::Order approved = service.ApproveOrder(placed.orderNo);

    EXPECT_TRUE(approved.status == Model::OrderStatus::PRODUCING);
    EXPECT_EQ(approved.shortage, 170);
    EXPECT_EQ(approved.actualProductionQty, 185); // ceil(170/0.92) = 184.78.. -> 185

    // 총 생산 시간 = 평균 생산시간(30) * 실생산량(185) = 5550
    const double expectedTotal = 30.0 * 185.0;
    EXPECT_NEAR(approved.totalProductionTimeMin, expectedTotal, 0.0001);

    // 재고 부족 승인 시점에는 재고가 즉시 차감되지 않는다(생산 완료 시 반영).
    auto sampleAfter = fake.FindSampleById("S-002");
    EXPECT_EQ(sampleAfter->stock, 30);
}

// 주문을 거절하면 REJECTED로 전환된다.
TEST_F(OrderServiceTest, RejectOrder_TransitionsToRejected) {
    fake.SaveSamples({ MakeSample("S-003", "산화막 웨이퍼-SiO2", 10.0, 1.0, 0) });

    Monitor::OrderService service(mock);
    Model::Order placed = service.PlaceOrder("S-003", "LG이노텍", 300);
    Model::Order rejected = service.RejectOrder(placed.orderNo);

    EXPECT_TRUE(rejected.status == Model::OrderStatus::REJECTED);
}

// 상태별 주문 건수 집계에서 REJECTED는 제외된다.
TEST_F(OrderServiceTest, GetOrderCountsByStatus_ExcludesRejected) {
    fake.SaveSamples({
        MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 500),
        MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 30)
    });

    Monitor::OrderService service(mock);

    Model::Order o1 = service.PlaceOrder("S-001", "A사", 100);
    service.ApproveOrder(o1.orderNo); // 재고 충분 -> CONFIRMED

    Model::Order o2 = service.PlaceOrder("S-002", "B사", 200);
    service.ApproveOrder(o2.orderNo); // 재고 부족 -> PRODUCING

    Model::Order o3 = service.PlaceOrder("S-001", "C사", 50);
    service.RejectOrder(o3.orderNo); // -> REJECTED (모니터링 제외 대상)

    Model::Order o4 = service.PlaceOrder("S-001", "D사", 10); // RESERVED 유지
    (void)o4;

    auto counts = service.GetOrderCountsByStatus();

    int64_t reserved = -1, confirmed = -1, producing = -1, release = -1;
    for (auto& c : counts) {
        if (c.status == Model::OrderStatus::RESERVED) reserved = c.count;
        if (c.status == Model::OrderStatus::CONFIRMED) confirmed = c.count;
        if (c.status == Model::OrderStatus::PRODUCING) producing = c.count;
        if (c.status == Model::OrderStatus::RELEASE) release = c.count;
        EXPECT_TRUE(c.status != Model::OrderStatus::REJECTED);
    }

    EXPECT_EQ(reserved, 1);
    EXPECT_EQ(confirmed, 1);
    EXPECT_EQ(producing, 1);
    EXPECT_EQ(release, 0);
}

// 생산 완료 시각이 경과한 PRODUCING 주문은 CatchUpProduction 호출 시 CONFIRMED로 전환되고 재고가 반영된다.
TEST_F(OrderServiceTest, CatchUpProduction_CompletedOrder_TransitionsToConfirmedAndUpdatesStock) {
    // 부족분 50, 수율 1.0 -> 실생산량 50, 평균생산시간 1min -> 총생산시간 50min
    fake.SaveSamples({ MakeSample("S-004", "GaN 에피택셜-4인치", 1.0, 1.0, 0) });

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
    fake.SaveOrders({ order });

    Monitor::OrderService service(mock);
    service.CatchUpProduction();

    auto updatedOrder = fake.FindOrderByNo("ORD-TEST-0001");
    ASSERT_TRUE(updatedOrder.has_value());
    EXPECT_TRUE(updatedOrder->status == Model::OrderStatus::CONFIRMED);

    auto sampleAfter = fake.FindSampleById("S-004");
    ASSERT_TRUE(sampleAfter.has_value());
    EXPECT_EQ(sampleAfter->stock, 50); // 0 + 실생산량(50) 반영
}

// 생산 완료 시각이 아직 경과하지 않은 PRODUCING 주문은 CatchUpProduction 호출 후에도 상태가 유지된다.
TEST_F(OrderServiceTest, CatchUpProduction_IncompleteOrder_KeepsProducingStatus) {
    fake.SaveSamples({ MakeSample("S-005", "포토레지스트-PR7", 10.0, 1.0, 0) });

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
    fake.SaveOrders({ order });

    Monitor::OrderService service(mock);
    service.CatchUpProduction();

    auto updatedOrder = fake.FindOrderByNo("ORD-TEST-0002");
    ASSERT_TRUE(updatedOrder.has_value());
    EXPECT_TRUE(updatedOrder->status == Model::OrderStatus::PRODUCING);

    auto sampleAfter = fake.FindSampleById("S-005");
    EXPECT_EQ(sampleAfter->stock, 0);
}

// 시료별 재고 상태가 여유/부족/고갈로 올바르게 판정된다.
TEST_F(OrderServiceTest, GetInventoryStatus_ClassifiesAbundantShortageDepleted) {
    fake.SaveSamples({
        MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480), // 수요 없음 -> 여유
        MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 30), // 수요 200 > 30 -> 부족
        MakeSample("S-003", "산화막 웨이퍼-SiO2", 10.0, 1.0, 0)    // 재고 0 -> 고갈
    });

    Monitor::OrderService service(mock);
    Model::Order o = service.PlaceOrder("S-002", "삼성전자 파운드리", 200); // RESERVED 상태로 수요 반영
    (void)o;

    auto statuses = service.GetInventoryStatus();

    Model::StockState s1 = Model::StockState::DEPLETED, s2 = Model::StockState::DEPLETED, s3 = Model::StockState::ABUNDANT;
    for (auto& inv : statuses) {
        if (inv.sample.id == "S-001") s1 = inv.state;
        if (inv.sample.id == "S-002") s2 = inv.state;
        if (inv.sample.id == "S-003") s3 = inv.state;
    }

    EXPECT_TRUE(s1 == Model::StockState::ABUNDANT);
    EXPECT_TRUE(s2 == Model::StockState::SHORTAGE);
    EXPECT_TRUE(s3 == Model::StockState::DEPLETED);
}

// 생산 큐는 접수/승인 순서(FIFO)대로 반환된다.
TEST_F(OrderServiceTest, GetProductionQueue_ReturnsFifoOrder) {
    fake.SaveSamples({ MakeSample("S-002", "SiC 파워기판-6인치", 30.0, 0.92, 0) });

    Monitor::OrderService service(mock);

    Model::Order first = service.PlaceOrder("S-002", "A사", 100);
    service.ApproveOrder(first.orderNo);

    Model::Order second = service.PlaceOrder("S-002", "B사", 50);
    service.ApproveOrder(second.orderNo);

    auto queue = service.GetProductionQueue();
    ASSERT_EQ(static_cast<int>(queue.size()), 2);
    EXPECT_TRUE(queue[0].order.orderNo == first.orderNo);
    EXPECT_TRUE(queue[1].order.orderNo == second.orderNo);
}

// 생산 큐에 등록된 주문의 진행률과 경과 시간이 큐잉 시각 기준으로 올바르게 계산된다.
TEST_F(OrderServiceTest, GetProductionQueue_InProgressOrder_ComputesProgressAndElapsed) {
    fake.SaveSamples({ MakeSample("S-006", "테스트시료-진행중", 1.0, 1.0, 0) });

    Model::Order order;
    order.orderNo = "ORD-TEST-0003";
    order.sampleId = "S-006";
    order.customerName = "테스트고객3";
    order.quantity = 100;
    order.status = Model::OrderStatus::PRODUCING;
    order.shortage = 100;
    order.actualProductionQty = 100;
    order.totalProductionTimeMin = 100.0; // 100분 소요
    order.createdAt = PastString(50.0);
    order.updatedAt = PastString(50.0); // 50분 전 큐잉 -> 절반 진행
    fake.SaveOrders({ order });

    Monitor::OrderService service(mock);
    auto queue = service.GetProductionQueue();
    ASSERT_EQ(static_cast<int>(queue.size()), 1);

    EXPECT_TRUE(queue[0].progressPercent > 0.0);
    EXPECT_TRUE(queue[0].progressPercent < 100.0);
    EXPECT_NEAR(queue[0].progressPercent, 50.0, 5.0); // 약 50% 진행
    EXPECT_NEAR(queue[0].elapsedMinutes, 50.0, 1.0);
}

// 총 생산 시간이 이미 경과한 주문은 GetProductionQueue 조회 시 진행률이 100%로 표시된다.
TEST_F(OrderServiceTest, GetProductionQueue_TimeElapsed_ShowsFullProgress) {
    fake.SaveSamples({ MakeSample("S-007", "테스트시료-완료", 1.0, 1.0, 0) });

    Model::Order order;
    order.orderNo = "ORD-TEST-0004";
    order.sampleId = "S-007";
    order.customerName = "테스트고객4";
    order.quantity = 50;
    order.status = Model::OrderStatus::PRODUCING;
    order.shortage = 50;
    order.actualProductionQty = 50;
    order.totalProductionTimeMin = 50.0;
    order.createdAt = PastString(120.0);
    order.updatedAt = PastString(120.0); // 120분 전 큐잉 -> 총생산시간(50분) 이미 경과
    fake.SaveOrders({ order });

    Monitor::OrderService service(mock);
    // CatchUpProduction을 호출하지 않은 상태(=아직 PRODUCING)에서 GetProductionQueue만 호출했을 때의 값을 검증.
    auto queue = service.GetProductionQueue();
    ASSERT_EQ(static_cast<int>(queue.size()), 1);

    EXPECT_NEAR(queue[0].progressPercent, 100.0, 0.0001);
    EXPECT_NEAR(queue[0].elapsedMinutes, 50.0, 0.0001);
}

// FIFO 큐에서 두 번째 주문은 첫 주문이 완료되기 전까지 진행률 0%로 대기한다.
TEST_F(OrderServiceTest, GetProductionQueue_SecondOrderWaitsUntilFirstCompletes) {
    fake.SaveSamples({ MakeSample("S-008", "테스트시료-FIFO", 1.0, 1.0, 0) });

    Model::Order first;
    first.orderNo = "ORD-TEST-0005";
    first.sampleId = "S-008";
    first.customerName = "고객A";
    first.quantity = 100;
    first.status = Model::OrderStatus::PRODUCING;
    first.shortage = 100;
    first.actualProductionQty = 100;
    first.totalProductionTimeMin = 100.0; // 100분 소요, 아직 진행중
    first.createdAt = PastString(10.0);
    first.updatedAt = PastString(10.0); // 10분 전 큐잉

    Model::Order second;
    second.orderNo = "ORD-TEST-0006";
    second.sampleId = "S-008";
    second.customerName = "고객B";
    second.quantity = 50;
    second.status = Model::OrderStatus::PRODUCING;
    second.shortage = 50;
    second.actualProductionQty = 50;
    second.totalProductionTimeMin = 50.0;
    second.createdAt = PastString(5.0);
    second.updatedAt = PastString(5.0); // 첫 주문보다 늦게 큐잉

    fake.SaveOrders({ first, second });

    Monitor::OrderService service(mock);
    auto queue = service.GetProductionQueue();
    ASSERT_EQ(static_cast<int>(queue.size()), 2);
    EXPECT_TRUE(queue[0].order.orderNo == first.orderNo);
    EXPECT_TRUE(queue[1].order.orderNo == second.orderNo);

    EXPECT_TRUE(queue[0].progressPercent > 0.0); // 첫 주문(라인 선점)은 진행중
    EXPECT_NEAR(queue[1].progressPercent, 0.0, 0.0001); // 두번째 주문은 첫 주문이 끝날 때까지 대기(0%)
    EXPECT_NEAR(queue[1].elapsedMinutes, 0.0, 0.0001);
}

// CONFIRMED 주문은 출고 처리 시 RELEASE로 전환되고 출고 대상 목록에서 제외된다.
TEST_F(OrderServiceTest, ReleaseOrder_TransitionsFromConfirmedToRelease) {
    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 500) });

    Monitor::OrderService service(mock);
    Model::Order placed = service.PlaceOrder("S-001", "SK하이닉스", 150);
    service.ApproveOrder(placed.orderNo);

    Model::Order released = service.ReleaseOrder(placed.orderNo);
    EXPECT_TRUE(released.status == Model::OrderStatus::RELEASE);

    auto releasable = service.GetReleasableOrders();
    EXPECT_EQ(static_cast<int>(releasable.size()), 0);
}

// 주문 승인 시 변경된 주문 목록이 실제로 저장소에 저장 요청되는지(SaveOrders 호출) gmock으로 검증한다.
TEST(OrderServiceInteractionTest, ApproveOrder_InvokesSaveOrdersOnDataStore) {
    NiceMock<MockDataStore> mock;
    InMemoryDataStore fake;
    TestMocks::DelegateToFake(mock, fake);

    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480) });

    Monitor::OrderService service(mock);
    Model::Order placed = service.PlaceOrder("S-001", "SK하이닉스", 150);

    EXPECT_CALL(mock, SaveOrders(::testing::_)).Times(::testing::AtLeast(1));

    Model::Order approved = service.ApproveOrder(placed.orderNo);
    EXPECT_TRUE(approved.status == Model::OrderStatus::CONFIRMED);
}
