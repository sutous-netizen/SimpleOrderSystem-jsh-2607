# Phase 6 요약 — 알려진 기술부채 정리

## 개요

Phase 5 요약에서 남겨뒀던 3가지 기술부채(표 정렬 근본 원인, 잔여 무한루프 패턴, main.cpp 도달불가 코드)를 정리했다. 계획 문서: `docs/phase/phase6-tech-debt-cleanup.md`.

## 1. 표 정렬 근본 원인 수정 (console)

`std::setw`가 문자열 "바이트 수" 기준으로 패딩을 계산해, 한글(멀티바이트 UTF-8)이 섞인 시료명/고객명 컬럼에서 정렬이 어긋나는 문제를 해결했다.

- `Console::PadDisplayWidth(text, targetWidth)` 신규 추가 — ASCII는 폭 1, UTF-8 continuation byte는 폭 0, 그 외(한글 등 멀티바이트 시작 바이트)는 폭 2로 근사하여 "화면 표시 너비" 기준으로 패딩.
- 적용 파일: `SampleMenuView`, `ApprovalMenuView`, `ReleaseMenuView`, `ProductionLineView`, (범위 밖이었지만 동일 문제가 있어 자체 판단으로 추가 적용한) `MonitoringView`의 시료명/고객명 컬럼.
- 헤더 라벨("고객", "시료" 등)에도 동일 함수를 적용해, 데이터 행과 헤더가 같은 기준으로 정렬되도록 일관성을 확보.
- `Tests/ConsoleUtilTests.cpp`에 ASCII-only/한글 포함/이미 목표 너비 초과 3가지 케이스 테스트 추가.
- **육안 확인**: `--seed`로 실제 더미 데이터 생성 후 모니터링/승인 화면을 실행해, 짧은 이름과 긴 한글 이름이 섞여도 다음 컬럼과 정렬됨을 확인했다.

## 2. 잔여 무한루프 패턴 제거 (console)

Phase 5에서 `OrderMenuView`/`SampleMenuView`에 적용했던 `Console::IsInputExhausted()` 체크를 `ApprovalMenuView`/`ReleaseMenuView`의 번호 선택 재시도 루프에도 동일하게 적용해, 표준 입력 고갈 시 무한루프 없이 취소되도록 했다. `Tests/ConsoleViewTests.cpp`에 각 View당 1건씩 검증 테스트를 추가했다.

## 3. main.cpp 도달 불가능 코드 정리 (director)

`#ifdef _DEBUG` 블록 안에서 `return`한 뒤 이어지던 콘솔 앱 실행 코드(도달 불가능한 채로 컴파일되던 부분)를, `RunTests()`(Debug 전용)/`RunConsoleApp()`(Release 전용) 두 함수로 나누고 `#ifdef _DEBUG ... #else ... #endif`로 명시적으로 분리했다. `main()`은 이제 단순히 두 함수 중 하나를 호출하고 반환하는 얇은 진입점이 되었다(단일 책임 원칙 강화).

## 적용한 기법 (console 에이전트 보고)

- **DRY**: `PadDisplayWidth`를 `ConsoleUtil`에 한 곳으로 모아 5개 View 파일에서 재사용(각자 바이트-폭 계산 로직을 중복 구현하지 않음).
- **매직 넘버 제거**: `kCustomerColumnWidth`, `kSampleColumnWidth` 등 이름 있는 상수로 컬럼 폭을 표현.
- **익명 네임스페이스 캡슐화**: 내부 헬퍼(`DisplayWidthOf`)를 헤더에 노출하지 않고 구현 파일에 숨김(기존 `LocalNow`, `g_inputExhausted`와 같은 방식 재사용).
- **기존 방어 패턴 일관 적용**: Phase 5에서 확립한 `IsInputExhausted()` 가드를 새로운 두 View에도 동일한 관용구로 적용(스타일 분기 없음).

## 통합 중 발견한 환경 이슈 (버그 아님)

통합 빌드 직후 전체 TC 실행 중 `ApprovalMenuView_ExhaustedInput_...` 테스트에서 한 차례 응답 없음(hang)이 관측되어 조사했으나, 원인은 코드 결함이 아니라 **이전에 종료되지 않고 남아있던 `SimpleOrderSystem.exe` 프로세스가 실행 파일을 잠그고 있던 환경 문제**였다(디버그 계측 후 재현 안 됨, 프로세스 정리 후 반복 실행 3회 모두 정상 통과 확인). 코드 변경 없이 해결되었다.

## Director 통합 작업

1. `main.cpp` 리팩토링(위 3번 항목).
2. console 에이전트 산출물 통합 확인(신규 파일 없음 → vcxproj 변경 불필요).
3. 4개 구성(Debug/Release × Win32/x64) 빌드 성공 확인.
4. Debug|x64 실행: TC **61/61** 통과(기존 56 + 신규 5: PadDisplayWidth 3건 + Approval/Release 무한루프 방지 2건) 확인, 반복 실행으로 안정성 재확인.
5. `.gitignore`에 `coverage/`(로컬 코드 커버리지 도구 산출물) 추가.

## 확인된 이슈 / 후속 조치 필요 사항

- 표의 헤더 라벨("번호", "주문번호") 자체는 `std::setw`(바이트 기준) 그대로라 여전히 붙어 보일 수 있다 — 이번 Phase 범위는 가변 길이 사용자 데이터(시료명/고객명)로 한정했기 때문에 의도적으로 남겨뒀다. 완전한 정렬을 원한다면 고정 라벨 컬럼에도 `PadDisplayWidth`를 확대 적용하는 후속 작업이 필요하다.

## 빌드/테스트 결과 요약

| 구성 | 결과 |
|---|---|
| Debug &#124; x64 | 빌드 성공, TC 61/61 통과(반복 실행 안정성 확인) |
| Release &#124; x64 | 빌드 성공, `--seed` 실행 후 정렬 개선 육안 확인 |
| Debug &#124; Win32 | 빌드 성공 |
| Release &#124; Win32 | 빌드 성공 |
