# Phase 6 — 알려진 기술부채 정리

## 목표

이전 Phase 요약에서 발견했지만 범위 밖이라 남겨뒀던 3가지를 정리한다.

1. **표 컬럼 정렬 문제의 근본 원인 수정**: `std::setw`는 문자열의 "바이트 수" 기준으로 패딩을 계산하는데, 한글(UTF-8에서 1글자=3바이트)이 섞인 문자열에 `std::setw`를 쓰면 자리가 어긋난다. 시료명/고객명처럼 한글이 들어가는 컬럼이 있는 모든 표(`SampleMenuView`, `ApprovalMenuView`, `ReleaseMenuView`, `ProductionLineView`)에서 발생한다.
2. **잔여 무한루프 패턴 제거**: Phase 5에서 `OrderMenuView`/`SampleMenuView`의 재시도 루프에 `Console::IsInputExhausted()` 체크를 추가해 표준 입력 고갈 시 무한루프를 막았다. 같은 `while(true)` 번호 선택 재시도 루프가 `ApprovalMenuView`/`ReleaseMenuView`에도 있는데 아직 이 체크가 없다.
3. **main.cpp 도달 불가능 코드 정리**: Debug 빌드에서 `#ifdef _DEBUG` 블록 안에서 `return`하고 나면, 그 아래 콘솔 앱 실행 코드는 도달 불가능한 채로 컴파일된다(경고 가능성). `#else`로 명시적으로 분리한다.

## 공유 유틸리티 (director가 정의하지 않고 console 에이전트가 직접 설계 — 아래는 필요조건만 제시)

`Console/ConsoleUtil.h/.cpp`에 UTF-8 문자열의 "화면 표시 너비"를 계산해서 목표 너비만큼 공백을 채우는 함수를 추가해라(함수명은 자유, 예: `PadDisplayWidth(const std::string& text, int targetWidth)` → 화면 너비 기준으로 패딩된 문자열 반환). 규칙:
- ASCII(1바이트, 0x00~0x7F)는 화면 폭 1.
- 나머지(한글 등 멀티바이트 UTF-8 시퀀스)는 화면 폭 2로 근사한다(완벽한 유니코드 East Asian Width 처리는 과설계이므로, "UTF-8 선행 바이트가 아닌 continuation byte(0x80~0xBF)는 폭 계산에서 건너뛰고, 나머지 코드포인트 시작 바이트 중 ASCII가 아니면 폭 2로 취급"하는 정도의 근사면 충분하다).
- 텍스트의 화면 폭이 이미 목표 너비 이상이면 자르지 말고 공백 없이 그대로 반환(현재 `std::setw`도 넘치면 자르지 않으므로 동일한 동작 유지).
- 목표 너비보다 짧으면 부족한 만큼 공백을 오른쪽에 붙인다(왼쪽 정렬 유지).

## 네가 할 일 (console 에이전트)

### 1. 표 정렬 수정
- `Console/ConsoleUtil.h/.cpp`에 위 패딩 함수를 추가하고 `Tests/ConsoleUtilTests.cpp`에 테스트를 추가해라(ASCII만 있는 문자열, 한글이 섞인 문자열, 목표 너비보다 긴 문자열 각각 최소 1건씩).
- `SampleMenuView::PrintSampleTable`, `ApprovalMenuView::Run`, `ReleaseMenuView::Run`, `ProductionLineView::Run`에서 한글이 들어갈 수 있는 컬럼(시료명, 고객명)에 대해 `std::setw` 대신 새 패딩 함수를 사용하도록 수정해라. 숫자/영문 컬럼(수량, 상태 등)은 `std::setw`를 그대로 사용해도 된다(문제 없음).

### 2. 잔여 무한루프 패턴 제거
- `ApprovalMenuView::Run`, `ReleaseMenuView::Run`의 번호 선택 `while(true)` 재시도 루프에 `Console::IsInputExhausted()` 체크를 추가해서, 표준 입력이 고갈되면 즉시 취소 메시지를 출력하고 반환하도록 수정해라(Phase 5에서 `OrderMenuView`/`SampleMenuView`에 적용한 것과 동일한 패턴 — 해당 파일들을 참고해라).
- `Tests/ConsoleViewTests.cpp`에 `ApprovalMenuView`, `ReleaseMenuView` 각각 "입력 고갈 시 무한루프 없이 취소되는지" 검증하는 테스트를 최소 1건씩 추가해라(Phase 5의 `ReadYesNo_ExhaustedInput_...` 테스트 패턴을 참고해라).

### 3. 테스트 이름 규칙
`TEST`/`TEST_F`의 SuiteName/CaseName은 영문 PascalCase만 사용, 각 테스트 위에 한글 설명 주석 유지(Phase 3부터 이어진 규칙).

### 4. 하지 마라
- `MonitoringView`, `AppController`는 이번 Phase 범위 밖이다(과잉 확장 금지) — 단, `MonitoringView`도 시료명 등 한글 컬럼이 있다면 같은 패딩 함수를 적용해도 좋다(네 판단에 맡긴다. 적용한다면 보고에 남겨라).
- vcxproj/.vcxproj.filters/main.cpp 수정 금지 — director가 처리.
- `Monitor/OrderService.h/.cpp`, `Persistence/*`, `Model/*`, `Tests/Mocks/MockDataStore.h` 수정 금지.
- 기존 테스트(Phase 1~5에서 작성된 것)는 절대 삭제/약화하지 마라.

## Director가 직접 할 일

1. `main.cpp`의 `#ifdef _DEBUG ... return ...; #endif` 이후 코드를 `#else` 블록으로 명시적으로 분리해 도달 불가능 코드 경고 가능성을 제거한다.
2. console 에이전트 산출물 통합: 신규/변경 파일 확인, vcxproj/filters에 변경사항 반영(신규 파일이 있다면 등록).
3. 4개 구성 빌드 검증, Debug TC 전체 통과 확인(수 증가 확인), Release 정상 동작 확인.
4. 실제로 한글이 포함된 시료명/고객명으로 표를 출력해서 정렬이 개선됐는지 육안으로 확인한다(`--seed`로 더미 데이터 생성 후 각 화면 실행).
5. docs/temp 요약 작성, 커밋 메시지 확인 후 commit/push.

## 진행 기록

- [ ] console: 표 정렬(PadDisplayWidth) + 잔여 무한루프 패턴 제거 + 테스트 추가
- [ ] director: main.cpp 도달불가 코드 정리
- [ ] 통합: vcxproj 등록, 빌드/실행/육안 확인
- [ ] docs/temp 요약, 커밋 메시지 확인 후 commit/push
