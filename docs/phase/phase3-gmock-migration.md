# Phase 3 — TC를 gmock/gtest로 마이그레이션, Debug 빌드는 TC만 수행

## 목표

`director.md`에 추가된 두 원칙을 실제 코드에 반영한다.

1. TC(테스트 코드)는 gmock(Google Test/Google Mock, NuGet `gmock.1.11.0`으로 이미 설치됨)을 사용해서 작성한다 — 자체 `Tests/TestFramework.h`는 폐기한다.
2. Debug 빌드 실행 시에는 TC만 자동 수행되고 종료한다 — 지금처럼 TC 실행 후 콘솔 앱이 이어서 실행되지 않는다. Release 빌드에는 테스트 코드/프레임워크가 아예 포함되지 않는다(제품 바이너리에 테스트 프레임워크가 섞이지 않도록).

## 공유 산출물 (director가 정의)

`SimpleOrderSystem/Tests/Mocks/MockDataStore.h` — gmock 기반 `Persistence::IDataStore` 목(mock)과, 상태를 실제로 들고 있는 `InMemoryDataStore`(위임 대상)를 함께 제공한다. "gmock의 Delegating Calls to a Fake" 패턴을 사용해, 기존 손수 작성한 Fake/InMemoryDataStore들의 상태 기반 동작은 유지하면서도 실제 `MOCK_METHOD`/`EXPECT_CALL`을 쓸 수 있게 한다.

```cpp
namespace TestMocks {

class MockDataStore : public Persistence::IDataStore {
public:
    MOCK_METHOD(std::vector<Model::Sample>, LoadSamples, (), (const, override));
    MOCK_METHOD(void, SaveSamples, (const std::vector<Model::Sample>&), (override));
    MOCK_METHOD(std::optional<Model::Sample>, FindSampleById, (const std::string&), (const, override));
    MOCK_METHOD(std::vector<Model::Order>, LoadOrders, (), (const, override));
    MOCK_METHOD(void, SaveOrders, (const std::vector<Model::Order>&), (override));
    MOCK_METHOD(std::optional<Model::Order>, FindOrderByNo, (const std::string&), (const, override));
    MOCK_METHOD(std::string, NextOrderNo, (const std::string&), (override));
};

class InMemoryDataStore : public Persistence::IDataStore { /* 상태를 실제로 보관하는 단순 구현 */ };

// mock의 모든 호출을 fake로 위임(ON_CALL...WillByDefault(Invoke(...))).
void DelegateToFake(::testing::NiceMock<MockDataStore>& mock, InMemoryDataStore& fake);

} // namespace TestMocks
```

사용 예: `::testing::NiceMock<TestMocks::MockDataStore> mock; TestMocks::InMemoryDataStore fake; TestMocks::DelegateToFake(mock, fake);` — 이후 `mock`을 `Monitor::OrderService`/`Dummy::DummyDataGenerator` 생성자에 그대로 넘기면 상태 기반 테스트가 가능하고, 필요한 곳에서 `EXPECT_CALL(mock, SaveOrders(_)).Times(...)` 같은 상호작용 검증도 추가할 수 있다.

## 테스트 이름 규칙 (반드시 준수)

gtest의 `TEST(SuiteName, CaseName)`은 유효한 C++ 식별자여야 하므로 **영문 PascalCase/CamelCase만 사용**한다(한글 식별자 금지 — 툴체인 호환성 위험). 각 테스트 위에 원래 의도를 설명하는 한글 주석 한 줄을 남겨서 가독성을 유지한다.

예:
```cpp
// 재고가 충분하면 승인 즉시 CONFIRMED로 전환되고 재고가 차감된다.
TEST(OrderServiceTest, ApproveWithSufficientStock_TransitionsToConfirmedAndDeductsStock) {
    ...
}
```

## 에이전트별 작업

### persistent — `Tests/ModelTests.cpp`, `Tests/PersistenceTests.cpp`
- 기존 `TestFramework.h` 기반 테스트(Model 6건 + Persistence 4건, 총 10건)를 gtest(`#include <gtest/gtest.h>`, `TEST`, `EXPECT_EQ`, `EXPECT_THROW` 등)로 1:1 마이그레이션한다(테스트 커버리지 축소 금지, 위 이름 규칙 적용).
- `PersistenceTests.cpp`는 실제 `Persistence::JsonDataStore`(파일 기반)를 그대로 테스트하므로 mock이 필요 없다 — 기존 임시 파일 픽스처 로직을 gtest 스타일(예: `TEST` 내부에서 setup/teardown, 또는 `TEST_F` + `Fixture`)로 정리한다.
- `Model`은 `IDataStore`와 무관하므로 mock 불필요.

### monitor — `Tests/MonitorTests.cpp`
- 기존 12건(Phase1 9건 + Phase2 3건)을 gtest로 마이그레이션한다.
- 기존에 이 파일 안에 직접 정의했던 `InMemoryDataStore`(Fake)를 제거하고, `Tests/Mocks/MockDataStore.h`의 `NiceMock<MockDataStore>` + `InMemoryDataStore` + `DelegateToFake()`로 교체한다.
- 최소 1건은 `EXPECT_CALL`로 실제 상호작용을 검증하는 테스트로 다시 작성한다(예: "승인 시 SaveOrders가 최소 1회 호출된다" 같은 상태 변경 저장 여부 검증) — gmock을 실질적으로 활용했음을 보여주는 테스트.

### dummy — `Tests/DummyTests.cpp`
- 기존 9건을 gtest로 마이그레이션한다. 이 파일 안에 직접 정의했던 `FakeDataStore`를 제거하고 동일하게 `Tests/Mocks/MockDataStore.h`를 사용한다.

## 통합 (director)

1. `SimpleOrderSystem/Tests/Mocks/MockDataStore.h` 작성.
2. `main.cpp` 수정: Debug 빌드에서는 `::testing::InitGoogleMock(&argc, argv)` 후 `RUN_ALL_TESTS()` 결과를 그대로 `return`하고 콘솔 앱을 실행하지 않는다. Release 빌드에는 테스트 관련 코드/헤더가 아예 포함되지 않게 한다(`#ifdef _DEBUG`로 분리).
3. `SimpleOrderSystem.vcxproj` 수정: `gmock.targets` import와 `Tests/*.cpp` 컴파일 대상을 `Condition="'$(Configuration)'=='Debug'"`로 제한해 Release 빌드에는 테스트 프레임워크/코드가 전혀 포함되지 않게 한다.
4. `Tests/TestFramework.h`를 삭제하고 vcxproj/filters에서 참조를 제거한다.
5. 4개 구성 빌드 검증: Debug는 TC만 실행 후 즉시 종료(콘솔 메뉴로 진입하지 않음)하는지 확인, Release는 정상적으로 콘솔 메뉴가 바로 뜨는지 확인.
6. docs/temp에 Phase 3 요약(적용한 OOP/clean code 기법, superpowers 활용 내역 포함) 작성.
7. **커밋/ push 전 커밋 메시지를 사용자에게 확인받는다** (director.md 원칙).

## 진행 기록

- [x] 공유 Mock 계약 정의 (director)
- [ ] persistent: ModelTests/PersistenceTests gtest 마이그레이션
- [ ] monitor: MonitorTests gtest+gmock 마이그레이션
- [ ] dummy: DummyTests gtest+gmock 마이그레이션
- [ ] 통합: main.cpp Debug-only TC, vcxproj 조건부 구성, TestFramework.h 제거, 빌드/실행 검증
- [ ] docs/temp 요약, 커밋 메시지 확인 후 commit/push
