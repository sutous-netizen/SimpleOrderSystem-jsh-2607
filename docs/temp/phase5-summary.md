# Phase 5 요약 — Console 계층 견고성 점검 및 테스트 커버리지 확대

## 개요

Console(View/Controller) 계층에 자동 테스트가 전혀 없던 공백을 메꾸고, 점검 과정에서 실제 방어 로직 누락(입력 스트림 고갈 시 무한루프 가능성)을 발견해 수정했다. 계획 문서: `docs/phase/phase5-console-hardening.md`.

## 발견 및 수정한 버그 (중요)

`Console::ReadLine`은 `std::getline` 실패(EOF 등 표준 입력 고갈) 시 `std::cin.clear()`만 하고 항상 빈 문자열을 반환했다. 문제는 `OrderMenuView`/`SampleMenuView`/`ReadYesNo`의 `while(true)` 재시도 루프가 "빈 입력"과 "입력 스트림이 아예 고갈된 상태"를 구분하지 못해, 입력이 더 이상 들어올 수 없는 상황(예: 파이프 입력이 끝났거나 Ctrl+Z로 EOF)에서도 계속 재시도하며 **무한루프에 빠지는** 결함이었다.

**수정**: `Console::IsInputExhausted()`를 새로 추가해 `ReadLine`이 EOF로 실패했는지 상태로 노출하고, `ReadYesNo`와 `OrderMenuView`(수량 입력)/`SampleMenuView`(평균생산시간/수율 입력)의 재시도 루프가 이 상태를 확인해 즉시 취소하고 반환하도록 고쳤다. 기존 함수 시그니처는 그대로 유지해 호출부 영향을 최소화했다(단일 책임 원칙 유지).

이 버그는 테스트를 작성하는 과정(입력 스트림을 고갈시키는 시나리오를 만들면서)에서 발견되었다 — TDD의 "테스트가 곧 검증 도구"라는 가치를 보여주는 사례다.

## 신규 테스트 (총 24건, 기존 32건 + 신규 24건 = 56건)

### Tests/ConsoleUtilTests.cpp (19건)
`Trim`(공백 제거/빈 문자열), `TryParseInt64`(양수/음수/공백 포함/비숫자/후행 문자/빈 문자열), `TryParseDouble`(소수/정수형/잘못된 형식), `NowDateCompact`(8자리 형식), `ReadLine`(트림, EOF 고갈), `ReadYesNo`(Y/N, 재시도 후 성공, EOF 고갈 시 무한루프 없이 false 반환) 커버.

### Tests/ConsoleViewTests.cpp (5건)
`Tests/Mocks/MockDataStore.h`의 `NiceMock<MockDataStore>` + `InMemoryDataStore` + 실제 `Monitor::OrderService`를 조합해 View + 도메인 로직을 통합 검증(화면 문구가 아닌 저장소 최종 상태로 검증):
- `OrderMenuView`: 존재하지 않는 시료 ID → 주문 미생성, 유효 입력 후 `[N]` → 미생성, 유효 입력 후 `[Y]` → `RESERVED` 주문 생성 확인.
- `SampleMenuView`: 중복 ID 등록 시도 → 시료 목록 불변, 잘못된 수율 반복 입력 후 유효값 → 재시도 루프를 거쳐 정상 등록.

## 적용한 기법 (console 에이전트 보고)

- **Characterize-then-harden(TDD)**: 레거시 코드에 테스트를 먼저 작성하면서 숨어있던 무한루프 위험을 실제로 찾아냄.
- **의존성 주입/인터페이스 분리**: `IDataStore` 인터페이스 덕분에 `NiceMock<MockDataStore>` + fake로 파일 I/O 없이 `OrderMenuView`/`SampleMenuView`를 실제 도메인 로직(`OrderService`)과 함께 통합 테스트.
- **단일 책임 유지**: `ReadLine`의 시그니처/반환 타입을 바꾸지 않고, EOF 상태를 별도의 작은 조회 함수(`IsInputExhausted`)로 분리해 기존 호출부에 영향 없이 확장.
- **테스트 픽스처**로 cin 스텁 설정/복원(`TearDown`에서 `std::cin.rdbuf()` 원복)을 캡슐화해 테스트 간 오염 방지.
- 화면 텍스트보다 "최종 상태"(저장소 내용) 검증을 우선해 테스트를 화면 문구 변경에 덜 취약하게 작성.

## Director 통합 작업

1. `Tests/ConsoleUtilTests.cpp`, `Tests/ConsoleViewTests.cpp`를 vcxproj/filters의 Debug 전용 테스트 ItemGroup에 등록.
2. 4개 구성(Debug/Release × Win32/x64) 빌드 성공 확인.
3. Debug|x64 실행: TC **56/56** 통과(기존 32 + 신규 24) 후 즉시 종료(콘솔 앱 미실행) 확인. `< /dev/null`로 표준 입력을 완전히 고갈시킨 상태로도 정상 종료됨을 확인(수정한 버그가 실제 실행 환경에서도 해결됐음을 재확인).
4. Release|x64 실행: 정상적으로 콘솔 메뉴 진입 확인(테스트 미포함).

## 확인된 이슈 / 후속 조치 필요 사항

- 이번 Phase는 계획대로 `OrderMenuView`/`SampleMenuView` 2개 View로 범위를 한정했다. 동일한 "입력 스트림 고갈 시 무한루프" 패턴이 `ApprovalMenuView`/`ReleaseMenuView`의 번호 선택 재시도 루프에도 구조적으로 남아있을 수 있다(director가 직접 확인 필요) — 다음 단계에서 `IsInputExhausted()` 체크를 이 두 View에도 동일하게 적용하는 것을 고려해야 한다.
- `MonitoringView`, `ProductionLineView`, `AppController`에는 여전히 자동 테스트가 없다(대부분 표시 전용이라 우선순위는 낮음).

## 빌드/테스트 결과 요약

| 구성 | 결과 |
|---|---|
| Debug &#124; x64 | 빌드 성공, TC 56/56 통과(표준입력 고갈 상태에서도 정상 종료 확인) |
| Release &#124; x64 | 빌드 성공, 콘솔 앱 정상 실행 확인 |
| Debug &#124; Win32 | 빌드 성공 |
| Release &#124; Win32 | 빌드 성공 |
