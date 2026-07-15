# Phase 1 — 전체 뼈대 코드 구성

## 목표

`docs/01-개요.md` ~ `docs/08-출고처리.md` 기준으로 MVC 뼈대 코드를 4개 서브에이전트(console/monitor/persistent/dummy)에게 분배하여 구성한다. 이 단계에서는 컴파일 가능한 스켈레톤 + 핵심 도메인 규칙(재고 계산, 상태 전이, FIFO)의 최소 동작 구현 + 각 모듈 TDD 테스트를 목표로 한다.

## 공유 계약 (director가 정의, 변경 금지 — 변경 필요 시 director에게 문의)

- `Model/Types.h`, `Model/Types.cpp` — `OrderStatus`, `StockState`, `Sample`, `Order`
- `Persistence/IDataStore.h` — 영속성 인터페이스
- `Tests/TestFramework.h` — 자가등록 테스트 매크로(`TEST`, `ASSERT_TRUE`, `ASSERT_EQ`) 및 `TestFramework::RunAllTests()`

### Monitor::OrderService 계약 (monitor가 구현, console/dummy가 소비)

```cpp
namespace Monitor {

struct InventoryStatus {
    Model::Sample sample;
    Model::StockState state;
};

struct OrderStatusCount {
    Model::OrderStatus status;
    int64_t count;
};

struct ProductionQueueItem {
    Model::Order order;
    Model::Sample sample;
};

class OrderService {
public:
    explicit OrderService(Persistence::IDataStore& store);

    Model::Order PlaceOrder(const std::string& sampleId, const std::string& customerName, int64_t quantity);
    std::vector<Model::Order> GetPendingOrders() const; // RESERVED
    Model::Order ApproveOrder(const std::string& orderNo); // -> CONFIRMED or PRODUCING
    Model::Order RejectOrder(const std::string& orderNo);  // -> REJECTED
    Model::Order ReleaseOrder(const std::string& orderNo); // CONFIRMED -> RELEASE
    std::vector<Model::Order> GetReleasableOrders() const; // CONFIRMED

    std::vector<OrderStatusCount> GetOrderCountsByStatus() const; // REJECTED 제외
    std::vector<InventoryStatus> GetInventoryStatus() const;

    std::vector<ProductionQueueItem> GetProductionQueue() const; // FIFO 순
    void CatchUpProduction(); // 재실행 시 실제 경과 시간 반영하여 완료 처리
};

} // namespace Monitor
```

## 파일 배치

```
SimpleOrderSystem/
  Model/Types.h, Types.cpp                (director)
  Persistence/IDataStore.h                (director)
  Persistence/JsonDataStore.h, .cpp       (persistent)
  Monitor/OrderService.h, .cpp            (monitor)
  Monitor/InventoryMonitor.h, .cpp        (monitor, 내부 헬퍼— 선택)
  Monitor/ProductionQueue.h, .cpp         (monitor, 내부 헬퍼— 선택)
  Console/AppController.h, .cpp           (console)
  Console/MainMenuView.h, .cpp            (console)
  Console/SampleMenuView.h, .cpp          (console)
  Console/OrderMenuView.h, .cpp           (console)
  Console/MonitoringView.h, .cpp          (console)
  Console/ProductionLineView.h, .cpp      (console)
  Console/ReleaseView.h, .cpp             (console)
  Dummy/DummyDataGenerator.h, .cpp        (dummy)
  Tests/TestFramework.h                   (director)
  Tests/ModelTests.cpp                    (director)
  Tests/PersistenceTests.cpp              (persistent)
  Tests/MonitorTests.cpp                  (monitor)
  Tests/DummyTests.cpp                    (dummy)
  main.cpp                                (director, 최종 통합)
  data/samples.json, data/orders.json     (persistent, 런타임 생성)
```

## 계산 공식 (전 모듈 공통 — docs/01, docs/07)

- 부족분 = 주문 수량 - 현재 재고
- 실 생산량 = `ceil(부족분 / 수율)`
- 총 생산 시간 = 평균 생산시간 * 실 생산량
- 생산 완료 시 재고에는 실 생산량 전체가 반영된다(수율 반영한 총 생산량 기준, docs/01 추가요구사항).

## 통합 원칙

- 각 서브에이전트는 `SimpleOrderSystem.vcxproj` / `.vcxproj.filters`를 직접 수정하지 않는다 — director가 Phase 종료 시 일괄 등록한다(동시 편집 충돌 방지).
- 각 서브에이전트는 자신의 담당 폴더 + `Tests/<모듈>Tests.cpp` 파일만 작성한다.
- JSON 파싱/직렬화는 외부 라이브러리 없이 직접 구현한다(현재 스키마가 단순하고 vcxproj에 NuGet/vcpkg 미구성 상태).
- 데이터 파일 경로는 실행파일 기준 상대 경로 `data/samples.json`, `data/orders.json` 을 사용한다.

## 진행 기록

- [x] 공유 계약 정의 (director)
- [ ] persistent: JsonDataStore 스켈레톤
- [ ] monitor: OrderService 스켈레톤
- [ ] console: 메뉴 View/Controller 스켈레톤
- [ ] dummy: DummyDataGenerator 스켈레톤
- [ ] 통합: vcxproj/filters 등록, main.cpp 작성, 빌드/테스트 검증
- [ ] docs/temp 요약 및 commit
