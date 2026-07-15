# Phase 5 — Console 계층 견고성 점검 및 테스트 커버리지 확대

## 목표

지금까지 자동 테스트(gtest/gmock)는 Model/Persistence/Monitor/Dummy에만 있고 Console(View/Controller) 계층에는 전혀 없다. Console에는 이미 입력 검증 로직(빈 값 거부, 범위 검사, 존재하지 않는 시료 ID 거부, 재시도 루프 등)이 상당히 들어있는데(director가 확인함), 검증되지 않은 채로 남아있다. 이번 Phase는:

1. `Console/ConsoleUtil.h/.cpp`의 순수 함수(`Trim`, `TryParseInt64`, `TryParseDouble`, `NowDateCompact`, `NowTimeString`, `ReadLine`, `ReadYesNo`)에 대한 gtest 커버리지를 추가한다.
2. 최소 2개 View(주문 접수 `OrderMenuView`, 시료 등록 `SampleMenuView`)에 대해 "입력 → 검증/거부 또는 처리" 흐름을 실제로 검증하는 테스트를 추가한다(표시 문자열이 아니라 최종 상태로 검증).
3. 점검 중 실제 방어 로직 누락(예: 잘못된 입력에 대한 무한루프, 크래시 가능성)을 발견하면 수정한다.

## 공유 테스트 기법 (director가 정의)

### 표준 입력(cin) 스텁

`ReadLine`/`ReadYesNo`는 `std::cin`을 직접 사용하므로, 테스트에서는 `std::cin.rdbuf()`를 임시로 `std::istringstream`으로 바꿔치기해서 시나리오를 주입한다. 픽스처의 `SetUp`/`TearDown`에서 원래 버퍼를 저장/복원해라.

```cpp
class ConsoleUtilTest : public ::testing::Test {
protected:
    void SetInput(const std::string& text) {
        input_ = std::make_unique<std::istringstream>(text);
        originalCinBuf_ = std::cin.rdbuf(input_->rdbuf());
    }
    void TearDown() override {
        if (originalCinBuf_) { std::cin.rdbuf(originalCinBuf_); }
    }
private:
    std::unique_ptr<std::istringstream> input_;
    std::streambuf* originalCinBuf_ = nullptr;
};
```

### 표준 출력(cout) 캡처

gtest 내장 유틸리티를 사용한다(수동 streambuf 조작 불필요):
```cpp
testing::internal::CaptureStdout();
view.Run();
const std::string output = testing::internal::GetCapturedStdout();
// 필요한 경우에만 output에서 핵심 문구를 EXPECT_THAT(output, HasSubstr("..."))로 확인.
// 가능하면 화면 문구보다 "최종 상태"(store에 실제로 반영됐는지)로 검증하는 것을 우선한다.
```

### IDataStore/OrderService 준비

`Tests/Mocks/MockDataStore.h`의 `NiceMock<MockDataStore>` + `InMemoryDataStore` + `DelegateToFake()`를 그대로 사용한다. `Monitor::OrderService`는 인터페이스가 아니라 구체 클래스이므로 mock하지 않고, mock으로 감싼 `IDataStore` 위에 실제 `OrderService`를 그대로 올려서 "View + 실제 도메인 로직 + 가짜 저장소"로 통합 검증한다.

## 네가 할 일 (console 에이전트)

1. `Tests/ConsoleUtilTests.cpp` 신규 작성 — 최소 아래를 커버:
   - `Trim`: 앞뒤 공백/탭 제거, 빈 문자열, 공백만 있는 문자열.
   - `TryParseInt64`: 정상 정수, 음수, 앞뒤 공백 포함, 숫자가 아닌 문자, 숫자 뒤에 문자가 붙은 경우(`"12ab"`) 실패해야 함, 빈 문자열 실패.
   - `TryParseDouble`: 정상 소수, 정수, 잘못된 형식 실패.
   - `NowDateCompact`: 정확히 8자리 숫자 문자열 형식인지(정규식 또는 길이+`isdigit` 검사, 실제 날짜값은 검증하지 않아도 됨).
   - `ReadLine`: 표준 입력 스텁으로 `"  hello  \n"`을 주입하면 트림된 `"hello"`를 반환하는지.
   - `ReadYesNo`: `"y\n"`/`"N\n"` 각각 true/false 반환, 잘못된 입력(`"x\n"`) 후 재시도해서 결국 유효한 값을 받으면 정상 반환하는지(무한루프 방지 확인 겸).
2. `Tests/ConsoleViewTests.cpp` 신규 작성 — 아래 시나리오를 실제 상태 변화로 검증(문구 매칭은 최소화):
   - `OrderMenuView`: 존재하지 않는 시료 ID 입력 시 주문이 생성되지 않는지(`fake.LoadOrders()`가 비어있음을 확인).
   - `OrderMenuView`: 유효한 입력 후 `[N]`(취소) 응답 시 주문이 생성되지 않는지.
   - `OrderMenuView`: 유효한 입력 후 `[Y]` 응답 시 `RESERVED` 상태의 주문이 실제로 저장되는지(개수/필드 확인).
   - `SampleMenuView`: 이미 존재하는 시료 ID로 등록 시도 시 시료 목록이 변하지 않는지(중복 등록 방지).
   - `SampleMenuView`: 잘못된 수율(예: `"2"`, `"0"`, `"abc"`)을 먼저 입력했다가 유효한 값(`"0.9"`)을 입력하면 재시도 루프를 거쳐 정상 등록되는지(무한루프가 되지 않고 유효 입력에서 종료됨을 확인).
3. 점검 중 실제로 크래시/무한루프/방어 로직 누락을 발견하면 해당 View/`ConsoleUtil` 코드를 직접 수정해라(발견한 문제와 수정 내용을 보고에 반드시 포함).
4. 나머지 View(`ApprovalMenuView`, `MonitoringView`, `ProductionLineView`, `ReleaseMenuView`)는 이번 Phase 범위 밖이다 — 코드 변경도, 테스트 추가도 하지 않는다(과잉 확장 금지, YAGNI).
5. **테스트 이름 규칙**: `TEST`/`TEST_F`의 SuiteName/CaseName은 영문 PascalCase만 사용하고, 각 테스트 위에 한글 설명 주석을 남겨라(Phase 3에서 확립한 규칙과 동일).
6. C++20, clean code/OOP 원칙 적용.

## 하지 마라

- vcxproj/.vcxproj.filters/main.cpp 수정 금지 — director가 통합(신규 테스트 파일 2개를 Debug 전용 ItemGroup에 등록할 예정).
- `Monitor/OrderService.h/.cpp`, `Persistence/*`, `Model/*`, `Tests/Mocks/MockDataStore.h` 수정 금지.
- `ApprovalMenuView`/`MonitoringView`/`ProductionLineView`/`ReleaseMenuView`는 건드리지 마라(범위 밖).

## 진행 기록

- [x] 공유 테스트 기법(cin 스텁, cout 캡처, mock 조합) 정의 (director)
- [ ] console: ConsoleUtilTests.cpp, ConsoleViewTests.cpp 작성 및 발견된 방어 로직 누락 수정
- [ ] 통합: vcxproj 등록(Debug 전용), 빌드/실행 검증(Debug TC 전체 통과, Release 정상 동작)
- [ ] docs/temp 요약, 커밋 메시지 확인 후 commit/push
