#pragma once
// 테스트용 더미 데이터 생성 도구 (docs/09-미션및제출방법.md "Dummy 데이터 생성 Tool").
// 실제 도메인 규칙(수율/부족분/실생산량 계산)을 위반하지 않는 값만 생성하며,
// 데이터 저장은 IDataStore 인터페이스를 통해서만 수행한다.
// 프로덕션 코드와 분리된 테스트/도구용 코드다.

#include "../Model/Types.h"
#include "../Persistence/IDataStore.h"

namespace Dummy {

class DummyDataGenerator {
public:
    explicit DummyDataGenerator(Persistence::IDataStore& store);

    // 시료 더미 데이터를 count개 생성한다.
    // reset=true 이면 기존 시료를 모두 비우고 새로 채우고,
    // reset=false 이면 기존 시료에 이어서 추가한다(시료 ID는 이어지는 순번 사용).
    void GenerateSamples(int count, bool reset);

    // 주문 더미 데이터를 상태별로 countPerStatus건씩 생성한다
    // (RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE 총 5 * countPerStatus건).
    // reset=true 이면 기존 주문을 모두 비우고 새로 채우고,
    // reset=false 이면 기존 주문에 이어서 추가한다.
    // 시료가 하나도 등록되어 있지 않으면 아무 것도 생성하지 않는다.
    void GenerateOrders(int countPerStatus, bool reset);

    // 합리적인 기본값(시료 12개, 주문 상태별 3건씩)으로 데이터를 한 번에 채운다.
    // reset=true면 기존 데이터를 비우고 새로 채우고, false면 기존 데이터에 추가한다.
    // (시료는 reset 여부를 그대로 따르고, 주문은 방금 생성/유지된 시료 위에 항상 추가로 쌓는다.)
    void SeedDefaultDataset(bool reset);

private:
    Persistence::IDataStore& store_;
};

} // namespace Dummy
