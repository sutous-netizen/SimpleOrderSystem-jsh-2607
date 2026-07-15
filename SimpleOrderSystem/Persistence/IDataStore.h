#pragma once
// 영속성 계층 공용 인터페이스.
// persistent 에이전트가 구현체(JsonDataStore)를 제공하고,
// console/monitor/dummy 에이전트는 이 인터페이스만 바라본다.

#include <vector>
#include <string>
#include <optional>
#include "../Model/Types.h"

namespace Persistence {

class IDataStore {
public:
    virtual ~IDataStore() = default;

    // ---- 시료 CRUD ----
    virtual std::vector<Model::Sample> LoadSamples() const = 0;
    virtual void SaveSamples(const std::vector<Model::Sample>& samples) = 0;
    virtual std::optional<Model::Sample> FindSampleById(const std::string& sampleId) const = 0;

    // ---- 주문 CRUD ----
    virtual std::vector<Model::Order> LoadOrders() const = 0;
    virtual void SaveOrders(const std::vector<Model::Order>& orders) = 0;
    virtual std::optional<Model::Order> FindOrderByNo(const std::string& orderNo) const = 0;

    // 다음 주문번호 채번: "ORD-YYYYMMDD-NNNN" (docs/04-시료주문.md)
    virtual std::string NextOrderNo(const std::string& yyyymmdd) = 0;
};

} // namespace Persistence
