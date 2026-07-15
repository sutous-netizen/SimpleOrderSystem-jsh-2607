# Phase 9 요약 — 통합 테스트 시나리오 (Debug TC 세트 + Release 시나리오)

## 개요

지금까지의 테스트는 대부분 mock/fake로 계층을 격리한 단위 테스트였다. 이번 Phase는 4개 에이전트를 병렬 디스패치해 여러 계층을 실제로 조합하는 **통합 테스트**를 추가했다: Debug 빌드용 gtest 통합 TC(`Tests/Integrate/Debug/`)와, Release 빌드는 테스트 프레임워크가 없으므로 실제 실행 파일을 구동하는 외부 시나리오(`Tests/Integrate/Release/`)로 나누어 작성했다. 계획 문서: `docs/phase/phase9-integration-tests.md`.

## 산출물

### Tests/Integrate/Debug/ (gtest, mock 없이 실제 컴포넌트 조합, 9개 TC)

- **monitor — `OrderLifecycleIntegrationTests.cpp`(3건)**: 실제 `JsonDataStore`+`OrderService`로 (1) 재고 충분 즉시승인→출고→재로드 검증, (2) 재고부족 승인→`CatchUpProduction`→Phase 8 수정 공식이 파일 기반으로도 정확한지, (3) 같은 시료에 순차 승인 시 이중 배정이 재발하지 않는지(Phase 8 회귀의 파일 기반 재현) 검증.
- **persistent — `RestartAndDummySeedIntegrationTests.cpp`(2건)**: (1) 인스턴스 A→소멸→인스턴스 B로 이어지는 전체 재실행 흐름(등록→주문→승인→[재실행]→출고→신규주문)이 파일에 정확히 누적되는지, (2) `SeedDefaultDataset`으로 시드 후 별도 인스턴스의 도메인 조회가 일치하는지 검증.
- **dummy — `SeedDataConsistencyIntegrationTests.cpp`(4건)**: 시드 데이터가 별도 인스턴스에서 상태별 집계/재고 분류/원본 저장 건수(REJECTED 포함) 모두 결정적으로 일치하는지 검증.

### Tests/Integrate/Release/ (실제 Release 실행 파일을 표준입력으로 구동하는 시나리오, 5건)

- **console**: 시료 등록+목록 확인, 재고부족 주문→즉시 PRODUCING 승인, 주문 거절, 주문 후 모니터링 확인 — 4개 시나리오. **"재고 충분" 시나리오는 코드 확인 결과 UI상 재고를 늘리는 메뉴 자체가 없어 불가능함을 확인**하고, 이를 "재고부족 즉시 PRODUCING" 시나리오로 합리적으로 조정함(가짜로 지어내지 않음).
- **dummy**: `--seed` 옵션으로 시드 후 모니터링/생산라인 화면 확인 시나리오 1건.
- **director — `run-scenarios.ps1`**: 시나리오 입력을 실제 Release 실행 파일에 주입하고 출력에 기대 문구가 포함되는지 자동 검증하는 PowerShell 러너.

## 통합 중 발견 및 수정한 이슈

1. **`SeedDataConsistencyIntegrationTests.cpp`의 include 경로 오류**: `Tests/Integrate/Debug/`에서 `SimpleOrderSystem/`까지는 3단계 위인데 dummy 에이전트가 2단계(`../../Persistence/...`)로 작성해 컴파일 경로가 어긋나 있었다 — director가 검토 중 발견해 3단계(`../../../Persistence/...`)로 수정.
2. **PowerShell 파이프의 표준입력 손상 버그**: `$lines | & $exe` 형태로 자식 프로세스에 표준입력을 주입하면 Windows PowerShell 5.1이 첫 줄 앞에 보이지 않는 바이트를 끼워 넣어(관찰상 UTF-8 BOM으로 추정) 콘솔 앱이 첫 입력("0", "1" 등)을 항상 "잘못된 선택"으로 오인식하는 문제를 실제 실행 중 발견했다. **해결**: 표준입력을 BOM 없는 UTF-8 임시 파일로 만들어 `cmd.exe`의 `<` 리다이렉션으로 주입하도록 러너를 재작성 — 직접 재현 테스트로 원인을 특정하고 수정을 검증했다.
3. **`.gitignore`의 `Debug/`/`Release/` 패턴이 신규 폴더를 가려버림**: 앵커(`/`) 없는 패턴이 트리 전체에서 매칭되어 새로 만든 `Tests/Integrate/Debug/`, `Tests/Integrate/Release/`가 통째로 git에서 무시되고 있었다. 루트 전용 앵커(`/x64/`, `/Debug/`, `/Release/`)로 좁히고, 실제 빌드 산출물 경로(`SimpleOrderSystem/x64/`, `SimpleOrderSystem/Debug/`, `SimpleOrderSystem/Release/` 등, Win32 빌드가 프로젝트 폴더 하위에 만드는 중간 산출물)는 별도로 명시했다. 이 과정에서 그동안 커밋되지 않은 채 방치되어 있던 중간 산출물(.obj/.tlog) 폴더도 함께 정리했다.

## 검증 결과

- **Debug|x64**: TC **71/71** 통과(기존 62건 + 신규 통합 TC 9건).
- **Release|x64**: `run-scenarios.ps1` 실행 결과 **5/5 시나리오 통과**.
- Debug/Release × Win32/x64 4개 구성 전부 빌드 성공.

## 적용한 OOP/clean code 기법 및 superpowers 활용 (에이전트 보고 종합)

- 기존 `PersistenceTests.cpp`/`MonitorTests.cpp`의 임시 파일 픽스처 패턴을 통합 테스트에도 재사용해 일관성 유지(DRY).
- "제3의 인스턴스로 재조회"하는 방식으로 메모리 상태가 아닌 실제 파일 영속화 결과를 검증(영속성 요구사항의 오탐 없는 검증).
- 결정적(deterministic) 불변식만 assert하도록 설계해 랜덤 더미 데이터에 의한 테스트 플래키니스를 최소화.
- console 에이전트는 코드를 먼저 정독해 실제 출력 문자열/입력 순서를 도출한 뒤 시나리오를 작성(추측 금지), UI 제약을 발견하면 시나리오를 현실에 맞게 조정.
- 이번 작업은 대부분 이미 완성된 프로덕션 코드에 대한 사후 통합 테스트 작성이라 Red-Green-Refactor TDD 사이클 자체보다는, 기존 계약(헤더/이전 테스트)을 정확히 따르는 명세 기반(spec-driven) 검증에 가까웠다. 다만 PowerShell 파이프 버그는 실행하며 실제로 발견·재현·수정한 것으로, 통합 테스트가 아니었다면 놓쳤을 문제다.
