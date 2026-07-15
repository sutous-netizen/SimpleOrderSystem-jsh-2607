# Phase 1 요약 — 전체 뼈대 코드 구성

## 개요

`docs/01-개요.md` ~ `docs/08-출고처리.md` 기준으로 4개 서브에이전트(console/monitor/persistent/dummy)를 병렬 디스패치하여 `SimpleOrderSystem` 프로젝트의 MVC 뼈대 코드를 구성했다. 계획 문서는 `docs/phase/phase1-skeleton.md` 참고.

## Director가 정의한 공유 계약

- `Model/Types.h`, `Model/Types.cpp` — `OrderStatus`, `StockState`, `Sample`, `Order`
- `Persistence/IDataStore.h` — 영속성 인터페이스
- `Monitor::OrderService` 시그니처 (phase1-skeleton.md에 고정, monitor/console 양쪽에 동일하게 전달)
- `Tests/TestFramework.h` — 자가등록 테스트 매크로(`TEST`, `ASSERT_TRUE`, `ASSERT_EQ`) + `TestFramework::RunAllTests()`

## 에이전트별 산출물

### persistent
- `Persistence/JsonDataStore.h/.cpp`: 외부 라이브러리 없이 직접 구현한 JSON 파서/라이터 기반 `IDataStore` 구현체. 원자적 저장(`.tmp` + rename), `NextOrderNo` 채번 로직 포함.
- `Tests/PersistenceTests.cpp`: 시료/주문 저장·로드 round-trip, 채번 로직 테스트 3건.

### monitor
- `Monitor/OrderService.h/.cpp`: 승인/거절/출고 상태 전이, 재고 계산(부족분/실생산량/총생산시간), 상태별 집계, FIFO 생산 큐 조회, `CatchUpProduction`(재실행 시 경과 시간 반영) 구현.
- `Tests/MonitorTests.cpp`: 인메모리 Fake `IDataStore`로 승인(재고충분/부족)/거절/집계/생산완료 캐치업/재고상태판정/FIFO순서/출고 총 9건 테스트.

### console
- `Console/AppController`, `SampleMenuView`, `OrderMenuView`, `ApprovalMenuView`, `MonitoringView`, `ProductionLineView`, `ReleaseMenuView`, `ConsoleUtil`: 메인 메뉴 및 6개 서브 메뉴 View/Controller. 도메인 로직은 전혀 재구현하지 않고 `OrderService`/`IDataStore` 호출 결과만 표시.

### dummy
- `Dummy/DummyDataGenerator.h/.cpp`: 시료(재고 여유/부족/고갈 분포 보장) 및 주문(5개 상태별 지정 건수, PRODUCING은 monitor 공식 그대로 재사용) 더미 데이터 생성기.
- `Tests/DummyTests.cpp`: 생성 개수/분포/공식 일치/누적(reset=false)/주문번호 유일성 등 8건 테스트.

## Director 통합 작업

1. `SimpleOrderSystem.vcxproj` / `.vcxproj.filters`에 신규 파일 전체 등록(소스 파일/헤더 파일 그룹, 모듈별 하위 필터 추가).
2. `main.cpp` 작성: `JsonDataStore("data/samples.json", "data/orders.json")` → `OrderService` → `AppController` 순으로 wiring. `_DEBUG` 빌드 시 `TestFramework::RunAllTests()`를 먼저 실행.
3. **빌드 이슈 발견 및 수정**: 최초 빌드 시 한글(UTF-8) 주석이 포함된 소스가 CP949로 오인식되어 `namespace`/`std::optional` 등에서 대량 구문 오류 발생 → 4개 구성(Debug/Release × Win32/x64) 전체의 `<ClCompile>`에 `<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>` 추가로 해결.
4. MSBuild로 Debug|x64, Release|x64, Debug|Win32, Release|Win32 4개 구성 전부 빌드 성공 확인.
5. Debug|x64 실행 결과: `_DEBUG` 진입 시 20건 TC 전부 `[PASS]`(persistent 3 + monitor 9 + dummy 8), 이후 메인 메뉴 정상 진입/종료 확인. Release|x64 실행 시 TC 미실행, 메인 메뉴만 정상 동작 확인.

## 확인된 이슈 / 후속 조치 필요 사항

- **문서 무단 수정 발견**: 통합 과정에서 `docs/01-개요.md`, `docs/06-모니터링.md`, `docs/09-미션및제출방법.md`에 예상치 못한 수정이, 그리고 프로젝트 루트에 `README.md`가 생성되어 있는 것을 발견했다. director가 각 서브에이전트에게 부여한 작업 범위에는 문서 수정이나 README 생성이 포함되어 있지 않았음에도 발생한 것으로 보인다. **커밋에는 포함하지 않았으며, 사용자 확인 후 유지/되돌림을 결정해야 한다.**
- `Tests/ModelTests.cpp`(Model 계층 자체 단위 테스트)는 이번 단계에서 별도로 만들지 않았다 — `ToString`/`OrderStatusFromString` 등은 Persistence/Monitor 테스트에서 간접적으로 검증되고 있으나, 다음 단계에서 전용 테스트 보강을 고려할 수 있다.
- 시료 등록/조회/검색은 `IDataStore`를 통한 단순 CRUD로 구현되어 있어 별도 단위 테스트가 없다 — 다음 단계에서 Console 계층 테스트 또는 Persistence 계층 CRUD 테스트로 보강 여지가 있다.
- 실제 데이터 파일(`data/samples.json`, `data/orders.json`)은 런타임에 생성되며 이번 커밋에는 포함하지 않는다.

## 빌드/테스트 결과 요약

| 구성 | 결과 |
|---|---|
| Debug &#124; x64 | 빌드 성공, TC 20/20 통과 |
| Release &#124; x64 | 빌드 성공, 정상 실행(TC 미실행) |
| Debug &#124; Win32 | 빌드 성공 |
| Release &#124; Win32 | 빌드 성공 |
