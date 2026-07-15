// JsonDataStore(Persistence) TDD 테스트.
// 실제 data/ 폴더를 건드리지 않도록 임시 디렉터리에 고유 파일명을 사용하고,
// 테스트 종료 시 생성한 파일을 정리한다.

#include "TestFramework.h"
#include "../Persistence/JsonDataStore.h"

#include <filesystem>
#include <cstdio>
#include <random>
#include <sstream>

namespace {

// 테스트마다 충돌하지 않도록 고유한 임시 파일 경로 쌍을 만든다.
struct TempPaths {
    std::string samplesPath;
    std::string ordersPath;

    TempPaths() {
        static std::mt19937_64 rng(std::random_device{}());
        std::ostringstream tag;
        tag << rng();
        std::filesystem::path dir = std::filesystem::temp_directory_path();
        samplesPath = (dir / ("persistence_test_samples_" + tag.str() + ".json")).string();
        ordersPath = (dir / ("persistence_test_orders_" + tag.str() + ".json")).string();
    }

    ~TempPaths() {
        std::error_code ec;
        std::filesystem::remove(samplesPath, ec);
        std::filesystem::remove(ordersPath, ec);
        // WriteFileTextAtomic 이 남길 수 있는 임시 파일도 함께 정리한다.
        std::filesystem::remove(samplesPath + ".tmp", ec);
        std::filesystem::remove(ordersPath + ".tmp", ec);
    }
};

} // namespace

TEST(JsonDataStore_Sample_저장후_로드하면_동일한_값) {
    TempPaths paths;
    Persistence::JsonDataStore store(paths.samplesPath, paths.ordersPath);

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
    ASSERT_EQ(loaded[0].id, s1.id);
    ASSERT_EQ(loaded[0].name, s1.name);
    ASSERT_TRUE(std::abs(loaded[0].avgProductionTimeMin - s1.avgProductionTimeMin) < 1e-9);
    ASSERT_TRUE(std::abs(loaded[0].yieldRate - s1.yieldRate) < 1e-9);
    ASSERT_EQ(loaded[0].stock, s1.stock);

    ASSERT_EQ(loaded[1].id, s2.id);
    ASSERT_EQ(loaded[1].name, s2.name);
    ASSERT_EQ(loaded[1].stock, s2.stock);

    auto found = store.FindSampleById("S-002");
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->name, s2.name);

    auto notFound = store.FindSampleById("S-999");
    ASSERT_TRUE(!notFound.has_value());
}

TEST(JsonDataStore_Order_저장후_로드하면_동일한_값_상태포함) {
    TempPaths paths;
    Persistence::JsonDataStore store(paths.samplesPath, paths.ordersPath);

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

    ASSERT_EQ(loaded[0].orderNo, o1.orderNo);
    ASSERT_EQ(loaded[0].sampleId, o1.sampleId);
    ASSERT_EQ(loaded[0].customerName, o1.customerName);
    ASSERT_EQ(loaded[0].quantity, o1.quantity);
    ASSERT_TRUE(loaded[0].status == Model::OrderStatus::PRODUCING);
    ASSERT_EQ(loaded[0].createdAt, o1.createdAt);
    ASSERT_EQ(loaded[0].updatedAt, o1.updatedAt);
    ASSERT_EQ(loaded[0].shortage, o1.shortage);
    ASSERT_EQ(loaded[0].actualProductionQty, o1.actualProductionQty);
    ASSERT_TRUE(std::abs(loaded[0].totalProductionTimeMin - o1.totalProductionTimeMin) < 1e-9);

    ASSERT_EQ(loaded[1].orderNo, o2.orderNo);
    ASSERT_TRUE(loaded[1].status == Model::OrderStatus::RELEASE);

    auto found = store.FindOrderByNo("ORD-20260715-0001");
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found->status == Model::OrderStatus::PRODUCING);

    auto notFound = store.FindOrderByNo("ORD-99999999-9999");
    ASSERT_TRUE(!notFound.has_value());
}

TEST(JsonDataStore_NextOrderNo_같은_날짜_다음_번호_반환) {
    TempPaths paths;
    Persistence::JsonDataStore store(paths.samplesPath, paths.ordersPath);

    // 주문이 하나도 없을 때는 0001 이어야 한다.
    std::string firstNo = store.NextOrderNo("20260715");
    ASSERT_EQ(firstNo, std::string("ORD-20260715-0001"));

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
    ASSERT_EQ(nextNo, std::string("ORD-20260715-0008"));

    std::string otherDateNo = store.NextOrderNo("20260714");
    ASSERT_EQ(otherDateNo, std::string("ORD-20260714-0100"));

    std::string freshDateNo = store.NextOrderNo("20260101");
    ASSERT_EQ(freshDateNo, std::string("ORD-20260101-0001"));
}

// "애플리케이션 재실행 후에도 데이터가 유지된다"(docs/01-개요.md) 요구사항을
// 실제 파일 기반으로 검증하는 통합 테스트.
// - 인스턴스 A로 저장 -> A를 스코프 종료로 소멸 -> 같은 경로로 인스턴스 B 생성(재실행 시뮬레이션)
//   -> B가 A의 데이터를 그대로 읽어오는지 확인한다.
TEST(JsonDataStore_재실행_시뮬레이션_저장한_시료와_주문이_그대로_유지) {
    TempPaths paths;

    Model::Sample savedSample;
    Model::Order savedOrder;

    {
        Persistence::JsonDataStore storeA(paths.samplesPath, paths.ordersPath);

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
    Persistence::JsonDataStore storeB(paths.samplesPath, paths.ordersPath);

    std::vector<Model::Sample> loadedSamples = storeB.LoadSamples();
    ASSERT_EQ(loadedSamples.size(), static_cast<size_t>(1));
    ASSERT_EQ(loadedSamples[0].id, savedSample.id);
    ASSERT_EQ(loadedSamples[0].name, savedSample.name);
    ASSERT_TRUE(std::abs(loadedSamples[0].avgProductionTimeMin - savedSample.avgProductionTimeMin) < 1e-9);
    ASSERT_TRUE(std::abs(loadedSamples[0].yieldRate - savedSample.yieldRate) < 1e-9);
    ASSERT_EQ(loadedSamples[0].stock, savedSample.stock);

    std::vector<Model::Order> loadedOrders = storeB.LoadOrders();
    ASSERT_EQ(loadedOrders.size(), static_cast<size_t>(1));
    ASSERT_EQ(loadedOrders[0].orderNo, savedOrder.orderNo);
    ASSERT_EQ(loadedOrders[0].sampleId, savedOrder.sampleId);
    ASSERT_EQ(loadedOrders[0].customerName, savedOrder.customerName);
    ASSERT_EQ(loadedOrders[0].quantity, savedOrder.quantity);
    ASSERT_TRUE(loadedOrders[0].status == savedOrder.status);
    ASSERT_EQ(loadedOrders[0].createdAt, savedOrder.createdAt);
    ASSERT_EQ(loadedOrders[0].updatedAt, savedOrder.updatedAt);
    ASSERT_EQ(loadedOrders[0].shortage, savedOrder.shortage);
    ASSERT_EQ(loadedOrders[0].actualProductionQty, savedOrder.actualProductionQty);
    ASSERT_TRUE(std::abs(loadedOrders[0].totalProductionTimeMin - savedOrder.totalProductionTimeMin) < 1e-9);

    // FindXxx 인터페이스도 재실행 후 정상 동작해야 한다.
    auto foundSample = storeB.FindSampleById(savedSample.id);
    ASSERT_TRUE(foundSample.has_value());
    ASSERT_EQ(foundSample->stock, savedSample.stock);

    auto foundOrder = storeB.FindOrderByNo(savedOrder.orderNo);
    ASSERT_TRUE(foundOrder.has_value());
    ASSERT_TRUE(foundOrder->status == savedOrder.status);
}
