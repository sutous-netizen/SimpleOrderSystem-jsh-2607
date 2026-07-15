# Phase 10 요약 — OOP/Clean Code 리팩토링 (superpowers 활용, TC 변경 없음)

## 개요

기능/버그 수정이 아니라, 지금까지 여러 Phase에 걸쳐 여러 에이전트가 작성한 코드를 OOP/clean code 관점에서 다시 훑어 리팩토링했다. **`Tests/`(하위 `Tests/Integrate/` 포함)는 어떤 파일도 수정하지 않았고, 기존 71개 TC가 전부 그대로 통과**해 동작 보존(behavior-preserving refactor)이 확인되었다. 계획 문서: `docs/phase/phase10-refactor-clean-code.md`.

## 에이전트별 리팩토링 내역

### console — DRY 제거
`Console/ApprovalMenuView.cpp`와 `Console/ReleaseMenuView.cpp`에 완전히 동일하게 중복 정의되어 있던 `SampleNameOf(store, sampleId)` 헬퍼를 `Console/ConsoleUtil.h/.cpp`의 공유 함수로 추출. 나머지 View(`SampleMenuView`/`OrderMenuView`/`MonitoringView`/`ProductionLineView`/`AppController`)는 점검 결과 이미 충분히 정리되어 있어 추가 변경 없음(YAGNI). MSBuild로 직접 빌드해 71/71 TC 통과를 확인함.

### monitor — Extract Method, DRY 제거
`Monitor/OrderService.cpp`의 `ApproveOrder`/`RejectOrder`/`ReleaseOrder`에 반복되던 "주문번호로 찾기→없으면 예외", "상태 확인→다르면 예외", "시료 조회→없으면 예외" 3가지 패턴을 각각 `FindOrderOrThrow`/`RequireStatus`/`FindSampleOrThrow` private 헬퍼로 추출(정적 메서드로 선언해 인스턴스 상태 의존 최소화). 계산 공식/이중배정 방지 로직은 전혀 손대지 않음. 빌드 후 71/71 TC 통과 확인(devenv.exe가 빌드 산출물을 잠그고 있던 환경 이슈를 발견해 임시 출력 경로로 우회 검증).

### persistent — Extract Method/Function, 매직 넘버 제거
`Persistence/JsonDataStore.cpp`의 `SampleFromJson`/`OrderFromJson`에 반복되던 "키 조회+타입체크+기본값" 패턴을 `GetString`/`GetInt64`/`GetDouble` 헬퍼로, `ToJson` 계열의 반복되는 필드 추가 패턴을 `JsonValue::AddString`/`AddNumber` 빌더 메서드로 추출. 정밀도(17)/들여쓰기 문자열/주문번호 포맷 상수(prefix/date/seq 길이)를 이름 있는 상수로 뽑음. JSON 필드명/포맷은 전혀 바뀌지 않음.

### dummy — 매직 넘버 제거, DRY 제거
`Dummy/DummyDataGenerator.cpp`에 흩어져 있던 수치 리터럴(생산시간/수율/재고 범위, 주문 수량 범위, 시각 오프셋 등)을 이름 있는 `constexpr` 상수로 전부 추출. `CONFIRMED`/`RELEASE` 케이스에서 완전히 동일하게 반복되던 재고 기반 수량 산정 로직을 `RandomQuantityWithinStock` 헬퍼로 통합. 값 자체와 RNG 호출 순서는 그대로 유지해 분포가 바뀌지 않음을 보장. 빌드 후 71/71 TC 통과 확인.

## Director가 직접 처리한 사항 — FormatTime 계층 간 중복 해소

`Monitor/OrderService.cpp`(`FormatTime`)와 `Console/ConsoleUtil.cpp`(`LocalNow`+`NowTimeString`)가 "YYYY-MM-DD HH:MM:SS" 포맷을 각자 독립적으로 구현하고 있던 것을 확인했다. 두 계층(Monitor/Console)이 이미 공통으로 의존하는 `Model/Types.h`에 `Model::FormatLocalTimestamp(std::time_t)`를 새로 추가(새 파일 없이 기존 `Types.h/.cpp`에 추가해 vcxproj 변경 불필요)하고, 양쪽의 중복 구현을 이 함수 호출로 교체했다. 출력 포맷은 원래 두 구현이 이미 바이트 단위로 동일했으므로(zero-padded, 동일 구분자) 동작 변화 없음.

## 검증 결과

- **Debug|x64**: TC **71/71** 통과(기존 62 + 통합 9, 전부 무변경), 기존 `Tests/` 디렉터리 어떤 파일도 수정되지 않음.
- **Release|x64**: `run-scenarios.ps1` 실행 결과 **5/5 시나리오** 그대로 통과.
- Debug/Release × Win32/x64 4개 구성 전부 빌드 성공.

## 적용한 OOP/Clean Code 기법 종합

- **DRY(중복 제거)**: `SampleNameOf`(Console), 3가지 예외 패턴(Monitor), JSON 필드 변환 패턴(Persistence), 재고 기반 수량 산정(Dummy), 시간 포맷(계층 간, Director) — 총 5곳의 중복을 제거.
- **Extract Method/Function**: 각 에이전트가 긴 함수에서 의도를 드러내는 이름의 작은 함수/메서드를 추출(`FindOrderOrThrow`, `GetString`, `RandomQuantityWithinStock` 등).
- **매직 넘버/리터럴 제거**: persistent(정밀도/포맷 상수), dummy(수치 범위 상수) 다수.
- **명명(Naming)**: 추출된 헬퍼 이름이 모두 "무엇을 하는지+실패 시 어떻게 되는지"를 드러내도록 지음(`FindOrderOrThrow`, `RequireStatus` 등).
- **공개 인터페이스 불변성**: 5개 파일 모두 헤더의 공개 클래스/함수 시그니처를 전혀 바꾸지 않아, 테스트 파일을 건드리지 않고도 전부 그대로 통과했다.

## superpowers 활용

이번 작업은 버그 수정이 아니라 "동작 보존 리팩토링"이 목적이라 `systematic-debugging`은 적용 대상이 아니었다. 대신 TDD의 "리팩토링" 단계와 동일한 절차(기존 테스트 계약 파악 → 리팩토링 → 컴파일/테스트로 회귀 확인)를 모든 에이전트가 일관되게 따랐고, 이번 Phase에서는 여러 에이전트가 실제로 로컬 MSBuild 컴파일 + 71개 TC 실행까지 직접 수행해 결과를 보고했다(이전 Phase 대비 컴파일 환경 가용성이 개선됨).

## 참고 (환경 이슈, 코드와 무관)

- monitor 에이전트가 빌드 중 Visual Studio(devenv.exe)가 프로젝트를 열어두고 있어 출력 파일(`vc145.pdb`)이 잠겨 있던 것을 발견했다고 보고했다. director의 최종 통합 빌드 시에도 devenv.exe가 실행 중인 것을 확인해 종료한 뒤 빌드했다.
