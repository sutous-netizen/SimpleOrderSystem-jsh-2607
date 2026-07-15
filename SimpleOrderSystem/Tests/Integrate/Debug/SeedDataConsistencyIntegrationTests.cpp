// 더미 생성 -> 파일 영속화 -> (별도 인스턴스의) 도메인 조회로 이어지는 지점을 검증하는 통합 테스트.
// mock 없이 실제 Persistence::JsonDataStore(임시 파일) + 실제 Dummy::DummyDataGenerator +
// 실제 Monitor::OrderService를 그대로 조합해서 사용한다.
//
// 시나리오: 인스턴스 A(JsonDataStore + DummyDataGenerator)로 임시 파일에
// SeedDefaultDataset(true)를 적용해 기본 더미 데이터셋(시료 12개, 상태별 주문 3건씩)을 채운 뒤,
// 같은 파일 경로를 가리키는 별도의 새 JsonDataStore 인스턴스 B와 새 OrderService로 다시 열어
// GetOrderCountsByStatus()/GetInventoryStatus() 결과가 시드된 데이터와 정확히 일치하는지 확인한다.

#include <gtest/gtest.h>

#include "../../../Persistence/JsonDataStore.h"
#include "../../../Dummy/DummyDataGenerator.h"
#include "../../../Monitor/OrderService.h"

#include <filesystem>
#include <random>
#include <sstream>
#include <unordered_map>

namespace {

// 테스트마다 충돌하지 않는 고유한 임시 파일 경로 쌍을 만들고,
// 테스트 종료 시(TearDown) 생성된 파일을 정리하는 픽스처.
// (SimpleOrderSystem/Tests/PersistenceTests.cpp의 임시 파일 픽스처 패턴을 그대로 따른다.)
class SeedDataConsistencyIntegrationTest : public ::testing::Test {
protected:
    std::string samplesPath;
    std::string ordersPath;

    void SetUp() override {
        static std::mt19937_64 rng(std::random_device{}());
        std::ostringstream tag;
        tag << rng();
        std::filesystem::path dir = std::filesystem::temp_directory_path();
        samplesPath = (dir / ("seed_consistency_samples_" + tag.str() + ".json")).string();
        ordersPath = (dir / ("seed_consistency_orders_" + tag.str() + ".json")).string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(samplesPath, ec);
        std::filesystem::remove(ordersPath, ec);
        // WriteFileTextAtomic이 남길 수 있는 임시 파일도 함께 정리한다.
        std::filesystem::remove(samplesPath + ".tmp", ec);
        std::filesystem::remove(ordersPath + ".tmp", ec);
    }

    // 인스턴스 A(store + generator)로 기본 더미 데이터셋을 실제 파일에 시드한다.
    void SeedDefaultDatasetIntoFile() {
        Persistence::JsonDataStore storeA(samplesPath, ordersPath);
        Dummy::DummyDataGenerator generator(storeA);
        generator.SeedDefaultDataset(/*reset=*/true);
    }
};

} // namespace

// SeedDefaultDataset으로 채운 뒤 별도 인스턴스의 OrderService::GetOrderCountsByStatus를 호출하면
// REJECTED를 제외한 각 상태(RESERVED/PRODUCING/CONFIRMED/RELEASE)가 정확히 3건씩 집계되어야 한다.
TEST_F(SeedDataConsistencyIntegrationTest, GetOrderCountsByStatus_MatchesSeededDistributionFromNewInstance) {
    SeedDefaultDatasetIntoFile();

    // 시드에 사용한 인스턴스 A와는 별개의, 같은 파일 경로를 바라보는 인스턴스 B + 새 OrderService.
    Persistence::JsonDataStore storeB(samplesPath, ordersPath);
    Monitor::OrderService orderService(storeB);

    const auto counts = orderService.GetOrderCountsByStatus();

    std::unordered_map<int, int64_t> countByStatus;
    for (const auto& item : counts) {
        countByStatus[static_cast<int>(item.status)] = item.count;
    }

    EXPECT_EQ(countByStatus[static_cast<int>(Model::OrderStatus::RESERVED)], 3);
    EXPECT_EQ(countByStatus[static_cast<int>(Model::OrderStatus::PRODUCING)], 3);
    EXPECT_EQ(countByStatus[static_cast<int>(Model::OrderStatus::CONFIRMED)], 3);
    EXPECT_EQ(countByStatus[static_cast<int>(Model::OrderStatus::RELEASE)], 3);

    // GetOrderCountsByStatus는 REJECTED를 결과에 포함하지 않는다(docs/06-모니터링.md).
    for (const auto& item : counts) {
        EXPECT_NE(item.status, Model::OrderStatus::REJECTED);
    }
}

// 원본 파일에는(REJECTED 포함) 5개 상태 x 3건 = 15건이 그대로 영속화되어 있어야 한다
// (모니터링 집계에서 제외되는 것과, 저장소에 실제로 남아있는 것은 별개임을 확인).
TEST_F(SeedDataConsistencyIntegrationTest, RawOrderStorage_ContainsAllFifteenSeededOrdersIncludingRejected) {
    SeedDefaultDatasetIntoFile();

    Persistence::JsonDataStore storeB(samplesPath, ordersPath);

    const auto orders = storeB.LoadOrders();
    ASSERT_EQ(orders.size(), static_cast<size_t>(15));

    std::unordered_map<int, int64_t> rawCountByStatus;
    for (const auto& order : orders) {
        rawCountByStatus[static_cast<int>(order.status)]++;
    }

    EXPECT_EQ(rawCountByStatus[static_cast<int>(Model::OrderStatus::RESERVED)], 3);
    EXPECT_EQ(rawCountByStatus[static_cast<int>(Model::OrderStatus::REJECTED)], 3);
    EXPECT_EQ(rawCountByStatus[static_cast<int>(Model::OrderStatus::PRODUCING)], 3);
    EXPECT_EQ(rawCountByStatus[static_cast<int>(Model::OrderStatus::CONFIRMED)], 3);
    EXPECT_EQ(rawCountByStatus[static_cast<int>(Model::OrderStatus::RELEASE)], 3);
}

// SeedDefaultDataset으로 채운 시료 12개는 GenerateSamples의 재고 분포 규칙(index % 3 == 0 -> 고갈)에
// 따라 정확히 4개가 재고 0(고갈)이어야 하고, 새 인스턴스의 OrderService::GetInventoryStatus는
// 그 4개를 예외 없이 StockState::DEPLETED로, 재고가 0 초과인 나머지 8개는 고갈이 아닌 상태로
// 정확히 분류해야 한다(더미 생성 -> 영속화 -> 도메인 조회가 실제로 맞물리는 지점).
TEST_F(SeedDataConsistencyIntegrationTest, GetInventoryStatus_ClassifiesSeededZeroStockSamplesAsDepleted) {
    SeedDefaultDatasetIntoFile();

    Persistence::JsonDataStore storeB(samplesPath, ordersPath);
    Monitor::OrderService orderService(storeB);

    const auto samples = storeB.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(12));

    const auto inventory = orderService.GetInventoryStatus();
    ASSERT_EQ(inventory.size(), static_cast<size_t>(12));

    int64_t depletedCount = 0;
    for (const auto& item : inventory) {
        if (item.sample.stock == 0) {
            EXPECT_EQ(item.state, Model::StockState::DEPLETED);
            ++depletedCount;
        } else {
            EXPECT_NE(item.state, Model::StockState::DEPLETED);
        }
    }

    // GenerateSamples는 index % 3 규칙으로 12개 중 4개(index 0,3,6,9)를 재고 0으로 만든다.
    EXPECT_EQ(depletedCount, 4);
}

// 시드에 사용한 인스턴스 A가 소멸된 뒤에도(재실행을 흉내낸 인스턴스 B가) 시료 목록/이름/수량 등이
// 그대로 이어져 보여야 한다 -- 더미 생성기가 만든 값이 유실/변형 없이 파일에 남아있는지 확인한다.
TEST_F(SeedDataConsistencyIntegrationTest, SeededSampleCount_PersistsAcrossNewStoreInstance) {
    SeedDefaultDatasetIntoFile();

    Persistence::JsonDataStore storeB(samplesPath, ordersPath);
    const auto samples = storeB.LoadSamples();

    ASSERT_EQ(samples.size(), static_cast<size_t>(12));
    for (const auto& sample : samples) {
        EXPECT_FALSE(sample.id.empty());
        EXPECT_FALSE(sample.name.empty());
        EXPECT_TRUE(sample.avgProductionTimeMin >= 0.2 && sample.avgProductionTimeMin <= 1.0);
        EXPECT_TRUE(sample.yieldRate >= 0.7 && sample.yieldRate <= 0.99);
        EXPECT_TRUE(sample.stock >= 0);
    }
}
