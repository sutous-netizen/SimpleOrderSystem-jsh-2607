#pragma once
// 테스트 전용 IDataStore 목(mock) + 상태를 실제로 들고 있는 인메모리 가짜(fake) 구현.
// "Delegating Calls to a Fake"(gmock cookbook) 패턴을 사용한다:
//   NiceMock<MockDataStore>가 받은 모든 호출을 InMemoryDataStore로 위임하여
//   상태 기반 테스트(저장/조회 왕복)를 그대로 지원하면서, 필요한 곳에서는
//   EXPECT_CALL로 실제 상호작용(예: "승인 시 SaveOrders가 호출되는가")도 검증할 수 있다.
//
// persistent/monitor/dummy 에이전트의 테스트가 공통으로 사용한다.

#include <gmock/gmock.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "../../Model/Types.h"
#include "../../Persistence/IDataStore.h"

namespace TestMocks {

class MockDataStore : public Persistence::IDataStore {
public:
    MOCK_METHOD(std::vector<Model::Sample>, LoadSamples, (), (const, override));
    MOCK_METHOD(void, SaveSamples, (const std::vector<Model::Sample>&), (override));
    MOCK_METHOD(std::optional<Model::Sample>, FindSampleById, (const std::string&), (const, override));
    MOCK_METHOD(std::vector<Model::Order>, LoadOrders, (), (const, override));
    MOCK_METHOD(void, SaveOrders, (const std::vector<Model::Order>&), (override));
    MOCK_METHOD(std::optional<Model::Order>, FindOrderByNo, (const std::string&), (const, override));
    MOCK_METHOD(std::string, NextOrderNo, (const std::string&), (override));
};

// 상태를 실제로 보관하는 단순한 IDataStore 구현(위임 대상).
class InMemoryDataStore : public Persistence::IDataStore {
public:
    std::vector<Model::Sample> LoadSamples() const override {
        return samples_;
    }

    void SaveSamples(const std::vector<Model::Sample>& samples) override {
        samples_ = samples;
    }

    std::optional<Model::Sample> FindSampleById(const std::string& sampleId) const override {
        for (const auto& sample : samples_) {
            if (sample.id == sampleId) {
                return sample;
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
        for (const auto& order : orders_) {
            if (order.orderNo == orderNo) {
                return order;
            }
        }
        return std::nullopt;
    }

    std::string NextOrderNo(const std::string& yyyymmdd) override {
        int maxSeq = 0;
        for (const auto& order : orders_) {
            if (order.orderNo.size() < 4 + 8 || order.orderNo.substr(4, 8) != yyyymmdd) {
                continue;
            }
            const size_t lastDash = order.orderNo.find_last_of('-');
            if (lastDash == std::string::npos) {
                continue;
            }
            try {
                maxSeq = std::max(maxSeq, std::stoi(order.orderNo.substr(lastDash + 1)));
            } catch (...) {
                // 형식이 다른 주문번호는 무시한다.
            }
        }
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "ORD-%s-%04d", yyyymmdd.c_str(), maxSeq + 1);
        return buffer;
    }

private:
    std::vector<Model::Sample> samples_;
    std::vector<Model::Order> orders_;
};

// mock으로 들어오는 모든 호출을 fake로 위임한다.
inline void DelegateToFake(::testing::NiceMock<MockDataStore>& mock, InMemoryDataStore& fake) {
    using ::testing::_;
    using ::testing::Invoke;

    ON_CALL(mock, LoadSamples()).WillByDefault(Invoke(&fake, &InMemoryDataStore::LoadSamples));
    ON_CALL(mock, SaveSamples(_)).WillByDefault(Invoke(&fake, &InMemoryDataStore::SaveSamples));
    ON_CALL(mock, FindSampleById(_)).WillByDefault(Invoke(&fake, &InMemoryDataStore::FindSampleById));
    ON_CALL(mock, LoadOrders()).WillByDefault(Invoke(&fake, &InMemoryDataStore::LoadOrders));
    ON_CALL(mock, SaveOrders(_)).WillByDefault(Invoke(&fake, &InMemoryDataStore::SaveOrders));
    ON_CALL(mock, FindOrderByNo(_)).WillByDefault(Invoke(&fake, &InMemoryDataStore::FindOrderByNo));
    ON_CALL(mock, NextOrderNo(_)).WillByDefault(Invoke(&fake, &InMemoryDataStore::NextOrderNo));
}

} // namespace TestMocks
