# CLAUDE.md

이 파일은 이 저장소에서 작업할 때 Claude Code(claude.ai/code)에게 제공되는 가이드입니다.

## 프로젝트 현황

이 저장소는 현재 뼈대만 갖춘 빈 Visual Studio C++ 프로젝트만 존재하는 상태입니다 — 소스 파일, README, 테스트, 커밋이 아직 하나도 없습니다. `SimpleOrderSystem/SimpleOrderSystem.vcxproj`가 프로젝트를 정의하고 있지만 `<ClCompile>`/`<ClInclude>` 항목은 비어 있습니다.

- 프로젝트 유형: Windows 콘솔 애플리케이션 (`ConfigurationType=Application`, `SubSystem=Console`)
- 언어 표준: C++20 (`LanguageStandard=stdcpp20`)
- 플랫폼 툴셋: `v145`, Win32/x64, Debug/Release 대상
- 대상 플랫폼: Windows 10 이상 (`WindowsTargetPlatformVersion=10.0`)

첫 소스 파일을 추가할 때는 `SimpleOrderSystem.vcxproj.filters`에서 `.cpp` 파일은 "소스 파일" 필터 그룹에, 헤더 파일은 "헤더 파일" 필터 그룹에 넣고(필터 파일은 한글 라벨 사용), `SimpleOrderSystem.vcxproj`에도 대응하는 `<ClCompile>`/`<ClInclude>` 항목을 추가해야 합니다.

## 기능 명세 문서

`docs/` 폴더에 과제 안내 자료(`[CRA_AI] Day3_개인과제_반도체시료관리.pdf`)를 기반으로 작성한 기능 명세가 있습니다. 기능 구현 전 반드시 해당 문서를 먼저 확인하세요.

- `docs/01-개요.md` : 배경, 역할별 흐름도, 시스템 개요, 주문 상태(RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE) 흐름
- `docs/02-메인메뉴.md` : 콘솔 메인 메뉴 구성
- `docs/03-시료관리.md` : 시료 등록/조회/검색 (시료 ID, 이름, 평균 생산시간, 수율)
- `docs/04-시료주문.md` : 시료 예약(주문 접수)
- `docs/05-주문승인거절.md` : 주문 승인(재고 충분 시 CONFIRMED, 부족 시 PRODUCING)/거절(REJECTED) 처리
- `docs/06-모니터링.md` : 상태별 주문 현황, 시료별 재고 현황(여유/부족/고갈)
- `docs/07-생산라인.md` : 생산 큐(FIFO), 실 생산량(`ceil(부족분/수율)`) 및 생산 시간 계산
- `docs/08-출고처리.md` : CONFIRMED 주문의 출고 처리(RELEASE 전환)
- `docs/09-미션및제출방법.md` : PoC/프로젝트 개발 미션 및 Repository 제출 규칙

## 빌드

MSBuild 또는 Visual Studio로 빌드합니다. CMake 등 크로스플랫폼 빌드 시스템은 사용하지 않습니다.

```powershell
msbuild SimpleOrderSystem.sln /p:Configuration=Debug /p:Platform=x64
```

테스트 프레임워크, 린트 설정, CI는 아직 구성되어 있지 않습니다.

## commit 정책
제목 해더에 아래 키워드로 시작한다.
[test] : 테스트 코드 반영
[fix] : bug 수정
[feat] : 기능 구현
[refac] : 코드 리팩토링
[etc] : 기타 정보를 "[etc]" 부분에 단어로 대체해서 표시

commit message 는 아래 내용으로 한다.
- 요약 정보
- 빌드결과
- 테스트 결과 표시