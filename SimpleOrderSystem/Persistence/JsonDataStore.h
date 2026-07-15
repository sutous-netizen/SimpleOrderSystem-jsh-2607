#pragma once
// JSON 파일 기반 IDataStore 구현체.
// 외부 라이브러리 없이 최소한의 JSON 파서/라이터를 내부에 캡슐화한다.
// console/monitor/dummy 에이전트는 IDataStore 인터페이스를 통해서만 이 클래스를 사용해야 한다.

#include <string>
#include <vector>
#include <optional>
#include "IDataStore.h"
#include "../Model/Types.h"

namespace Persistence {

class JsonDataStore final : public IDataStore {
public:
    // samplesPath, ordersPath : 시료/주문 JSON 파일 경로.
    // 파일이 존재하지 않으면 빈 배열로 취급한다(최초 실행 대비).
    JsonDataStore(std::string samplesPath, std::string ordersPath);

    // ---- 시료 CRUD ----
    std::vector<Model::Sample> LoadSamples() const override;
    void SaveSamples(const std::vector<Model::Sample>& samples) override;
    std::optional<Model::Sample> FindSampleById(const std::string& sampleId) const override;

    // ---- 주문 CRUD ----
    std::vector<Model::Order> LoadOrders() const override;
    void SaveOrders(const std::vector<Model::Order>& orders) override;
    std::optional<Model::Order> FindOrderByNo(const std::string& orderNo) const override;

    // 다음 주문번호 채번: "ORD-YYYYMMDD-NNNN"
    std::string NextOrderNo(const std::string& yyyymmdd) override;

private:
    std::string samplesPath_;
    std::string ordersPath_;
};

} // namespace Persistence
