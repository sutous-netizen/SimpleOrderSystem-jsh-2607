# Phase 2 요약 — 생산라인 진행률/ETA, 통합 테스트 보강, 더미 시드 유틸리티

## 개요

Phase 1에서 남긴 후속 조치(`docs/temp/phase1-summary.md`)를 해소하기 위해 4개 서브에이전트를 다시 병렬 디스패치했다. 계획 문서: `docs/phase/phase2-production-and-hardening.md`.

## 공유 계약 변경

`Monitor::ProductionQueueItem`에 `elapsedMinutes`(double), `progressPercent`(double, 0~100), `estimatedCompletionAt`(string, "YYYY-MM-DD HH:MM:SS") 3개 필드 추가. 단일 생산 라인 + FIFO 누적 스케줄링(기존 `CatchUpProduction`과 동일한 규칙)으로 계산.

## 에이전트별 산출물

### monitor
- `Monitor/OrderService.h/.cpp`: `ProductionQueueItem` 확장, 스케줄링 로직을 `BuildProductionSchedule()` private 헬퍼로 추출해 `CatchUpProduction()`/`GetProductionQueue()`가 공유하도록 리팩토링(DRY).
- `Tests/MonitorTests.cpp`: 진행 중/완료시각 경과/FIFO 2번째 주문 대기 3건 테스트 추가.

### console
- `Console/ProductionLineView.cpp`: 진행률(%)과 완료 예정 시각(HH:MM)을 "현재 처리 중" 강조 블록 + 대기 목록 표에 추가 표시. 값은 monitor가 계산한 값을 그대로 사용(재계산 없음).

### persistent
- `Tests/ModelTests.cpp`(신규): `OrderStatus`/`StockState` 문자열 변환 왕복·예외 테스트 6건.
- `Tests/PersistenceTests.cpp`: 실제 파일 기반 "재실행 시뮬레이션" 통합 테스트 1건 추가(인스턴스 A로 저장 → 소멸 → 인스턴스 B로 재로드하여 값 일치 검증) — "재실행 후 데이터 유지" 요구사항을 실제 파일로 검증.

### dummy
- `Dummy/DummyDataGenerator.h/.cpp`: `SeedDefaultDataset(bool reset)` 추가(시료 12개 + 상태별 주문 3건씩 = 15건). 시료는 `reset` 인자를 따르고 주문은 항상 append.
- `Tests/DummyTests.cpp`: `SeedDefaultDataset` 검증 테스트 1건 추가.

## Director 통합 작업

1. `main.cpp`: `--seed` 커맨드라인 옵션 추가 → `Dummy::DummyDataGenerator::SeedDefaultDataset(true)` 호출 후 콘솔 앱 실행.
2. `Tests/ModelTests.cpp`를 `SimpleOrderSystem.vcxproj`/`.vcxproj.filters`에 등록.
3. 4개 구성(Debug/Release × Win32/x64) 전부 빌드 성공 확인.
4. Debug 실행 결과: TC **31/31** 통과(Model 6 + Persistence 4 + Monitor 12 + Dummy 9).
5. `--seed` 실행 후 실제 동작 확인:
   - 메인 메뉴 시스템 현황에 시료 12종/주문 15건 반영.
   - `[4] 모니터링 → [1] 주문량 확인`: 상태별 건수 정상 표시(단, `CatchUpProduction`이 시작 시 실행되어 일부 PRODUCING 주문이 자동으로 CONFIRMED로 전환됨 — 의도된 동작).
   - `[5] 생산라인 조회`: "현재 처리 중" 블록에 진행률(%)·완료 예정 시각 표시, 대기 목록 표에도 각 항목의 진행률/예상완료 컬럼이 정상 출력됨을 확인.

## 확인된 이슈 / 후속 조치 필요 사항

- 생산라인 대기 목록 표의 컬럼 정렬(고정폭 패딩)이 다소 좁아 시료명이 긴 경우 컬럼이 붙어 보인다 — 기능상 문제는 없으나 다음 단계에서 표 포맷 다듬기 여지가 있다.
- Visual Studio에서 추가된 gmock/gtest NuGet 참조(`packages.config`)는 이번 Phase에서도 실제 테스트 코드에 활용하지 않았다 — 현재의 자체 `TestFramework`(자가등록 매크로)로 충분히 TDD를 수행 중이며, gmock/gtest 도입 여부는 별도 결정이 필요하다.
- `SeedDefaultDataset`은 주문을 항상 append하므로 `--seed`를 여러 번 실행하면 주문이 계속 누적된다(의도된 동작이나 사용자에게 안내 문구가 없음 — 필요 시 후속 개선 가능).

## 빌드/테스트 결과 요약

| 구성 | 결과 |
|---|---|
| Debug &#124; x64 | 빌드 성공, TC 31/31 통과, `--seed` 동작 확인 |
| Release &#124; x64 | 빌드 성공 |
| Debug &#124; Win32 | 빌드 성공 |
| Release &#124; Win32 | 빌드 성공 |
