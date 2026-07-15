# Phase 8 요약 — 전체 코드 최종 리뷰 및 잔여 마이너 이슈 정리

## 개요

처음으로 특정 범위에 국한하지 않고 전체 코드베이스(Model/Persistence/Monitor/Console/Dummy/Tests/main.cpp)를 독립 리뷰 에이전트(Opus)로 검토했다. 발견된 항목을 심각도별로 검증·수정했고, Phase 6에서 남겨뒀던 표 헤더 라벨 정렬도 마무리했다. 계획 문서: `docs/phase/phase8-final-review.md`.

## 발견 및 수정한 이슈

### Critical

**C1 — 생산 완료 시 재고 이중 계상(팬텀 재고)**: `Monitor::OrderService::CatchUpProduction`이 생산 완료된 주문의 재고를 `stock += actualProductionQty`로만 반영하고, 그 주문이 소비하는 `quantity`는 차감하지 않았다. 반면 즉시 승인(재고 충분) 경로는 `stock -= quantity`로 정확히 차감한다 — 두 경로의 회계 방식이 불일치했다.
- **영향**: 재고 30에 주문 200을 승인해 PRODUCING(실생산량 206)으로 전환 후 생산이 완료되면, 재고가 `30+206=236`으로 표시되지만 이 200개는 이미 이 주문에 배정된 것이라 실제 가용 재고가 아니다. 이후 같은 시료의 다른 200개 주문이 "재고 충분"으로 오판되어 잘못 즉시 승인(이중 배정)될 수 있었다.
- **수정**: `sIt->stock += oIt->actualProductionQty - oIt->quantity;` — 생산량 반영과 이 주문의 소비를 함께 정산.
- **회귀 테스트 갱신**: 기존 `CatchUpProduction_CompletedOrder_TransitionsToConfirmedAndUpdatesStock` 테스트가 버그 동작(전량 더하기)을 그대로 기대값으로 고정하고 있어 수정했고, 이중 배정 시나리오를 직접 재현해 방지되는지 검증하는 신규 테스트 `CatchUpProduction_DoesNotDoubleBookStockForSubsequentOrder`를 추가했다.

**C2 — 메인 메뉴/시료 관리/모니터링 메뉴 루프의 입력 고갈 무한루프**: Phase 5/6에서 `OrderMenuView`/`SampleMenuView`(등록 입력)/`ApprovalMenuView`/`ReleaseMenuView`(번호 선택)에 적용했던 `IsInputExhausted()` 가드가, 정작 최상위 메뉴 선택 루프 3곳(`AppController::Run`, `SampleMenuView::Run`, `MonitoringView::Run`)에는 누락되어 있었다. 표준 입력이 고갈되면 이 루프들이 "잘못된 선택"을 반복 출력하며 무한루프에 빠질 수 있었다. 세 곳 모두 동일한 가드를 추가해 고갈 시 안전하게 종료/복귀하도록 수정했다.

### Important

**I1 — 최상위 예외 처리 부재**: `main.cpp`/`AppController`에 `try/catch`가 전혀 없어, 손상된 JSON 파일 파싱 실패(`JsonDataStore`의 `std::runtime_error`)나 도메인 계층의 상태 전이 오류(`std::logic_error`/`std::invalid_argument`)가 발생하면 프로그램 전체가 `std::terminate`로 죽었다. `AppController::Run()`의 초기 `CatchUpProduction()` 호출과 메인 메뉴 루프 본문을 `try/catch(std::exception&)`로 감싸, 오류 발생 시 메시지를 출력하고 메인 메뉴로 안전하게 복귀하도록 수정했다.

**I2 — 승인/거절 화면에서 Y/N 입력 고갈이 조용히 "거절(REJECTED)"로 처리됨**: `ReadYesNo`는 입력 고갈 시 `false`를 반환하는데, `ApprovalMenuView`는 `false`를 그대로 "거절"로 해석해 되돌릴 수 없는 상태 전이를 사용자 의사와 무관하게 실행했다. `IsInputExhausted()`를 별도로 확인해, 고갈 시에는 거절이 아니라 "처리 취소"(아무 상태 변경 없음)로 처리하도록 수정했다.

### Minor (수정)

- **M1**: JSON 라이터가 `\b`/`\f` 및 기타 제어문자(0x00~0x1F)를 이스케이프하지 않아 비표준 JSON을 생성할 수 있던 문제 — `\b`/`\f` 및 나머지 제어문자에 대한 `\u00XX` 이스케이프를 추가.
- **M2**: FIFO 생산 큐 정렬이 `std::sort`(불안정 정렬)를 사용해 동일 초에 등록된 주문의 순서가 실행마다 달라질 수 있던 문제 — `std::stable_sort`로 교체해 저장소 기록 순서를 tie-break로 사용하도록 수정.

### Minor (기록만, 이번 Phase에서는 미수정)

- **M3**: `CeilDivide`의 부동소수점 경계에서 정확히 나누어떨어지는 경우에도 부동소수점 오차로 실생산량이 1 과다 산정될 수 있는 이론적 가능성 — 발생 빈도가 낮고 수정 시 반올림 처리 검증이 추가로 필요해 별도 Phase로 미룬다.
- **M4**: `JsonDataStore`의 원자적 쓰기 폴백 경로(`rename` 실패 시 `copy_file`)가 완전히 원자적이지 않고, 임시 파일 삭제가 예외를 던질 수 있는 경로 — 저장 계층의 핵심 경로라 신중한 별도 검증이 필요해 이번 Phase에서는 손대지 않았다.

### 리뷰에서 "문제 없음"으로 확인된 영역
빌드 구성(Debug/Release 조건부 테스트 분리), 계산 공식(부족분/실생산량/총생산시간), 상태 전이 가드, REJECTED 모니터링 제외, FIFO 누적 스케줄링 로직과 진행률 clamp — 모두 스펙과 일치함을 확인.

## 부가 작업: 표 헤더 라벨 정렬 마무리 (director)

Phase 6에서 시료명/고객명 등 가변 길이 컬럼에만 적용했던 `PadDisplayWidth`를, "번호"/"주문번호"/"순서"/"주문량"/"부족분"/"실생산량"/"총생산시간"/"진행률"/"재고"/"평균 생산시간"/"수율"/"현재 재고" 등 나머지 한글 고정 헤더 라벨에도 확대 적용했다(`ApprovalMenuView`, `ReleaseMenuView`, `ProductionLineView`, `MonitoringView`, `SampleMenuView`). 또한 실제 화면에서 "주문번호" 컬럼 폭(12)이 실제 주문번호 형식(`ORD-YYYYMMDD-NNNN`, 17자)보다 좁아 다음 컬럼과 붙어버리는 것을 추가로 발견해 폭을 20으로 넓혔다.

## 빌드/테스트 결과

| 구성 | 결과 |
|---|---|
| Debug &#124; x64 | 빌드 성공, TC **62/62** 통과(신규 이중배정 방지 테스트 포함) |
| Release &#124; x64 | 빌드 성공, `--seed` 실행 후 화면 정렬 확인 |
| Debug &#124; Win32 | 빌드 성공 |
| Release &#124; Win32 | 빌드 성공 |
