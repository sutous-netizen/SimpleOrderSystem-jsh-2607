// 통합 테스트: mock 없이 실제 Persistence::JsonDataStore(파일 기반) + 실제
// Monitor::OrderService 를 그대로 조합해 여러 계층이 실제로 맞물려 동작하는지 검증한다.
// (기존 Tests/MonitorTests.cpp 는 Tests/Mocks/MockDataStore.h 로 IDataStore 를 격리한
//  단위 테스트였다 — 이 파일은 그 반대로 실제 파일 I/O 경로까지 함께 검증한다.)
//
// Phase 8에서 고친 재고 이중 배정 버그(Monitor/OrderService.cpp 의 CatchUpProduction 이
// "생산완료 재고를 actualProductionQty 만 더하고 주문수량을 차감하지 않던 문제")를 전제로,
// 수정된 공식 stock += actualProductionQty - quantity 가 실제 파일 기반 시나리오에서도
// 정확히 반영되는지 확인하는 것이 이 파일의 핵심 목적이다.

#include <gtest/gtest.h>

#include "../../../Persistence/JsonDataStore.h"
#include "../../../Monitor/OrderService.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <optional>
#include <random>
#include <sstream>
#include <vector>

namespace {

// "YYYY-MM-DD HH:MM:SS" 형식의 로컬 타임스탬프 문자열(OrderService.cpp 의 FormatTime 과 동일 포맷).
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

// 지금으로부터 minutesAgo 분 전 시각 문자열. 생산이 이미 끝난 것처럼 updatedAt 을
// 과거로 조작할 때 사용한다(Tests/MonitorTests.cpp 의 PastString 헬퍼와 동일한 역할).
std::string PastString(double minutesAgo) {
    std::time_t t = std::time(nullptr) - static_cast<std::time_t>(minutesAgo * 60.0);
    return FormatLocal(t);
}

Model::Sample MakeSample(const std::string& id, const std::string& name,
    double avgMin, double yieldRate, int64_t stock) {
    Model::Sample s;
    s.id = id;
    s.name = name;
    s.avgProductionTimeMin = avgMin;
    s.yieldRate = yieldRate;
    s.stock = stock;
    return s;
}

// store 에 저장된 주문들 중 orderNo 와 일치하는 주문의 updatedAt 을 강제로 과거 시각으로
// 바꿔 저장한다(생산 큐 등록 이후 실제 시간이 흘러 생산이 완료된 상황을 파일 기반으로 재현).
void ForceUpdatedAtToPast(Persistence::IDataStore& store, const std::string& orderNo, double minutesAgo) {
    auto orders = store.LoadOrders();
    auto it = std::find_if(orders.begin(), orders.end(),
        [&](const Model::Order& o) { return o.orderNo == orderNo; });
    ASSERT_TRUE(it != orders.end()) << "대상 주문을 찾을 수 없습니다: " << orderNo;
    it->updatedAt = PastString(minutesAgo);
    store.SaveOrders(orders);
}

// 테스트마다 충돌하지 않도록 고유한 임시 파일 경로 쌍을 만들고,
// 테스트 종료 시(TearDown) 관련 파일을 모두 정리하는 픽스처.
// (Tests/PersistenceTests.cpp 의 픽스처 패턴을 그대로 재사용)
class OrderLifecycleIntegrationTest : public ::testing::Test {
protected:
    std::string samplesPath;
    std::string ordersPath;

    void SetUp() override {
        static std::mt19937_64 rng(std::random_device{}());
        std::ostringstream tag;
        tag << rng();
        std::filesystem::path dir = std::filesystem::temp_directory_path();
        samplesPath = (dir / ("monitor_integration_samples_" + tag.str() + ".json")).string();
        ordersPath = (dir / ("monitor_integration_orders_" + tag.str() + ".json")).string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(samplesPath, ec);
        std::filesystem::remove(ordersPath, ec);
        std::filesystem::remove(samplesPath + ".tmp", ec);
        std::filesystem::remove(ordersPath + ".tmp", ec);
    }
};

} // namespace

// 시나리오 1: 재고가 충분한 시료에 주문 → 승인(즉시 CONFIRMED) → 출고(RELEASE)까지
// 실제 JsonDataStore + OrderService 조합으로 처리한 뒤, 완전히 새로운 JsonDataStore
// 인스턴스로 같은 파일을 다시 열어(재실행 시뮬레이션) 최종 상태(RELEASE, 재고 차감)가
// 파일에 그대로 영속화됐는지 검증한다.
TEST_F(OrderLifecycleIntegrationTest, PlaceApproveConfirmedRelease_PersistsAcrossReload) {
    {
        Persistence::JsonDataStore store(samplesPath, ordersPath);
        store.SaveSamples(std::vector<Model::Sample>{
            MakeSample("S-100", "재고충분시료", 10.0, 1.0, 100) });

        Monitor::OrderService service(store);

        Model::Order placed = service.PlaceOrder("S-100", "고객A", 30);
        EXPECT_TRUE(placed.status == Model::OrderStatus::RESERVED);
        EXPECT_EQ(placed.quantity, 30);

        Model::Order approved = service.ApproveOrder(placed.orderNo);
        ASSERT_TRUE(approved.status == Model::OrderStatus::CONFIRMED);
        EXPECT_EQ(approved.shortage, 0);
        EXPECT_EQ(approved.actualProductionQty, 0);

        Model::Order released = service.ReleaseOrder(placed.orderNo);
        ASSERT_TRUE(released.status == Model::OrderStatus::RELEASE);

        // store 는 이 블록을 벗어나며 소멸된다(애플리케이션 종료를 흉내낸다).
    }

    // 같은 파일 경로로 완전히 새로운 JsonDataStore + OrderService 를 구성한다(재실행 시뮬레이션).
    Persistence::JsonDataStore reloadedStore(samplesPath, ordersPath);
    Monitor::OrderService reloadedService(reloadedStore);

    auto orders = reloadedStore.LoadOrders();
    ASSERT_EQ(orders.size(), static_cast<size_t>(1));
    EXPECT_TRUE(orders[0].status == Model::OrderStatus::RELEASE);
    EXPECT_EQ(orders[0].quantity, 30);

    auto sample = reloadedStore.FindSampleById("S-100");
    ASSERT_TRUE(sample.has_value());
    EXPECT_EQ(sample->stock, 70); // 100 - 30

    // GetOrderCountsByStatus 도 재실행 후 정상적으로 RELEASE 1건을 집계해야 한다.
    auto counts = reloadedService.GetOrderCountsByStatus();
    auto releaseCount = std::find_if(counts.begin(), counts.end(),
        [](const Monitor::OrderStatusCount& c) { return c.status == Model::OrderStatus::RELEASE; });
    ASSERT_TRUE(releaseCount != counts.end());
    EXPECT_EQ(releaseCount->count, 1);
}

// 시나리오 2: 재고가 부족한 시료에 주문 → 승인 시 PRODUCING 전환(부족분/실생산량/총생산시간
// 확인) → 생산 큐 등록 시각(updatedAt)을 이미 완료된 것처럼 과거로 조작해 저장 →
// CatchUpProduction() 호출 → CONFIRMED 전환과 함께 Phase 8에서 고친 공식대로
// (stock += actualProductionQty - quantity) 재고가 정확히 반영되는지, 새 JsonDataStore
// 인스턴스로 다시 읽어도 동일한지 검증한다.
TEST_F(OrderLifecycleIntegrationTest, ApproveProducing_CatchUpProduction_AppliesCorrectStockFormulaAcrossReload) {
    const int64_t initialStock = 10;
    const int64_t orderQty = 50;
    const double yieldRate = 0.5;
    const double avgProductionTimeMin = 8.0;

    // 부족분 = 50 - 10 = 40, 실생산량 = ceil(40 / 0.5) = 80, 총생산시간 = 8 * 80 = 640분
    const int64_t expectedShortage = 40;
    const int64_t expectedActualProductionQty = 80;
    const double expectedTotalProductionTimeMin = 640.0;

    std::string orderNo;
    {
        Persistence::JsonDataStore store(samplesPath, ordersPath);
        store.SaveSamples(std::vector<Model::Sample>{
            MakeSample("S-200", "재고부족시료", avgProductionTimeMin, yieldRate, initialStock) });

        Monitor::OrderService service(store);

        Model::Order placed = service.PlaceOrder("S-200", "고객B", orderQty);
        orderNo = placed.orderNo;

        Model::Order approved = service.ApproveOrder(orderNo);
        ASSERT_TRUE(approved.status == Model::OrderStatus::PRODUCING);
        EXPECT_EQ(approved.shortage, expectedShortage);
        EXPECT_EQ(approved.actualProductionQty, expectedActualProductionQty);
        EXPECT_TRUE(std::abs(approved.totalProductionTimeMin - expectedTotalProductionTimeMin) < 1e-9);

        // 총생산시간(640분)이 이미 지난 것처럼 큐잉 시각을 과거로 조작해 저장한다.
        ForceUpdatedAtToPast(store, orderNo, expectedTotalProductionTimeMin + 30.0);

        service.CatchUpProduction();

        auto orders = store.LoadOrders();
        auto it = std::find_if(orders.begin(), orders.end(),
            [&](const Model::Order& o) { return o.orderNo == orderNo; });
        ASSERT_TRUE(it != orders.end());
        EXPECT_TRUE(it->status == Model::OrderStatus::CONFIRMED);

        auto sample = store.FindSampleById("S-200");
        ASSERT_TRUE(sample.has_value());
        // stock = 기존재고 + 실생산량 - 주문수량 = 10 + 80 - 50 = 40
        EXPECT_EQ(sample->stock, initialStock + expectedActualProductionQty - orderQty);
    }

    // 완전히 새로운 JsonDataStore 인스턴스로 다시 읽어도 동일한 값이어야 한다.
    Persistence::JsonDataStore reloadedStore(samplesPath, ordersPath);

    auto reloadedOrder = reloadedStore.FindOrderByNo(orderNo);
    ASSERT_TRUE(reloadedOrder.has_value());
    EXPECT_TRUE(reloadedOrder->status == Model::OrderStatus::CONFIRMED);

    auto reloadedSample = reloadedStore.FindSampleById("S-200");
    ASSERT_TRUE(reloadedSample.has_value());
    EXPECT_EQ(reloadedSample->stock, initialStock + expectedActualProductionQty - orderQty);
}

// 시나리오 3(Phase 8 회귀 재현): 재고가 적은 시료에 대해 첫 번째 주문을 PRODUCING으로
// 승인하고 생산 완료(CatchUpProduction)까지 처리한 뒤, 그 다음 두 번째 주문을 승인할 때
// 실제 파일에 반영된 "남은 재고"를 정확히 기준으로 판단하는지 검증한다.
// (수정 전 버그: CatchUpProduction 이 stock += actualProductionQty 만 하고 quantity 를
//  차감하지 않아 재고가 실제보다 과다하게 잡혀, 두 번째 주문이 부족해야 함에도 잘못 CONFIRMED
//  되는 이중 배정이 발생했다.)
TEST_F(OrderLifecycleIntegrationTest, SequentialApprovals_AfterCatchUp_DoNotDoubleAssignStock) {
    const int64_t initialStock = 5;
    const double avgProductionTimeMin = 2.0;
    const double yieldRate = 1.0;

    Persistence::JsonDataStore store(samplesPath, ordersPath);
    store.SaveSamples(std::vector<Model::Sample>{
        MakeSample("S-300", "이중배정검증시료", avgProductionTimeMin, yieldRate, initialStock) });

    Monitor::OrderService service(store);

    // 첫 번째 주문: 재고(5) < 수량(20) -> PRODUCING.
    // 부족분 = 15, 실생산량 = ceil(15/1.0) = 15, 총생산시간 = 2 * 15 = 30분.
    Model::Order firstOrder = service.PlaceOrder("S-300", "고객C", 20);
    Model::Order firstApproved = service.ApproveOrder(firstOrder.orderNo);
    ASSERT_TRUE(firstApproved.status == Model::OrderStatus::PRODUCING);
    EXPECT_EQ(firstApproved.shortage, 15);
    EXPECT_EQ(firstApproved.actualProductionQty, 15);
    EXPECT_TRUE(std::abs(firstApproved.totalProductionTimeMin - 30.0) < 1e-9);

    // 첫 번째 주문의 생산이 이미 끝난 것처럼 조작 후 생산 완료 처리.
    ForceUpdatedAtToPast(store, firstOrder.orderNo, 60.0);
    service.CatchUpProduction();

    auto firstReloaded = store.FindOrderByNo(firstOrder.orderNo);
    ASSERT_TRUE(firstReloaded.has_value());
    EXPECT_TRUE(firstReloaded->status == Model::OrderStatus::CONFIRMED);

    // 수정된 공식: stock = 5 + 15 - 20 = 0.
    // (버그가 남아있다면 20을 차감하지 않아 stock = 20 이 되어 아래 두 번째 주문이
    //  잘못 CONFIRMED 되는 이중 배정이 재발한다.)
    auto sampleAfterFirst = store.FindSampleById("S-300");
    ASSERT_TRUE(sampleAfterFirst.has_value());
    EXPECT_EQ(sampleAfterFirst->stock, 0);

    // 두 번째 주문: 실제 남은 재고(0) < 수량(1) 이므로 PRODUCING 이어야 한다(CONFIRMED 로
    // 잘못 즉시 승인되면 이중 배정 버그가 재발한 것).
    Model::Order secondOrder = service.PlaceOrder("S-300", "고객D", 1);
    Model::Order secondApproved = service.ApproveOrder(secondOrder.orderNo);
    ASSERT_TRUE(secondApproved.status == Model::OrderStatus::PRODUCING);
    EXPECT_EQ(secondApproved.shortage, 1);
    EXPECT_EQ(secondApproved.actualProductionQty, 1);

    // 두 번째 주문도 생산 완료 처리 후 최종 재고를 검증한다: stock = 0 + 1 - 1 = 0.
    ForceUpdatedAtToPast(store, secondOrder.orderNo, 10.0);
    service.CatchUpProduction();

    auto secondReloaded = store.FindOrderByNo(secondOrder.orderNo);
    ASSERT_TRUE(secondReloaded.has_value());
    EXPECT_TRUE(secondReloaded->status == Model::OrderStatus::CONFIRMED);

    auto sampleAfterSecond = store.FindSampleById("S-300");
    ASSERT_TRUE(sampleAfterSecond.has_value());
    EXPECT_EQ(sampleAfterSecond->stock, 0);

    // 새 JsonDataStore 인스턴스로 다시 읽어도 동일한 최종 재고여야 한다.
    Persistence::JsonDataStore reloadedStore(samplesPath, ordersPath);
    auto reloadedSample = reloadedStore.FindSampleById("S-300");
    ASSERT_TRUE(reloadedSample.has_value());
    EXPECT_EQ(reloadedSample->stock, 0);
}
