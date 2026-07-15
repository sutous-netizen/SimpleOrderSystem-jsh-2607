# 반도체 시료 생산주문관리 시스템 (SimpleOrderSystem)

가상의 반도체 회사 "S-Semi"의 시료(Sample) 주문 · 생산 · 재고 · 출고 과정을 콘솔 애플리케이션으로 관리하는 개인과제 프로젝트다. 상세 배경과 기능 명세는 `[CRA_AI] Day3_개인과제_반도체시료관리.pdf`와 `docs/` 폴더를 기준으로 한다.

## 배경

S-Semi는 연구소·팹리스(Fabless) 업체·대학 연구실 등에 반도체 시료를 납품한다. 주문이 급증하면서 엑셀·메모장 기반 관리로는 처리 현황 파악, 재고 대비 공정 판단이 어려워졌고, 이를 해결하기 위해 콘솔 기반 생산주문관리 시스템을 개발한다.

## 역할별 흐름

```
고객 --(시료 요청: 이메일)--> 주문 담당자 --(주문서 전달)--> 생산 담당자
                                     ^                              |
                                     |______(승인 / 거절)___________|
```

- **고객**: 필요한 시료를 이메일로 요청
- **주문 담당자**: 요청에 맞게 주문서 작성 및 관리
- **생산 담당자**: 개발 시료 등록, 주문 수신 후 승인/거절 및 생산

## 전체 처리 흐름 및 주문 상태

```
주문 등록(RESERVED)
   │
   ▼
승인 여부 ──거절──▶ REJECTED
   │
  승인
   │
   ▼
재고 확인 ──재고 충분──▶ 출고 준비(CONFIRMED) ──▶ 출고 처리(RELEASE) ──▶ 고객 도달
   │
  재고 부족
   │
   ▼
생산 요청(PRODUCING) ──생산 완료──▶ 출고 준비(CONFIRMED)
```

| 상태 | 의미 |
|---|---|
| RESERVED | 주문 접수 |
| REJECTED | 주문 거절 (정상 흐름 외 상태, 모니터링에서 제외) |
| PRODUCING | 승인 완료, 재고 부족으로 생산 중 |
| CONFIRMED | 승인 완료, 출고 대기 중 |
| RELEASE | 출고 완료 |

## 주요 기능

| 메뉴 | 설명 |
|---|---|
| 시료 관리 | 시료 등록(ID, 이름, 평균 생산시간, 수율), 목록 조회(재고 포함), 이름 검색 |
| 시료 주문 | 시료 ID/고객명/수량으로 예약 접수 (`RESERVED`) |
| 주문 승인/거절 | `RESERVED` 목록 확인 후 승인(재고 충분 시 `CONFIRMED`, 부족 시 `PRODUCING`) 또는 거절(`REJECTED`) |
| 모니터링 | 상태별 주문 현황, 시료별 재고 현황(여유/부족/고갈) |
| 생산 라인 | 생산 큐(FIFO) 현황, 실 생산량·생산 시간 계산 및 표시 |
| 출고 처리 | `CONFIRMED` 주문의 출고 실행 (`RELEASE` 전환) |

### 계산 공식

- 수율 = 정상적인 시료 수 / 총 생산 시료 수
- 부족분 = 주문 수량 − 현재 재고
- 실 생산량 = `ceil(부족분 / 수율)`
- 총 생산 시간 = 평균 생산시간 × 실 생산량
- 생산라인은 한 번에 한 종류만 생산하며 나머지는 FIFO 대기 큐로 처리, 먼저 승인된 주문부터 생산

### 추가 요구사항

- 데이터 관리는 JSON으로 처리
- 프로그램 재실행 시 실제 경과 시간 기준으로 생산 완료 등을 반영 (백그라운드 동작 아님)
- 생산량은 수율을 반영해 실 생산량만큼 생산하고, 재고에는 실 생산량 전체를 반영
- 주문 승인 시점의 재고 현황만으로 승인 여부 판단

## 프로젝트 구조

Windows 콘솔 애플리케이션(C++20, MSBuild `v145` 툴셋)이며 MVC 패턴을 따른다.

```
SimpleOrderSystem/
  Model/        Types(OrderStatus, StockState, Sample, Order 등 공유 도메인 모델)
  Persistence/  IDataStore 인터페이스, JsonDataStore(JSON 기반 영속성)
  Monitor/      OrderService(주문/재고/생산 큐 도메인 로직)
  Console/      AppController 및 메뉴별 View(시료/주문/승인/모니터링/생산라인/출고)
  Dummy/        DummyDataGenerator(테스트용 더미 데이터 생성)
  Tests/        gtest/gmock 기반 TC + 공유 Mock(Tests/Mocks/MockDataStore.h)
  main.cpp      진입점
data/           samples.json, orders.json (런타임 자동 생성, git 미포함)
```

## 빌드 및 실행

MSBuild 또는 Visual Studio로 `SimpleOrderSystem.slnx`(새 XML 솔루션 형식)를 빌드한다 (CMake 등 크로스플랫폼 빌드 시스템 미사용).

```powershell
msbuild SimpleOrderSystem.slnx /p:Configuration=Debug /p:Platform=x64
msbuild SimpleOrderSystem.slnx /p:Configuration=Release /p:Platform=x64
```

- **Debug 빌드**(`x64\Debug\SimpleOrderSystem.exe`): gtest/gmock 기반 TC만 자동 수행하고 결과 코드로 즉시 종료한다(콘솔 앱은 실행되지 않는다) — TC 확인 전용.
- **Release 빌드**(`x64\Release\SimpleOrderSystem.exe`): 테스트 코드가 전혀 포함되지 않으며, 바로 콘솔 애플리케이션이 실행된다.
- `--seed` 옵션으로 실행하면 기본 더미 데이터셋(시료 12개, 상태별 주문 3건씩)을 생성한다: `SimpleOrderSystem.exe --seed`

## 테스트

테스트는 gtest/gmock(NuGet `gmock.1.11.0`)으로 작성되어 있으며 `Tests/` 폴더에 위치한다. 공유 `Tests/Mocks/MockDataStore.h`가 gmock 기반 `IDataStore` mock과 상태 보관용 fake를 함께 제공한다(gmock cookbook의 "Delegating Calls to a Fake" 패턴). Debug 빌드 실행이 곧 테스트 실행이다.

## 참고 문서

- [docs/01-개요.md](docs/01-개요.md) — 배경, 역할별 흐름도, 시스템 개요, 주문 상태 흐름
- [docs/02-메인메뉴.md](docs/02-메인메뉴.md) — 콘솔 메인 메뉴 구성
- [docs/03-시료관리.md](docs/03-시료관리.md) — 시료 등록/조회/검색
- [docs/04-시료주문.md](docs/04-시료주문.md) — 시료 예약(주문 접수)
- [docs/05-주문승인거절.md](docs/05-주문승인거절.md) — 주문 승인/거절 처리
- [docs/06-모니터링.md](docs/06-모니터링.md) — 상태별 주문 현황, 재고 현황
- [docs/07-생산라인.md](docs/07-생산라인.md) — 생산 큐(FIFO), 실 생산량/시간 계산
- [docs/08-출고처리.md](docs/08-출고처리.md) — 출고 처리(RELEASE 전환)
- [docs/09-미션및제출방법.md](docs/09-미션및제출방법.md) — PoC/프로젝트 개발 미션 및 제출 규칙
- [PRD.md](PRD.md) — 기능 명세를 통합 정리한 제품 요구사항 문서

## 미션 개요

과제는 2단계로 구성된다.

1. **PoC 개발**: MVC 스켈레톤 코드, 데이터 영속성 처리(CRUD 포함), 데이터 모니터링 Tool, Dummy 데이터 생성 Tool을 각각 개별 Repository로 제출
2. **프로젝트 개발**: Agentic Engineering을 도입하여 기능 명세를 충족하는 고품질 코드 개발 (CLAUDE.md/PRD.md 등 문서 관리, Harness 도입, Test, CleanCode, Commit 이력이 주안점)
