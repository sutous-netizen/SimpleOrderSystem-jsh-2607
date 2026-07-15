# Phase 9 — 통합 테스트 시나리오 (Debug TC 세트 + Release 시나리오)

## 목표

지금까지의 테스트(Model/Persistence/Monitor/Dummy/Console)는 대부분 mock/fake(`Tests/Mocks/MockDataStore.h`)로 각 계층을 격리해 검증했다. 이번 Phase는 여러 계층을 실제로 조합해서(mock 없이) 전체 흐름이 맞물려 동작하는지 검증하는 **통합 테스트**를 추가한다.

- `SimpleOrderSystem/Tests/Integrate/Debug/` — gtest 기반, Debug 빌드에서만 컴파일/자동 실행되는 통합 TC. 실제 `Persistence::JsonDataStore`(임시 파일) + 실제 `Monitor::OrderService`(+ 필요시 실제 Console View)를 그대로 조합해서 사용하고, mock은 쓰지 않는다.
- `SimpleOrderSystem/Tests/Integrate/Release/` — Release 빌드는 설계상 테스트 프레임워크가 전혀 포함되지 않으므로(Phase 3), 컴파일되는 TC가 아니라 **실제로 빌드된 Release 실행 파일을 표준입력으로 구동해 결과를 검증하는 외부 시나리오**(입력 스크립트 + 기대 결과 + 이를 자동 실행/검증하는 PowerShell 러너)로 구성한다.

## 공유 규칙 (director가 정의)

### Debug 통합 TC
- 파일당 하나의 "여러 계층에 걸친 시나리오"를 검증한다(단위 테스트의 재검증이 아니라, 여러 컴포넌트가 실제로 맞물리는 지점에 집중).
- `Persistence::JsonDataStore`는 임시 디렉터리에 고유 파일 경로를 생성해 사용하고, 테스트 종료 시 정리한다(`PersistenceTests.cpp`의 기존 픽스처 패턴을 참고해도 좋다).
- 테스트 이름 규칙은 기존과 동일: `TEST`/`TEST_F`의 SuiteName/CaseName은 영문 PascalCase, 각 테스트 위에 한글 설명 주석.
- director가 vcxproj의 Debug 전용 `<ItemGroup Condition="'$(Configuration)'=='Debug'">`에 신규 파일들을 등록한다(각 에이전트는 vcxproj를 직접 건드리지 않는다).

### Release 시나리오
- `Tests/Integrate/Release/scenarios/<번호>-<시나리오이름>.txt` — 실제 콘솔 앱에 그대로 흘려보낼 표준입력 스크립트(줄바꿈으로 구분된 메뉴 입력 시퀀스).
- `Tests/Integrate/Release/scenarios/<번호>-<시나리오이름>.expect.txt` — 해당 시나리오 실행 후 출력에 반드시 포함되어야 하는 부분 문자열 목록(한 줄에 하나씩, 순서 무관 포함 여부만 검사).
- director가 `Tests/Integrate/Release/run-scenarios.ps1`(PowerShell 러너)을 작성해 실제 빌드된 `x64\Release\SimpleOrderSystem.exe`에 각 입력 스크립트를 주입하고, 출력에 기대 문자열이 모두 포함되는지 자동으로 검증한다.
- 시나리오는 매번 독립적인 `data/` 상태에서 시작해야 하므로, 러너가 시나리오 실행 전 `data/` 를 초기화(또는 격리된 임시 작업 디렉터리 사용)한다.

## 에이전트별 작업

### monitor — `Tests/Integrate/Debug/OrderLifecycleIntegrationTests.cpp`
실제 `Persistence::JsonDataStore` + 실제 `Monitor::OrderService`를 조합해(mock 없이) 아래 시나리오를 gtest로 작성해라:
1. 시료 등록 → `PlaceOrder` → 재고 충분 `ApproveOrder`(즉시 CONFIRMED) → `ReleaseOrder` → 파일에서 다시 로드해 최종 상태(RELEASE, 재고 차감)가 정확한지 검증.
2. 재고 부족 → `ApproveOrder`(PRODUCING) → 생산 완료 시각을 지난 것처럼 `updatedAt`을 과거로 세팅한 뒤 `CatchUpProduction()` → CONFIRMED 전환 및 재고가 Phase 8에서 고친 공식(`stock += actualProductionQty - quantity`)대로 정확히 반영되는지, 새 `JsonDataStore` 인스턴스로 다시 읽어도 동일한지 검증.
3. 같은 시료에 대해 순서대로 두 건을 승인할 때 이중 배정되지 않는지(Phase 8 회귀 시나리오를 파일 기반 실제 스토어로 재현).

### persistent — `Tests/Integrate/Debug/RestartAndDummySeedIntegrationTests.cpp`
1. "재실행" 시나리오를 처음부터 끝까지: 인스턴스 A로 시료+주문을 저장하고 일부는 `Monitor::OrderService`로 상태 전이(승인 등)까지 시킨 뒤 인스턴스 소멸 → 새 `JsonDataStore` 인스턴스 B + 새 `OrderService`로 이어서 조회/조작해도 이전 상태가 정확히 이어지는지 검증.
2. `Dummy::DummyDataGenerator::SeedDefaultDataset`으로 실제 파일에 데이터를 채운 뒤, 그 데이터를 실제 `OrderService`로 조회(상태별 집계, 재고 현황)했을 때 개수/분포가 기대와 일치하는지 검증(더미 생성기 + 영속성 + 도메인 조회가 실제로 맞물리는 지점).

### console — `Tests/Integrate/Release/scenarios/` 입력 스크립트 + 기대 결과 작성
아래 4개 시나리오를 각각 `<번호>-<이름>.txt`(입력)와 `<번호>-<이름>.expect.txt`(기대 출력 부분 문자열)로 작성해라. 실제 메뉴 흐름(`docs/02`~`docs/08` 예시 화면)을 기준으로 정확한 입력 시퀀스를 만들어야 한다(예: 시료 등록 → 목록 조회 → 종료 등):
1. `01-register-and-list-sample` — 시료 관리에서 새 시료를 등록하고 목록에서 확인 후 종료.
2. `02-place-order-sufficient-stock` — 시료 등록(재고를 충분히 준 뒤) → 주문 접수 → 승인(즉시 CONFIRMED 확인) → 출고 처리까지.
3. `03-place-order-insufficient-stock` — 재고가 부족한 시료로 주문 → 승인(PRODUCING 전환, 부족분/실생산량 문구 확인) → 생산라인 조회 화면 확인.
4. `04-reject-order` — 주문 접수 후 승인 화면에서 거절 선택 → 거절 완료 확인.
각 `.expect.txt`에는 해당 시나리오가 성공했다면 반드시 화면에 나타나야 하는 핵심 문구(예: "예약 접수 완료", "승인 완료", "상태 변경  RESERVED → CONFIRMED" 등)를 적어라.

### dummy — `Tests/Integrate/Release/scenarios/05-seed-and-monitor.txt` + `.expect.txt`, `Tests/Integrate/Debug/SeedDataConsistencyIntegrationTests.cpp`
1. Release 시나리오: `--seed` 옵션으로 실행한 뒤(러너가 `--seed`를 인자로 넘길 수 있도록 시나리오 메타데이터에 표시 — 파일 첫 줄에 `#ARGS --seed` 주석 형태로 표기해라) 모니터링(`[4]`) 화면에서 상태별 건수/재고 현황이 정상적으로 표시되는지, 생산라인(`[5]`) 화면이 정상 표시되는지 확인하는 시나리오.
2. Debug 통합 테스트: `SeedDefaultDataset`으로 실제 파일에 시드한 뒤, 별도의 `OrderService`/`JsonDataStore` 인스턴스로 다시 열어 `GetOrderCountsByStatus`/`GetInventoryStatus` 결과가 시드 데이터와 일치하는지 검증(더미 생성 → 파일 저장 → 다른 인스턴스에서 도메인 조회까지 이어지는 통합 지점).

## Director가 할 일
1. 위 산출물을 vcxproj(Debug 전용 ItemGroup)에 등록.
2. `Tests/Integrate/Release/run-scenarios.ps1` 작성 — 각 시나리오 입력 파일을 읽어 `x64\Release\SimpleOrderSystem.exe`(필요시 `--seed`)에 주입하고, 출력에 `.expect.txt`의 모든 줄이 부분 문자열로 포함되는지 검사, 시나리오마다 독립된 임시 작업 디렉터리에서 실행(격리).
3. 4개 구성 빌드, Debug TC 전체 통과 확인(신규 통합 TC 포함), Release 빌드 후 `run-scenarios.ps1` 실행해 모든 시나리오 통과 확인.
4. docs/temp 요약, 커밋 메시지 확인 후 commit/push.

## 진행 기록
- [x] 공유 규칙/디렉터리 정의 (director)
- [ ] monitor: OrderLifecycleIntegrationTests.cpp
- [ ] persistent: RestartAndDummySeedIntegrationTests.cpp
- [ ] console: Release 시나리오 1~4
- [ ] dummy: Release 시나리오 5 + SeedDataConsistencyIntegrationTests.cpp
- [ ] 통합: vcxproj 등록, run-scenarios.ps1 작성, 빌드/실행 검증
- [ ] docs/temp 요약, 커밋 메시지 확인 후 commit/push
