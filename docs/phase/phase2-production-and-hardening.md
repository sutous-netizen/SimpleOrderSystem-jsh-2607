# Phase 2 — 생산라인 진행률/ETA, 통합 테스트 보강, 더미 데이터 시드 유틸리티

## 목표

Phase 1(뼈대) 완료 후 `docs/phase/phase1-skeleton.md`의 "확인된 이슈 / 후속 조치" 항목을 해소한다.

1. `docs/07-생산라인.md` 예시 화면의 "진행률 / 완료 예정 시각" 표시를 실제로 구현한다(Phase 1에서는 Order에 시작 시각 정보가 없어 생략했음).
2. Model 계층(`OrderStatus`/`StockState` 변환) 전용 단위 테스트와, persistent 계층의 "재실행 후 데이터 유지" 요구사항을 검증하는 실제 파일 기반 통합 테스트를 추가한다.
3. Dummy 데이터 생성기를 실제 실행 파일에서 커맨드라인 옵션으로 시드할 수 있게 연결한다(PoC 단계 도구가 실제 앱과 분리된 채로만 존재하던 문제 해소).

## 공유 계약 변경 (director가 정의, monitor가 구현, console이 소비)

`Monitor/OrderService.h`의 `ProductionQueueItem`에 필드 3개를 추가한다(기존 필드는 그대로 유지):

```cpp
struct ProductionQueueItem {
    Model::Order order;
    Model::Sample sample;
    double elapsedMinutes = 0.0;        // 이 주문의 생산 경과 시간(분). 아직 라인에 진입 전이면 0.
    double progressPercent = 0.0;       // 0~100 사이로 clamp된 진행률(%).
    std::string estimatedCompletionAt;  // "YYYY-MM-DD HH:MM:SS" 형식의 예상 완료 시각.
};
```

계산 방식: 단일 생산 라인 + FIFO 누적 스케줄링(Phase 1의 `CatchUpProduction`과 동일한 로직)을 사용한다.
- PRODUCING 주문을 큐잉 시각(`updatedAt`) 오름차순으로 정렬.
- `cumulativeStart = max(이전 주문의 completion, 이 주문의 큐잉 시각)`, `completion = cumulativeStart + totalProductionTimeMin`.
- `elapsedMinutes = clamp(now - (completion - totalProductionTimeMin), 0, totalProductionTimeMin)`.
- `progressPercent = elapsedMinutes / totalProductionTimeMin * 100` (totalProductionTimeMin이 0이면 100).
- `estimatedCompletionAt = completion을 "YYYY-MM-DD HH:MM:SS"로 포맷`.
- `CatchUpProduction`과 `GetProductionQueue`가 동일한 누적 스케줄링 계산을 공유하도록 내부 헬퍼로 추출한다(DRY, clean code 원칙).

## 에이전트별 작업

### monitor — `SimpleOrderSystem/Monitor/OrderService.h/.cpp`
- 위 계약대로 `ProductionQueueItem`을 확장하고 `GetProductionQueue()`가 값을 채워 반환하도록 구현.
- `CatchUpProduction()`과 스케줄링 계산 로직을 공유하는 내부 헬퍼(예: `private` 메서드)로 리팩토링(중복 제거).
- `Tests/MonitorTests.cpp`에 진행률/ETA 계산 테스트 2~3건 추가(진행 중인 케이스, 이미 완료 시각이 지난 케이스, 아직 대기 중이라 진행률 0%인 케이스).

### console — `SimpleOrderSystem/Console/ProductionLineView.h/.cpp`
- `GetProductionQueue()`가 반환하는 `elapsedMinutes`/`progressPercent`/`estimatedCompletionAt`을 그대로 표시(재계산 금지). `docs/07-생산라인.md` 예시처럼 "진행 72%  완료 예정 09:49" 형태로 표시.
- 다른 화면은 변경하지 않는다.
- **주의**: 이 시점에 monitor의 실제 파일이 아직 갱신 전일 수 있다. 위 계약(§공유 계약 변경)의 필드명/타입을 그대로 믿고 작성해라.

### persistent — `SimpleOrderSystem/Tests/ModelTests.cpp`(신규), `SimpleOrderSystem/Tests/PersistenceTests.cpp`(보강)
- `Tests/ModelTests.cpp` 신규 작성: `Model::ToString(OrderStatus)` / `Model::OrderStatusFromString` / `Model::ToString(StockState)` 왕복 변환 테스트, 잘못된 문자열 입력 시 예외 발생 테스트.
- `Tests/PersistenceTests.cpp`에 통합 테스트 1건 추가: 임시 경로에 `JsonDataStore` 인스턴스 A로 시료/주문 저장 → 인스턴스 A를 소멸시키고 같은 경로로 `JsonDataStore` 인스턴스 B를 새로 생성(재실행 시뮬레이션) → B가 A가 저장한 데이터를 그대로 읽어오는지 검증("재실행 후 데이터 유지" 요구사항 검증).
- 기존 `JsonDataStore` 구현은 계약 변경이 없으므로 수정하지 않는다(테스트만 추가).

### dummy — `SimpleOrderSystem/Dummy/DummyDataGenerator.h/.cpp`
- `void SeedDefaultDataset(Persistence::IDataStore& store, bool reset);` 함수를 추가한다(기존 `GenerateSamples`/`GenerateOrders`를 합리적인 기본값으로 호출하는 조합 함수 — 예: 시료 12개, 상태별 주문 3건씩).
- `Tests/DummyTests.cpp`에 `SeedDefaultDataset` 호출 후 시료/주문이 비어있지 않은지 검증하는 테스트 1건 추가.
- director가 `main.cpp`에서 커맨드라인 인자(`--seed`)로 이 함수를 호출하도록 연결할 예정이니, 헤더에 시그니처만 정확히 맞춰두면 된다.

## 통합 (director)

1. `main.cpp`: `argc`/`argv`에 `--seed`가 있으면 `Dummy::SeedDefaultDataset(store, /*reset=*/true)` 호출 후 콘솔 앱 실행.
2. `Tests/ModelTests.cpp` 신규 파일을 `SimpleOrderSystem.vcxproj`/`.vcxproj.filters`에 등록.
3. 4개 구성(Debug/Release × Win32/x64) 빌드 검증, Debug 실행 시 `_DEBUG` TC 전체 통과 확인.
4. `--seed` 옵션 실제 실행해 더미 데이터가 `data/` 파일에 반영되는지, 이어서 일반 실행 시 모니터링/생산라인 화면에 반영되는지 확인.
5. docs/temp에 Phase 2 요약 작성, commit.

## 진행 기록

- [x] 공유 계약(ProductionQueueItem 확장) 정의 (director)
- [ ] monitor: 진행률/ETA 계산 + 스케줄링 헬퍼 공유
- [ ] console: 생산라인 화면 진행률/ETA 표시
- [ ] persistent: ModelTests 신규 + 재실행 통합 테스트
- [ ] dummy: SeedDefaultDataset 추가
- [ ] 통합: main.cpp --seed 연결, vcxproj/filters 등록, 빌드/테스트/실행 검증
- [ ] docs/temp 요약 및 commit
