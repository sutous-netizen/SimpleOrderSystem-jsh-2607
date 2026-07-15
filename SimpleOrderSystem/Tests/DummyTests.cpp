// DummyDataGenerator TDD 테스트.
// 실제 파일 기반 저장소(JsonDataStore) 대신, 이 파일 안에서 인메모리 Fake IDataStore를
// 직접 구현해 사용한다(Dummy 에이전트는 저장 방식을 직접 다루지 않고 인터페이스만 소비).

#include "TestFramework.h"
#include "../Dummy/DummyDataGenerator.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace {

class FakeDataStore : public Persistence::IDataStore {
public:
    std::vector<Model::Sample> LoadSamples() const override {
        return samples_;
    }

    void SaveSamples(const std::vector<Model::Sample>& samples) override {
        samples_ = samples;
    }

    std::optional<Model::Sample> FindSampleById(const std::string& sampleId) const override {
        for (const auto& s : samples_) {
            if (s.id == sampleId) return s;
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
        for (const auto& o : orders_) {
            if (o.orderNo == orderNo) return o;
        }
        return std::nullopt;
    }

    std::string NextOrderNo(const std::string& yyyymmdd) override {
        int maxSeq = 0;
        for (const auto& o : orders_) {
            if (o.orderNo.size() < 4 + 8) continue;
            std::string datePart = o.orderNo.substr(4, 8);
            if (datePart != yyyymmdd) continue;
            size_t lastDash = o.orderNo.find_last_of('-');
            if (lastDash == std::string::npos) continue;
            try {
                int seq = std::stoi(o.orderNo.substr(lastDash + 1));
                maxSeq = std::max(maxSeq, seq);
            } catch (...) {
                // 무시
            }
        }
        std::ostringstream oss;
        oss << "ORD-" << yyyymmdd << "-";
        oss.width(4);
        oss.fill('0');
        oss << (maxSeq + 1);
        return oss.str();
    }

private:
    std::vector<Model::Sample> samples_;
    std::vector<Model::Order> orders_;
};

} // namespace

TEST(DummyGenerator_GenerateSamples_reset_true_이면_지정개수만큼_생성) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);

    generator.GenerateSamples(10, true);

    std::vector<Model::Sample> samples = store.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(10));

    for (const auto& s : samples) {
        ASSERT_TRUE(!s.id.empty());
        ASSERT_TRUE(!s.name.empty());
        ASSERT_TRUE(s.avgProductionTimeMin >= 0.2 && s.avgProductionTimeMin <= 1.0);
        ASSERT_TRUE(s.yieldRate >= 0.7 && s.yieldRate <= 0.99);
        ASSERT_TRUE(s.stock >= 0);
    }
}

TEST(DummyGenerator_GenerateSamples_재고_분포가_고갈_부족_여유로_섞여있다) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);

    generator.GenerateSamples(9, true);

    std::vector<Model::Sample> samples = store.LoadSamples();
    bool hasDepleted = false;   // stock == 0
    bool hasShortageish = false; // 소량 (1~50)
    bool hasAbundant = false;   // 대량 (100 이상)

    for (const auto& s : samples) {
        if (s.stock == 0) hasDepleted = true;
        else if (s.stock > 0 && s.stock <= 50) hasShortageish = true;
        else if (s.stock >= 100) hasAbundant = true;
    }

    ASSERT_TRUE(hasDepleted);
    ASSERT_TRUE(hasShortageish);
    ASSERT_TRUE(hasAbundant);
}

TEST(DummyGenerator_GenerateSamples_reset_false_이면_기존데이터에_추가) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);

    generator.GenerateSamples(3, true);
    ASSERT_EQ(store.LoadSamples().size(), static_cast<size_t>(3));

    generator.GenerateSamples(4, false);
    std::vector<Model::Sample> samples = store.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(7));

    // ID가 이어지는 순번을 사용하며 중복되지 않아야 한다.
    std::vector<std::string> ids;
    for (const auto& s : samples) ids.push_back(s.id);
    std::sort(ids.begin(), ids.end());
    ASSERT_TRUE(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
}

TEST(DummyGenerator_GenerateOrders_시료가_없으면_아무것도_생성하지_않는다) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);

    generator.GenerateOrders(3, true);

    ASSERT_EQ(store.LoadOrders().size(), static_cast<size_t>(0));
}

TEST(DummyGenerator_GenerateOrders_상태별로_지정건수만큼_생성) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);
    generator.GenerateSamples(5, true);

    const int countPerStatus = 4;
    generator.GenerateOrders(countPerStatus, true);

    std::vector<Model::Order> orders = store.LoadOrders();
    ASSERT_EQ(orders.size(), static_cast<size_t>(countPerStatus * 5));

    std::map<Model::OrderStatus, int> counts;
    for (const auto& o : orders) counts[o.status]++;

    ASSERT_EQ(counts[Model::OrderStatus::RESERVED], countPerStatus);
    ASSERT_EQ(counts[Model::OrderStatus::REJECTED], countPerStatus);
    ASSERT_EQ(counts[Model::OrderStatus::PRODUCING], countPerStatus);
    ASSERT_EQ(counts[Model::OrderStatus::CONFIRMED], countPerStatus);
    ASSERT_EQ(counts[Model::OrderStatus::RELEASE], countPerStatus);
}

TEST(DummyGenerator_GenerateOrders_PRODUCING_주문은_공식대로_계산된다) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);
    generator.GenerateSamples(5, true);

    generator.GenerateOrders(3, true);

    std::vector<Model::Sample> samples = store.LoadSamples();
    std::vector<Model::Order> orders = store.LoadOrders();

    bool checkedAny = false;
    for (const auto& o : orders) {
        if (o.status != Model::OrderStatus::PRODUCING) continue;
        checkedAny = true;

        auto sampleIt = std::find_if(samples.begin(), samples.end(),
            [&](const Model::Sample& s) { return s.id == o.sampleId; });
        ASSERT_TRUE(sampleIt != samples.end());

        // 부족분 = 주문수량 - 재고
        int64_t expectedShortage = o.quantity - sampleIt->stock;
        ASSERT_EQ(o.shortage, expectedShortage);
        ASSERT_TRUE(o.shortage > 0); // PRODUCING이므로 반드시 재고가 부족해야 한다.

        // 실 생산량 = ceil(부족분 / 수율)
        int64_t expectedQty = static_cast<int64_t>(
            std::ceil(static_cast<double>(expectedShortage) / sampleIt->yieldRate));
        ASSERT_EQ(o.actualProductionQty, expectedQty);

        // 총 생산 시간 = 평균 생산시간 * 실 생산량
        double expectedTime = sampleIt->avgProductionTimeMin * static_cast<double>(expectedQty);
        ASSERT_TRUE(std::abs(o.totalProductionTimeMin - expectedTime) < 1e-6);
    }
    ASSERT_TRUE(checkedAny);
}

TEST(DummyGenerator_GenerateOrders_reset_false_이면_기존주문에_누적) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);
    generator.GenerateSamples(5, true);

    generator.GenerateOrders(2, true);
    ASSERT_EQ(store.LoadOrders().size(), static_cast<size_t>(2 * 5));

    generator.GenerateOrders(1, false);
    ASSERT_EQ(store.LoadOrders().size(), static_cast<size_t>(2 * 5 + 1 * 5));
}

TEST(DummyGenerator_GenerateOrders_주문번호는_고유하다) {
    FakeDataStore store;
    Dummy::DummyDataGenerator generator(store);
    generator.GenerateSamples(5, true);
    generator.GenerateOrders(5, true);

    std::vector<Model::Order> orders = store.LoadOrders();
    std::vector<std::string> orderNos;
    for (const auto& o : orders) orderNos.push_back(o.orderNo);
    std::sort(orderNos.begin(), orderNos.end());
    ASSERT_TRUE(std::adjacent_find(orderNos.begin(), orderNos.end()) == orderNos.end());
}
