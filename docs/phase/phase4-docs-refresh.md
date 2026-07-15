# Phase 4 — 문서 최신화 (docs 관리)

## 목표

Phase 1~3에서 실제 구현이 완료되었음에도 `CLAUDE.md`가 여전히 "빈 스켈레톤 프로젝트" 상태를 설명하고 있고, `docs/09-미션및제출방법.md`가 요구하는 `PRD.md`가 없었다. 이번 Phase는 서브에이전트 디스패치 없이 director가 직접 수행하는 문서 정리 작업이다.

## 작업 내용

1. `CLAUDE.md`
   - "프로젝트 현황" 섹션을 실제 구현 상태(Phase 1~3 완료, MVC 구조, 계층별 담당, 테스트/인코딩/데이터/시드 방식)로 갱신.
   - "빌드" 섹션을 `.sln` → `SimpleOrderSystem.slnx` 기준으로 갱신하고, Debug(TC 전용 실행)/Release(콘솔 앱) 구분을 명시.
   - "기능 명세 문서" 목록에 `PRD.md`, `docs/phase/`, `docs/temp/` 참조 추가.
2. `PRD.md` 신규 작성 — `docs/01`~`08`의 기능 명세를 배경/역할/도메인 모델/기능 요구사항/계산 공식/비기능 요구사항/아키텍처/범위 밖(Out of Scope)으로 통합 정리.
3. `README.md` 갱신 — 프로젝트 구조에 `Tests/Mocks/` 반영, 빌드 섹션에 `.slnx` + Debug(TC 전용)/Release(콘솔 앱) 구분 + `--seed` 옵션 안내, 테스트 섹션(gtest/gmock) 신설, 참고 문서에 `PRD.md` 추가.

## 진행 기록

- [x] CLAUDE.md 갱신
- [x] PRD.md 작성
- [x] README.md 갱신
- [x] docs/temp 요약 및 commit/push (커밋 메시지 확인 필요)
