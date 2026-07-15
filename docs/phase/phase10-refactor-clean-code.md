# Phase 10 — OOP/Clean Code 리팩토링 (superpowers 활용, TC 변경 없음)

## 목표

기능 추가나 버그 수정이 아니라, 지금까지 여러 Phase에 걸쳐 여러 에이전트가 작성한 코드를 OOP/clean code 관점에서 다시 훑어 리팩토링 요소를 발굴하고 고친다. **동작은 절대 바뀌면 안 되며(behavior-preserving refactor), 기존 TC(`Tests/` 전체, `Tests/Integrate/` 포함)는 수정하지 않는다** — 기존 테스트가 전부 그대로 통과하는 것이 이번 리팩토링이 안전했다는 증거다.

## 절차 (superpowers 플러그인 활용)

각 에이전트는 리팩토링 전에 `superpowers:systematic-debugging`이 아니라 **동작 보존 리팩토링**이 목적이므로, 다음 절차를 따른다(TDD의 "리팩토링" 단계와 동일한 정신):
1. 대상 파일을 다시 읽고 OOP/clean code 관점에서 문제를 찾는다(아래 "점검 관점" 참고).
2. 리팩토링 전, 관련 기존 테스트가 어떤 파일인지 파악한다(예: `Monitor/OrderService.cpp` → `Tests/MonitorTests.cpp`, `Tests/Integrate/Debug/OrderLifecycleIntegrationTests.cpp`).
3. 리팩토링을 적용한다(이름 변경, 중복 제거, 함수/클래스 추출, 매직 넘버 상수화 등) — **공개 인터페이스(헤더의 클래스/함수 시그니처)는 가능한 한 그대로 유지**해서 테스트 파일을 손대지 않고도 그대로 통과하게 한다. 만약 시그니처 변경이 불가피하면 그 이유를 보고에 명확히 남겨라(이 경우에도 테스트 파일 자체는 건드리지 말고, director에게 알려 판단을 맡겨라).
4. 컴파일러가 있으면 컴파일 후 관련 테스트를 실행해서 그대로 통과하는지 확인한다(없으면 self-review로 대체, 최종 검증은 director가 전체 빌드로 수행).

## 점검 관점 (OOP / Clean Code)

- **중복 제거(DRY)**: 같은 로직이 여러 파일에 반복되어 있는가?
- **단일 책임 원칙(SRP)**: 하나의 함수/클래스가 너무 많은 일을 하고 있는가?
- **매직 넘버/문자열**: 의미 있는 이름의 상수로 뽑아낼 수 있는 리터럴이 있는가?
- **캡슐화**: 익명 네임스페이스로 숨겨야 할 구현 세부사항이 공개되어 있지 않은가? 반대로 과도하게 숨겨서 재사용을 막고 있지 않은가?
- **명명**: 의도를 드러내지 못하는 이름이 있는가?
- **함수 길이/복잡도**: 너무 길거나 중첩이 깊은 함수를 더 작은 단위로 추출할 수 있는가?

## 이미 발견된 중복 (director가 사전 확인, 참고만 하라 — 강제 아님)

- `Console/ApprovalMenuView.cpp`와 `Console/ReleaseMenuView.cpp`에 완전히 동일한 `SampleNameOf(store, sampleId)` 헬퍼 함수가 각각 독립적으로 정의되어 있다(전형적인 DRY 위반).
- `Monitor/OrderService.cpp`(`FormatTime`)와 `Console/ConsoleUtil.cpp`(`LocalNow`/`NowTimeString`)가 "현재/특정 시각을 YYYY-MM-DD HH:MM:SS 문자열로 포맷"하는 로직을 각자 독립적으로 구현하고 있다 — 이건 계층(Monitor/Console)을 가로지르는 중복이라 director가 직접 처리한다(에이전트는 건드리지 않아도 된다).

## 에이전트별 작업 범위

### console — `Console/*.h`, `Console/*.cpp` (Tests 제외)
- `ApprovalMenuView.cpp`/`ReleaseMenuView.cpp`의 중복된 `SampleNameOf`를 `Console/ConsoleUtil.h/.cpp`의 공유 함수로 추출해 양쪽에서 재사용하도록 고쳐라.
- 그 외 View 파일들(`SampleMenuView`, `OrderMenuView`, `MonitoringView`, `ProductionLineView`, `AppController`)도 위 "점검 관점"으로 훑어 발견한 것이 있으면 고쳐라(예: 컬럼 폭 매직 넘버가 이미 상수화되어 있는지, 반복되는 출력 포맷 로직이 있는지).

### monitor — `Monitor/*.h`, `Monitor/*.cpp` (Tests 제외)
- `OrderService.cpp`를 점검 관점으로 훑어라. 특히 `ApproveOrder`가 CONFIRMED/PRODUCING 두 분기에서 각각 재고/상태 갱신을 처리하는데, 공통 부분(주문 상태 찾기, 시료 찾기 등)이 중복되어 있지 않은지, `BuildProductionSchedule`/`CatchUpProduction`/`GetProductionQueue` 사이에 리팩토링 여지가 있는지 확인해라.
- `ApproveOrder`/`RejectOrder`/`ReleaseOrder`에 반복되는 "주문번호로 찾기 + 존재하지 않으면 예외" 패턴이 있다면 private 헬퍼로 추출하는 것을 고려해라.

### persistent — `Persistence/*.h`, `Persistence/*.cpp` (Tests 제외)
- `JsonDataStore.cpp`를 점검 관점으로 훑어라. `SampleToJson`/`SampleFromJson`/`OrderToJson`/`OrderFromJson` 등 변환 함수들의 반복되는 패턴(필드별로 유사한 코드가 반복되는지), 매직 넘버(예: 버퍼 크기, 정밀도 등)를 확인해라.

### dummy — `Dummy/*.h`, `Dummy/*.cpp` (Tests 제외)
- `DummyDataGenerator.cpp`를 점검 관점으로 훑어라. 이름 풀/범위 상수들이 매직 넘버로 흩어져 있지 않은지, `GenerateSamples`/`GenerateOrders`/`SeedDefaultDataset` 사이에 중복되는 로직이 있는지 확인해라.

## 공통 지침

- **`Tests/` 디렉터리(하위 `Tests/Integrate/` 포함)는 어떤 파일도 수정하지 마라.** 리팩토링이 기존 테스트를 깨뜨린다면, 테스트를 고치지 말고 리팩토링 방식을 바꿔라(공개 동작을 보존하는 것이 최우선).
- vcxproj/.vcxproj.filters/main.cpp 수정 금지 — 새 파일을 추가했다면(예: 공유 헬퍼를 새 파일로 뽑았다면) director에게 보고하고, director가 등록한다.
- 과잉 리팩토링 금지(YAGNI) — 실제로 문제가 있는 부분만 고쳐라. "이렇게 하는 게 좀 더 낫지 않을까" 수준의 취향 변경은 하지 마라.
- **적용한 OOP/clean code 기법과 superpowers 활용 여부를 반드시 보고해라** — director가 docs/temp 요약에 반영한다.

## Director가 직접 할 일

1. `Monitor/OrderService.cpp`의 `FormatTime`과 `Console/ConsoleUtil.cpp`의 시간 포맷 로직 중복을 해소한다(계층을 가로지르므로 director가 처리). 공유 지점을 어디에 둘지(예: `Model/Types.h`에 유틸 함수 추가는 계층 원칙 위반이므로, 각자 유지하되 중복을 최소화하는 절충안을 택하거나, 별도 판단) 상황을 보고 결정한다.
2. 각 에이전트 산출물 통합, 신규 파일 있으면 vcxproj 등록.
3. 4개 구성 빌드, 기존 TC 전체(Debug) 통과 확인 — **테스트 파일을 전혀 건드리지 않았는데도 전부 그대로 통과해야 리팩토링이 안전했다는 뜻이다.**
4. docs/temp 요약, 커밋 메시지 확인 후 commit/push.

## 진행 기록

- [x] 사전 중복 조사 (director)
- [ ] console: SampleNameOf 등 중복 제거
- [ ] monitor: OrderService 리팩토링
- [ ] persistent: JsonDataStore 리팩토링
- [ ] dummy: DummyDataGenerator 리팩토링
- [ ] director: FormatTime/NowTimeString 중복 해소, 통합, 빌드/TC 검증
- [ ] docs/temp 요약, 커밋 메시지 확인 후 commit/push
