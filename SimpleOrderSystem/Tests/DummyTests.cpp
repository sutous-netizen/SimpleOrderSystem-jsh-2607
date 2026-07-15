// DummyDataGenerator gtest/gmock 테스트.
// 저장소 접근은 공유 Mock(Tests/Mocks/MockDataStore.h)의
// NiceMock<MockDataStore> + InMemoryDataStore(fake) 위임 조합을 사용한다
// (Dummy 에이전트는 저장 방식을 직접 다루지 않고 IDataStore 인터페이스만 소비한다).

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Mocks/MockDataStore.h"
#include "../Dummy/DummyDataGenerator.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace {

// 각 테스트에서 공통으로 사용하는 mock+fake 조합을 준비하는 픽스처.
class DummyDataGeneratorTest : public ::testing::Test {
protected:
    ::testing::NiceMock<TestMocks::MockDataStore> mockStore;
    TestMocks::InMemoryDataStore fakeStore;

    void SetUp() override {
        TestMocks::DelegateToFake(mockStore, fakeStore);
    }
};

} // namespace

// GenerateSamples: reset=true이면 지정 개수만큼 시료가 생성되고, 각 값이 유효 범위 내에 있다.
TEST_F(DummyDataGeneratorTest, GenerateSamplesWithResetTrueCreatesExactCount) {
    Dummy::DummyDataGenerator generator(mockStore);

    generator.GenerateSamples(10, true);

    std::vector<Model::Sample> samples = fakeStore.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(10));

    for (const auto& sample : samples) {
        EXPECT_FALSE(sample.id.empty());
        EXPECT_FALSE(sample.name.empty());
        EXPECT_TRUE(sample.avgProductionTimeMin >= 0.2 && sample.avgProductionTimeMin <= 1.0);
        EXPECT_TRUE(sample.yieldRate >= 0.7 && sample.yieldRate <= 0.99);
        EXPECT_TRUE(sample.stock >= 0);
    }
}

// GenerateSamples: 재고 분포가 고갈/부족/여유 케이스를 골고루 포함한다.
TEST_F(DummyDataGeneratorTest, GenerateSamplesProducesMixedStockDistribution) {
    Dummy::DummyDataGenerator generator(mockStore);

    generator.GenerateSamples(9, true);

    std::vector<Model::Sample> samples = fakeStore.LoadSamples();
    bool hasDepleted = false;    // stock == 0
    bool hasShortageish = false; // 소량 (1~50)
    bool hasAbundant = false;    // 대량 (100 이상)

    for (const auto& sample : samples) {
        if (sample.stock == 0) {
            hasDepleted = true;
        } else if (sample.stock > 0 && sample.stock <= 50) {
            hasShortageish = true;
        } else if (sample.stock >= 100) {
            hasAbundant = true;
        }
    }

    EXPECT_TRUE(hasDepleted);
    EXPECT_TRUE(hasShortageish);
    EXPECT_TRUE(hasAbundant);
}

// GenerateSamples: reset=false이면 기존 데이터에 추가되고, ID는 중복 없이 이어진다.
TEST_F(DummyDataGeneratorTest, GenerateSamplesWithResetFalseAppendsToExisting) {
    Dummy::DummyDataGenerator generator(mockStore);

    generator.GenerateSamples(3, true);
    ASSERT_EQ(fakeStore.LoadSamples().size(), static_cast<size_t>(3));

    generator.GenerateSamples(4, false);
    std::vector<Model::Sample> samples = fakeStore.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(7));

    std::vector<std::string> ids;
    for (const auto& sample : samples) {
        ids.push_back(sample.id);
    }
    std::sort(ids.begin(), ids.end());
    EXPECT_TRUE(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
}

// GenerateOrders: 시료가 없으면 주문을 하나도 생성하지 않는다.
TEST_F(DummyDataGeneratorTest, GenerateOrdersWithNoSamplesCreatesNothing) {
    Dummy::DummyDataGenerator generator(mockStore);

    generator.GenerateOrders(3, true);

    EXPECT_EQ(fakeStore.LoadOrders().size(), static_cast<size_t>(0));
}

// GenerateOrders: 상태별로 지정 건수만큼 고르게 생성된다.
TEST_F(DummyDataGeneratorTest, GenerateOrdersCreatesEqualCountPerStatus) {
    Dummy::DummyDataGenerator generator(mockStore);
    generator.GenerateSamples(5, true);

    const int countPerStatus = 4;
    generator.GenerateOrders(countPerStatus, true);

    std::vector<Model::Order> orders = fakeStore.LoadOrders();
    ASSERT_EQ(orders.size(), static_cast<size_t>(countPerStatus * 5));

    std::map<Model::OrderStatus, int> counts;
    for (const auto& order : orders) {
        counts[order.status]++;
    }

    EXPECT_EQ(counts[Model::OrderStatus::RESERVED], countPerStatus);
    EXPECT_EQ(counts[Model::OrderStatus::REJECTED], countPerStatus);
    EXPECT_EQ(counts[Model::OrderStatus::PRODUCING], countPerStatus);
    EXPECT_EQ(counts[Model::OrderStatus::CONFIRMED], countPerStatus);
    EXPECT_EQ(counts[Model::OrderStatus::RELEASE], countPerStatus);
}

// GenerateOrders: PRODUCING 상태 주문은 부족분/실 생산량/총 생산시간 공식대로 계산된다.
TEST_F(DummyDataGeneratorTest, GenerateOrdersProducingFollowsProductionFormula) {
    Dummy::DummyDataGenerator generator(mockStore);
    generator.GenerateSamples(5, true);

    generator.GenerateOrders(3, true);

    std::vector<Model::Sample> samples = fakeStore.LoadSamples();
    std::vector<Model::Order> orders = fakeStore.LoadOrders();

    bool checkedAny = false;
    for (const auto& order : orders) {
        if (order.status != Model::OrderStatus::PRODUCING) {
            continue;
        }
        checkedAny = true;

        auto sampleIt = std::find_if(samples.begin(), samples.end(),
            [&](const Model::Sample& sample) { return sample.id == order.sampleId; });
        ASSERT_TRUE(sampleIt != samples.end());

        // 부족분 = 주문수량 - 재고
        const int64_t expectedShortage = order.quantity - sampleIt->stock;
        EXPECT_EQ(order.shortage, expectedShortage);
        EXPECT_TRUE(order.shortage > 0); // PRODUCING이므로 반드시 재고가 부족해야 한다.

        // 실 생산량 = ceil(부족분 / 수율)
        const int64_t expectedQty = static_cast<int64_t>(
            std::ceil(static_cast<double>(expectedShortage) / sampleIt->yieldRate));
        EXPECT_EQ(order.actualProductionQty, expectedQty);

        // 총 생산 시간 = 평균 생산시간 * 실 생산량
        const double expectedTime = sampleIt->avgProductionTimeMin * static_cast<double>(expectedQty);
        EXPECT_TRUE(std::abs(order.totalProductionTimeMin - expectedTime) < 1e-6);
    }
    EXPECT_TRUE(checkedAny);
}

// GenerateOrders: reset=false이면 기존 주문에 누적된다.
TEST_F(DummyDataGeneratorTest, GenerateOrdersWithResetFalseAccumulates) {
    Dummy::DummyDataGenerator generator(mockStore);
    generator.GenerateSamples(5, true);

    generator.GenerateOrders(2, true);
    ASSERT_EQ(fakeStore.LoadOrders().size(), static_cast<size_t>(2 * 5));

    generator.GenerateOrders(1, false);
    EXPECT_EQ(fakeStore.LoadOrders().size(), static_cast<size_t>(2 * 5 + 1 * 5));
}

// SeedDefaultDataset: 기본값으로 시료 12건, 상태별 3건씩(5상태 x 3건) 주문을 생성한다.
TEST_F(DummyDataGeneratorTest, SeedDefaultDatasetCreatesDefaultSamplesAndOrders) {
    Dummy::DummyDataGenerator generator(mockStore);

    generator.SeedDefaultDataset(true);

    std::vector<Model::Sample> samples = fakeStore.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(12));

    std::vector<Model::Order> orders = fakeStore.LoadOrders();
    ASSERT_EQ(orders.size(), static_cast<size_t>(5 * 3));

    std::map<Model::OrderStatus, int> counts;
    for (const auto& order : orders) {
        counts[order.status]++;
    }

    EXPECT_EQ(counts[Model::OrderStatus::RESERVED], 3);
    EXPECT_EQ(counts[Model::OrderStatus::REJECTED], 3);
    EXPECT_EQ(counts[Model::OrderStatus::PRODUCING], 3);
    EXPECT_EQ(counts[Model::OrderStatus::CONFIRMED], 3);
    EXPECT_EQ(counts[Model::OrderStatus::RELEASE], 3);
}

// GenerateOrders: 생성된 주문번호는 서로 중복되지 않는다.
TEST_F(DummyDataGeneratorTest, GenerateOrdersProducesUniqueOrderNumbers) {
    Dummy::DummyDataGenerator generator(mockStore);
    generator.GenerateSamples(5, true);
    generator.GenerateOrders(5, true);

    std::vector<Model::Order> orders = fakeStore.LoadOrders();
    std::vector<std::string> orderNos;
    for (const auto& order : orders) {
        orderNos.push_back(order.orderNo);
    }
    std::sort(orderNos.begin(), orderNos.end());
    EXPECT_TRUE(std::adjacent_find(orderNos.begin(), orderNos.end()) == orderNos.end());
}
