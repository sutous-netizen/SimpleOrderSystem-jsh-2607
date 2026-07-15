// Persistence + Monitor + Dummy 통합 테스트 (mock 없이 실제 컴포넌트 조합).
// - 실제 Persistence::JsonDataStore(임시 파일) + 실제 Monitor::OrderService +
//   실제 Dummy::DummyDataGenerator를 그대로 조합해, 재실행 시나리오와
//   더미 시드 + 도메인 조회가 실제로 맞물리는 지점을 검증한다.
// - Tests/PersistenceTests.cpp의 임시 파일 픽스처 패턴(고유 경로 생성 후 종료 시 정리)을 그대로 따른다.

#include <gtest/gtest.h>
#include "../../../Persistence/JsonDataStore.h"
#include "../../../Monitor/OrderService.h"
#include "../../../Dummy/DummyDataGenerator.h"

#include <filesystem>
#include <random>
#include <sstream>
#include <algorithm>

namespace {

// 테스트마다 충돌하지 않는 고유한 임시 파일 경로 쌍을 만들고,
// 테스트 종료 시(TearDown) 관련 파일을 모두 정리하는 픽스처.
// 실제 data/ 폴더는 절대 건드리지 않는다.
class RestartAndDummySeedIntegrationTest : public ::testing::Test {
protected:
    std::string samplesPath;
    std::string ordersPath;

    void SetUp() override {
        static std::mt19937_64 rng(std::random_device{}());
        std::ostringstream tag;
        tag << rng();
        std::filesystem::path dir = std::filesystem::temp_directory_path();
        samplesPath = (dir / ("integrate_restart_samples_" + tag.str() + ".json")).string();
        ordersPath = (dir / ("integrate_restart_orders_" + tag.str() + ".json")).string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(samplesPath, ec);
        std::filesystem::remove(ordersPath, ec);
        // JsonDataStore의 원자적 저장(WriteFileTextAtomic)이 남길 수 있는 임시 파일도 정리한다.
        std::filesystem::remove(samplesPath + ".tmp", ec);
        std::filesystem::remove(ordersPath + ".tmp", ec);
    }
};

} // namespace

// 시나리오 1: "재실행" 전체 흐름.
// 인스턴스 A(JsonDataStore) + 서비스 A(OrderService)로 시료 등록 -> 주문 접수(PlaceOrder)
// -> 승인(ApproveOrder, 재고 충분하여 즉시 CONFIRMED)까지 진행한 뒤,
// 스코프를 벗어나 인스턴스 A/서비스 A를 소멸시켜 애플리케이션 재실행을 흉내낸다.
// 이어서 같은 파일 경로로 인스턴스 B + 서비스 B를 새로 만들어 CONFIRMED 주문을 출고(ReleaseOrder)하고,
// 새 주문을 하나 더 접수한다.
// 최종적으로 파일에 저장된 전체 데이터(주문 개수, 상태, 재고)가 A와 B에 걸친 작업을 모두 정확히 반영하는지 검증한다.
TEST_F(RestartAndDummySeedIntegrationTest, RestartAcrossInstances_PersistsOrderLifecycleAndInventory) {
    const std::string sampleId = "S-100";
    const int64_t initialStock = 50;
    const int64_t firstOrderQty = 20;

    std::string firstOrderNo;

    {
        // ---- 인스턴스 A: 시료 등록 -> 주문 접수 -> 승인(CONFIRMED) ----
        Persistence::JsonDataStore storeA(samplesPath, ordersPath);
        Monitor::OrderService serviceA(storeA);

        Model::Sample sample;
        sample.id = sampleId;
        sample.name = "재실행통합테스트시료";
        sample.avgProductionTimeMin = 5.0;
        sample.yieldRate = 0.9;
        sample.stock = initialStock;
        storeA.SaveSamples(std::vector<Model::Sample>{ sample });

        Model::Order firstOrder = serviceA.PlaceOrder(sampleId, "고객A", firstOrderQty);
        firstOrderNo = firstOrder.orderNo;
        EXPECT_TRUE(firstOrder.status == Model::OrderStatus::RESERVED);

        Model::Order approved = serviceA.ApproveOrder(firstOrderNo);
        // 재고(50) >= 주문 수량(20) 이므로 즉시 CONFIRMED, 재고는 30으로 차감되어야 한다.
        EXPECT_TRUE(approved.status == Model::OrderStatus::CONFIRMED);

        // storeA/serviceA는 이 블록을 벗어나며 소멸된다(애플리케이션 종료를 흉내낸다).
    }

    // ---- "재실행" ----
    // 같은 파일 경로로 새 인스턴스 B + 새 서비스 B를 생성한다.
    Persistence::JsonDataStore storeB(samplesPath, ordersPath);
    Monitor::OrderService serviceB(storeB);

    // 재실행 직후 이전 상태(CONFIRMED, 재고 차감분)가 그대로 이어져야 한다.
    auto sampleAfterRestart = storeB.FindSampleById(sampleId);
    ASSERT_TRUE(sampleAfterRestart.has_value());
    EXPECT_EQ(sampleAfterRestart->stock, initialStock - firstOrderQty);

    auto firstOrderAfterRestart = storeB.FindOrderByNo(firstOrderNo);
    ASSERT_TRUE(firstOrderAfterRestart.has_value());
    EXPECT_TRUE(firstOrderAfterRestart->status == Model::OrderStatus::CONFIRMED);

    // 이어서 인스턴스 B에서 출고 처리를 수행한다.
    Model::Order released = serviceB.ReleaseOrder(firstOrderNo);
    EXPECT_TRUE(released.status == Model::OrderStatus::RELEASE);

    // 그리고 새 주문을 하나 더 접수한다.
    const int64_t secondOrderQty = 10;
    Model::Order secondOrder = serviceB.PlaceOrder(sampleId, "고객B", secondOrderQty);
    EXPECT_TRUE(secondOrder.status == Model::OrderStatus::RESERVED);
    EXPECT_NE(secondOrder.orderNo, firstOrderNo);

    // ---- 최종 검증: 파일에 저장된 전체 데이터가 A/B에 걸친 작업을 모두 정확히 반영해야 한다 ----
    // 검증은 반드시 "제3의" 인스턴스로 파일을 다시 읽어 확인한다(메모리 상태가 아닌 실제 저장 상태 검증).
    Persistence::JsonDataStore storeC(samplesPath, ordersPath);

    std::vector<Model::Order> allOrders = storeC.LoadOrders();
    ASSERT_EQ(allOrders.size(), static_cast<size_t>(2));

    auto findOrder = [&](const std::string& orderNo) -> const Model::Order* {
        for (auto& o : allOrders) {
            if (o.orderNo == orderNo) return &o;
        }
        return nullptr;
    };

    const Model::Order* persistedFirst = findOrder(firstOrderNo);
    ASSERT_NE(persistedFirst, nullptr);
    EXPECT_TRUE(persistedFirst->status == Model::OrderStatus::RELEASE);

    const Model::Order* persistedSecond = findOrder(secondOrder.orderNo);
    ASSERT_NE(persistedSecond, nullptr);
    EXPECT_TRUE(persistedSecond->status == Model::OrderStatus::RESERVED);
    EXPECT_EQ(persistedSecond->quantity, secondOrderQty);

    auto finalSample = storeC.FindSampleById(sampleId);
    ASSERT_TRUE(finalSample.has_value());
    // 출고(ReleaseOrder)는 재고에 영향을 주지 않으므로, 재고는 여전히 승인 시 차감된 값 그대로여야 한다.
    EXPECT_EQ(finalSample->stock, initialStock - firstOrderQty);
}

// 시나리오 2: 더미 시드 + 도메인 조회 통합.
// Dummy::DummyDataGenerator::SeedDefaultDataset(true)로 실제 파일에 데이터를 채운 뒤,
// 별도의 OrderService 인스턴스로 상태별 집계(GetOrderCountsByStatus)와
// 재고 현황(GetInventoryStatus)을 조회해 시드된 데이터와 일치하는지 검증한다.
TEST_F(RestartAndDummySeedIntegrationTest, SeedDefaultDataset_ThenQueryViaSeparateOrderService_MatchesSeededData) {
    {
        // ---- 더미 데이터 생성기로 실제 파일에 시드 ----
        Persistence::JsonDataStore seedStore(samplesPath, ordersPath);
        Dummy::DummyDataGenerator generator(seedStore);
        generator.SeedDefaultDataset(true);
        // seedStore/generator는 이 블록을 벗어나며 소멸된다.
    }

    // ---- 별도의 새 JsonDataStore + OrderService 인스턴스로 다시 열어 도메인 조회 ----
    Persistence::JsonDataStore queryStore(samplesPath, ordersPath);
    Monitor::OrderService queryService(queryStore);

    // 시료 12개가 전부 조회되어야 한다.
    std::vector<Monitor::InventoryStatus> inventory = queryService.GetInventoryStatus();
    EXPECT_EQ(inventory.size(), static_cast<size_t>(12));

    // 상태별 건수: RESERVED/CONFIRMED/PRODUCING/RELEASE 각 3건씩, REJECTED는 집계에서 제외되어야 한다.
    std::vector<Monitor::OrderStatusCount> counts = queryService.GetOrderCountsByStatus();
    ASSERT_EQ(counts.size(), static_cast<size_t>(4));

    auto findCount = [&](Model::OrderStatus status) -> int64_t {
        for (auto& c : counts) {
            if (c.status == status) return c.count;
        }
        return -1;
    };

    EXPECT_EQ(findCount(Model::OrderStatus::RESERVED), 3);
    EXPECT_EQ(findCount(Model::OrderStatus::CONFIRMED), 3);
    EXPECT_EQ(findCount(Model::OrderStatus::PRODUCING), 3);
    EXPECT_EQ(findCount(Model::OrderStatus::RELEASE), 3);

    // GetOrderCountsByStatus에 REJECTED 항목 자체가 포함되지 않아야 한다(모니터링에서 제외).
    bool hasRejectedEntry = std::any_of(counts.begin(), counts.end(), [](const Monitor::OrderStatusCount& c) {
        return c.status == Model::OrderStatus::REJECTED;
    });
    EXPECT_FALSE(hasRejectedEntry);

    // 원본 저장소를 직접 로드했을 때도 REJECTED 3건을 포함해 총 15건이 실제로 저장되어 있어야 한다
    // (더미 생성기 -> 파일 저장 -> 다른 인스턴스에서 도메인 조회로 이어지는 통합 지점 확인).
    std::vector<Model::Order> allOrders = queryStore.LoadOrders();
    EXPECT_EQ(allOrders.size(), static_cast<size_t>(15));

    int64_t rejectedCount = std::count_if(allOrders.begin(), allOrders.end(), [](const Model::Order& o) {
        return o.status == Model::OrderStatus::REJECTED;
    });
    EXPECT_EQ(rejectedCount, 3);
}
