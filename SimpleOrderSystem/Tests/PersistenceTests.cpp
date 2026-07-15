// JsonDataStore(Persistence) 통합 테스트.
// 실제 data/ 폴더를 건드리지 않도록 임시 디렉터리에 고유 파일명을 사용하고,
// 테스트 종료 시 생성한 파일을 정리한다.
// 실제 Persistence::JsonDataStore(파일 기반 구현)를 그대로 검증하므로 mock 이 필요 없다.

#include <gtest/gtest.h>
#include "../Persistence/JsonDataStore.h"

#include <filesystem>
#include <cmath>
#include <random>
#include <sstream>

namespace {

// 테스트마다 충돌하지 않도록 고유한 임시 파일 경로 쌍을 만들고,
// 테스트 종료 시(TearDown) 관련 파일을 모두 정리하는 픽스처.
class JsonDataStoreTest : public ::testing::Test {
protected:
    std::string samplesPath;
    std::string ordersPath;

    void SetUp() override {
        static std::mt19937_64 rng(std::random_device{}());
        std::ostringstream tag;
        tag << rng();
        std::filesystem::path dir = std::filesystem::temp_directory_path();
        samplesPath = (dir / ("persistence_test_samples_" + tag.str() + ".json")).string();
        ordersPath = (dir / ("persistence_test_orders_" + tag.str() + ".json")).string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(samplesPath, ec);
        std::filesystem::remove(ordersPath, ec);
        // WriteFileTextAtomic 이 남길 수 있는 임시 파일도 함께 정리한다.
        std::filesystem::remove(samplesPath + ".tmp", ec);
        std::filesystem::remove(ordersPath + ".tmp", ec);
    }
};

} // namespace

// 시료를 저장한 뒤 다시 로드하면 동일한 값(특수문자 포함 이름 등)을 얻어야 한다.
TEST_F(JsonDataStoreTest, SaveThenLoadSamples_ReturnsIdenticalValues) {
    Persistence::JsonDataStore store(samplesPath, ordersPath);

    std::vector<Model::Sample> samples;
    Model::Sample s1;
    s1.id = "S-001";
    s1.name = "테스트시료A";
    s1.avgProductionTimeMin = 12.5;
    s1.yieldRate = 0.85;
    s1.stock = 100;
    samples.push_back(s1);

    Model::Sample s2;
    s2.id = "S-002";
    s2.name = "시료 \"특수문자\" 포함";
    s2.avgProductionTimeMin = 3.0;
    s2.yieldRate = 1.0;
    s2.stock = 0;
    samples.push_back(s2);

    store.SaveSamples(samples);

    std::vector<Model::Sample> loaded = store.LoadSamples();
    ASSERT_EQ(loaded.size(), samples.size());
    EXPECT_EQ(loaded[0].id, s1.id);
    EXPECT_EQ(loaded[0].name, s1.name);
    EXPECT_TRUE(std::abs(loaded[0].avgProductionTimeMin - s1.avgProductionTimeMin) < 1e-9);
    EXPECT_TRUE(std::abs(loaded[0].yieldRate - s1.yieldRate) < 1e-9);
    EXPECT_EQ(loaded[0].stock, s1.stock);

    EXPECT_EQ(loaded[1].id, s2.id);
    EXPECT_EQ(loaded[1].name, s2.name);
    EXPECT_EQ(loaded[1].stock, s2.stock);

    auto found = store.FindSampleById("S-002");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, s2.name);

    auto notFound = store.FindSampleById("S-999");
    EXPECT_TRUE(!notFound.has_value());
}

// 주문을 저장한 뒤 다시 로드하면 상태(OrderStatus)를 포함해 동일한 값을 얻어야 한다.
TEST_F(JsonDataStoreTest, SaveThenLoadOrders_ReturnsIdenticalValuesIncludingStatus) {
    Persistence::JsonDataStore store(samplesPath, ordersPath);

    std::vector<Model::Order> orders;
    Model::Order o1;
    o1.orderNo = "ORD-20260715-0001";
    o1.sampleId = "S-001";
    o1.customerName = "고객A";
    o1.quantity = 50;
    o1.status = Model::OrderStatus::PRODUCING;
    o1.createdAt = "2026-07-15 09:00:00";
    o1.updatedAt = "2026-07-15 09:30:00";
    o1.shortage = 20;
    o1.actualProductionQty = 24;
    o1.totalProductionTimeMin = 300.0;
    orders.push_back(o1);

    Model::Order o2;
    o2.orderNo = "ORD-20260715-0002";
    o2.sampleId = "S-002";
    o2.customerName = "고객B";
    o2.quantity = 10;
    o2.status = Model::OrderStatus::RELEASE;
    o2.createdAt = "2026-07-15 10:00:00";
    o2.updatedAt = "2026-07-15 12:00:00";
    orders.push_back(o2);

    store.SaveOrders(orders);

    std::vector<Model::Order> loaded = store.LoadOrders();
    ASSERT_EQ(loaded.size(), orders.size());

    EXPECT_EQ(loaded[0].orderNo, o1.orderNo);
    EXPECT_EQ(loaded[0].sampleId, o1.sampleId);
    EXPECT_EQ(loaded[0].customerName, o1.customerName);
    EXPECT_EQ(loaded[0].quantity, o1.quantity);
    EXPECT_TRUE(loaded[0].status == Model::OrderStatus::PRODUCING);
    EXPECT_EQ(loaded[0].createdAt, o1.createdAt);
    EXPECT_EQ(loaded[0].updatedAt, o1.updatedAt);
    EXPECT_EQ(loaded[0].shortage, o1.shortage);
    EXPECT_EQ(loaded[0].actualProductionQty, o1.actualProductionQty);
    EXPECT_TRUE(std::abs(loaded[0].totalProductionTimeMin - o1.totalProductionTimeMin) < 1e-9);

    EXPECT_EQ(loaded[1].orderNo, o2.orderNo);
    EXPECT_TRUE(loaded[1].status == Model::OrderStatus::RELEASE);

    auto found = store.FindOrderByNo("ORD-20260715-0001");
    ASSERT_TRUE(found.has_value());
    EXPECT_TRUE(found->status == Model::OrderStatus::PRODUCING);

    auto notFound = store.FindOrderByNo("ORD-99999999-9999");
    EXPECT_TRUE(!notFound.has_value());
}

// 같은 날짜 내에서는 다음 채번 번호를, 다른 날짜/신규 날짜는 각각 독립적으로 채번해야 한다.
TEST_F(JsonDataStoreTest, NextOrderNo_ReturnsNextSequenceForSameDateAndIsolatedForOtherDates) {
    Persistence::JsonDataStore store(samplesPath, ordersPath);

    // 주문이 하나도 없을 때는 0001 이어야 한다.
    std::string firstNo = store.NextOrderNo("20260715");
    EXPECT_EQ(firstNo, std::string("ORD-20260715-0001"));

    std::vector<Model::Order> orders;
    Model::Order o1;
    o1.orderNo = "ORD-20260715-0001";
    o1.sampleId = "S-001";
    o1.customerName = "고객A";
    o1.quantity = 1;
    orders.push_back(o1);

    Model::Order o2;
    o2.orderNo = "ORD-20260715-0007";
    o2.sampleId = "S-001";
    o2.customerName = "고객B";
    o2.quantity = 1;
    orders.push_back(o2);

    // 다른 날짜의 주문은 채번에 영향을 주지 않아야 한다.
    Model::Order o3;
    o3.orderNo = "ORD-20260714-0099";
    o3.sampleId = "S-001";
    o3.customerName = "고객C";
    o3.quantity = 1;
    orders.push_back(o3);

    store.SaveOrders(orders);

    std::string nextNo = store.NextOrderNo("20260715");
    EXPECT_EQ(nextNo, std::string("ORD-20260715-0008"));

    std::string otherDateNo = store.NextOrderNo("20260714");
    EXPECT_EQ(otherDateNo, std::string("ORD-20260714-0100"));

    std::string freshDateNo = store.NextOrderNo("20260101");
    EXPECT_EQ(freshDateNo, std::string("ORD-20260101-0001"));
}

// "애플리케이션 재실행 후에도 데이터가 유지된다"(docs/01-개요.md) 요구사항을
// 실제 파일 기반으로 검증하는 통합 테스트.
// - 인스턴스 A로 저장 -> A를 스코프 종료로 소멸 -> 같은 경로로 인스턴스 B 생성(재실행 시뮬레이션)
//   -> B가 A의 데이터를 그대로 읽어오는지 확인한다.
TEST_F(JsonDataStoreTest, RestartSimulation_PersistsSavedSampleAndOrderAcrossInstances) {
    Model::Sample savedSample;
    Model::Order savedOrder;

    {
        Persistence::JsonDataStore storeA(samplesPath, ordersPath);

        savedSample.id = "S-010";
        savedSample.name = "재실행테스트시료";
        savedSample.avgProductionTimeMin = 8.0;
        savedSample.yieldRate = 0.9;
        savedSample.stock = 35;
        storeA.SaveSamples(std::vector<Model::Sample>{ savedSample });

        savedOrder.orderNo = "ORD-20260715-0099";
        savedOrder.sampleId = savedSample.id;
        savedOrder.customerName = "재실행고객";
        savedOrder.quantity = 60;
        savedOrder.status = Model::OrderStatus::PRODUCING;
        savedOrder.createdAt = "2026-07-15 08:00:00";
        savedOrder.updatedAt = "2026-07-15 08:10:00";
        savedOrder.shortage = 25;
        savedOrder.actualProductionQty = 28;
        savedOrder.totalProductionTimeMin = 224.0;
        storeA.SaveOrders(std::vector<Model::Order>{ savedOrder });

        // storeA 는 이 블록을 벗어나며 소멸된다(애플리케이션 종료를 흉내낸다).
    }

    // 같은 파일 경로로 새 인스턴스 B 를 생성한다(애플리케이션 재실행을 흉내낸다).
    Persistence::JsonDataStore storeB(samplesPath, ordersPath);

    std::vector<Model::Sample> loadedSamples = storeB.LoadSamples();
    ASSERT_EQ(loadedSamples.size(), static_cast<size_t>(1));
    EXPECT_EQ(loadedSamples[0].id, savedSample.id);
    EXPECT_EQ(loadedSamples[0].name, savedSample.name);
    EXPECT_TRUE(std::abs(loadedSamples[0].avgProductionTimeMin - savedSample.avgProductionTimeMin) < 1e-9);
    EXPECT_TRUE(std::abs(loadedSamples[0].yieldRate - savedSample.yieldRate) < 1e-9);
    EXPECT_EQ(loadedSamples[0].stock, savedSample.stock);

    std::vector<Model::Order> loadedOrders = storeB.LoadOrders();
    ASSERT_EQ(loadedOrders.size(), static_cast<size_t>(1));
    EXPECT_EQ(loadedOrders[0].orderNo, savedOrder.orderNo);
    EXPECT_EQ(loadedOrders[0].sampleId, savedOrder.sampleId);
    EXPECT_EQ(loadedOrders[0].customerName, savedOrder.customerName);
    EXPECT_EQ(loadedOrders[0].quantity, savedOrder.quantity);
    EXPECT_TRUE(loadedOrders[0].status == savedOrder.status);
    EXPECT_EQ(loadedOrders[0].createdAt, savedOrder.createdAt);
    EXPECT_EQ(loadedOrders[0].updatedAt, savedOrder.updatedAt);
    EXPECT_EQ(loadedOrders[0].shortage, savedOrder.shortage);
    EXPECT_EQ(loadedOrders[0].actualProductionQty, savedOrder.actualProductionQty);
    EXPECT_TRUE(std::abs(loadedOrders[0].totalProductionTimeMin - savedOrder.totalProductionTimeMin) < 1e-9);

    // FindXxx 인터페이스도 재실행 후 정상 동작해야 한다.
    auto foundSample = storeB.FindSampleById(savedSample.id);
    ASSERT_TRUE(foundSample.has_value());
    EXPECT_EQ(foundSample->stock, savedSample.stock);

    auto foundOrder = storeB.FindOrderByNo(savedOrder.orderNo);
    ASSERT_TRUE(foundOrder.has_value());
    EXPECT_TRUE(foundOrder->status == savedOrder.status);
}
