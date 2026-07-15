# Phase 3 요약 — TC를 gmock/gtest로 마이그레이션, Debug 빌드는 TC만 수행

## 개요

`director.md`에 추가된 원칙(TC는 gmock 사용, Debug 빌드는 TC만 자동 수행)을 실제 코드에 반영했다. 계획 문서: `docs/phase/phase3-gmock-migration.md`.

## 공유 산출물 (director)

`Tests/Mocks/MockDataStore.h` — gmock cookbook의 "Delegating Calls to a Fake" 패턴 적용:
- `TestMocks::MockDataStore`: `MOCK_METHOD` 기반 `Persistence::IDataStore` 목.
- `TestMocks::InMemoryDataStore`: 상태를 실제로 보관하는 단순 fake.
- `TestMocks::DelegateToFake(NiceMock<MockDataStore>&, InMemoryDataStore&)`: mock 호출을 전부 fake로 위임하는 헬퍼.

이 조합 덕분에 각 에이전트가 개별적으로 손수 작성했던 Fake(`InMemoryDataStore`/`FakeDataStore`) 중복 정의를 제거하고, 상태 기반 검증과 gmock의 `EXPECT_CALL` 상호작용 검증을 동시에 지원할 수 있게 됐다.

## 에이전트별 마이그레이션 결과 (총 32개 TC, 기존 31개 + 상호작용 검증 1건 신규)

### persistent — `Tests/ModelTests.cpp`(6건), `Tests/PersistenceTests.cpp`(4건)
- Model 테스트는 `IDataStore`와 무관하므로 mock 없이 순수 gtest로 작성.
- Persistence 테스트는 실제 `JsonDataStore`(파일 기반)를 그대로 검증(통합 테스트 성격이라 mock 불필요), 임시 파일 RAII 패턴을 `TEST_F` 픽스처(`SetUp`/`TearDown`)로 재구성.
- **적용 기법**: Extract Fixture 리팩토링(중복 셋업 제거), `ASSERT_*`/`EXPECT_*` 용도 구분(선행조건 vs 값 검증)으로 진단 정보 최적화.

### monitor — `Tests/MonitorTests.cpp`(12건 + 신규 1건 = 13건)
- 자체 `InMemoryDataStore`를 제거하고 공유 `Tests/Mocks/MockDataStore.h` 사용.
- 신규 `OrderServiceInteractionTest.ApproveOrder_InvokesSaveOrdersOnDataStore`: `EXPECT_CALL(mock, SaveOrders(_)).Times(AtLeast(1))`로 승인 처리 시 저장소 호출이 실제로 발생하는지 검증 — 상태 기반 검증과 상호작용 검증을 결합한 이번 마이그레이션의 핵심 사례.
- **적용 기법**: Test Fixture(`OrderServiceTest`)로 공통 셋업 캡슐화(DRY), Delegating Mock 패턴으로 상태/행위 검증의 관심사 분리.

### dummy — `Tests/DummyTests.cpp`(9건)
- 자체 `FakeDataStore`를 제거하고 공유 mock 사용, `DummyDataGeneratorTest` 픽스처로 셋업 통합.
- **적용 기법**: 의존성 역전(테스트가 구체 타입이 아닌 `IDataStore` 인터페이스에만 의존 — `DummyDataGenerator` 자신의 설계 원칙과 동일하게 일관성 유지).

## 테스트 이름 규칙

gtest `TEST(SuiteName, CaseName)`은 유효한 C++ 식별자여야 하므로 전부 영문 PascalCase로 통일하고(예: `기존 생산큐_FIFO_순서_반환` → `OrderServiceTest.GetProductionQueue_ReturnsFifoOrder`), 각 테스트 위에 원래 의도를 설명하는 한글 주석 한 줄을 유지해 가독성과 툴체인 호환성을 모두 확보했다.

## Director 통합 작업

1. `Tests/Mocks/MockDataStore.h` 작성(공유 계약).
2. `main.cpp`: Debug 빌드에서 `::testing::InitGoogleMock(&argc, argv); return RUN_ALL_TESTS();`로 TC만 수행 후 즉시 종료하도록 변경(콘솔 앱 미실행). Release 빌드는 기존과 동일하게 콘솔 앱을 바로 실행.
3. `SimpleOrderSystem.vcxproj`: `Tests/*.cpp` 컴파일 대상과 `gmock.targets` import(gtest-all.cc/gmock-all.cc 컴파일 포함)를 `Condition="'$(Configuration)'=='Debug'"`로 제한 — Release 빌드에는 테스트 프레임워크가 전혀 포함되지 않는다.
4. 자체 제작했던 `Tests/TestFramework.h`를 삭제하고 vcxproj/filters에서 참조 제거.
5. 4개 구성(Debug/Release × Win32/x64) 빌드 성공 확인.
6. 실행 검증:
   - Debug|x64: `[==========] 32 tests from 6 test suites ran. [PASSED] 32 tests.` 출력 후 즉시 종료(콘솔 메뉴로 진입하지 않음) 확인.
   - Release|x64: TC 없이 바로 콘솔 메인 메뉴 진입 확인, 빌드 산출물에 gtest/gmock 오브젝트가 전혀 없음을 확인.

## superpowers / TDD 활용 메모

이번 Phase는 이미 통과하던 테스트를 프레임워크만 교체하는 리팩토링이 대부분이라 신규 Red-Green-Refactor 사이클은 크게 수행하지 않았으나, monitor 에이전트가 신규 추가한 상호작용 검증 테스트(`ApproveOrder_InvokesSaveOrdersOnDataStore`)는 "이 동작이 저장소 호출로 이어지는가"라는 명세를 먼저 정의하고 그에 맞춰 작성하는 TDD적 접근을 취했다.

## 확인된 이슈 / 후속 조치 필요 사항

- `main.cpp`의 Debug 분기(`#ifdef _DEBUG ... return ...; #endif`) 이후에 이어지는 코드(콘솔 앱 실행부)는 Debug 빌드에서는 도달 불가능한 코드로 컴파일된다(컴파일 경고 가능성 있음, 빌드 실패 아님) — 다음 단계에서 `#else` 분기로 더 명시적으로 정리할 여지가 있다.
- gmock 관련 NuGet 패키지(`packages/gmock.1.11.0`)는 Release 구성에서는 더 이상 컴파일되지 않지만, 여전히 저장소에는 `packages.config`로 참조가 남아있다(정상 — Debug 빌드에 필요).

## 빌드/테스트 결과 요약

| 구성 | 결과 |
|---|---|
| Debug &#124; x64 | 빌드 성공, TC 32/32 통과 후 즉시 종료(콘솔 앱 미실행) |
| Release &#124; x64 | 빌드 성공, TC 없이 바로 콘솔 메뉴 진입, 테스트 코드 미포함 확인 |
| Debug &#124; Win32 | 빌드 성공 |
| Release &#124; Win32 | 빌드 성공 |
