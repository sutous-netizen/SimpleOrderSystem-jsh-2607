# Phase 8 — 전체 코드 최종 리뷰 및 잔여 마이너 이슈 정리

## 목표

Phase 1~7은 매번 특정 범위(스켈레톤, 생산라인, 테스트 마이그레이션, 문서, Console 견고성, 기술부채)만 검토했다. 전체가 완성된 지금, 처음으로 전체 코드베이스를 한 번에 훑어 스펙(docs/01~08) 대비 누락·버그·일관성 문제를 독립적인 시각에서 점검한다. 아울러 Phase 6에서 남겨뒀던 표 헤더 라벨("번호", "주문번호") 정렬도 마무리한다.

## 절차

1. 독립 리뷰 에이전트(이번 Phase 전용, 특정 서브에이전트 정체성 없이 신선한 시각으로)를 디스패치해 `SimpleOrderSystem/` 전체(Model/Persistence/Monitor/Console/Dummy/Tests/main.cpp)를 docs/01~08 스펙과 대조하며 검토하게 한다.
2. 발견된 항목을 심각도(Critical/Important/Minor)로 분류해 보고받는다.
3. Critical/Important는 director가 직접 수정하거나 해당 소유 에이전트에게 재위임한다. Minor는 기록만 하고 넘어간다(과잉 대응 금지).
4. 표 헤더 라벨("번호", "주문번호")에 `Console::PadDisplayWidth`를 적용해 Phase 6에서 남겨뒀던 정렬 이슈를 마무리한다(director가 직접, 작은 변경이라 재위임 없이 처리).
5. 4개 구성 빌드 검증, TC 전체 통과 확인.
6. docs/temp 요약, 커밋 메시지 확인 후 commit/push.

## 진행 기록

- [ ] 전체 코드 리뷰 디스패치 및 findings 수집
- [ ] Critical/Important 수정
- [ ] 헤더 라벨 정렬 마무리 (director)
- [ ] 빌드/테스트 검증
- [ ] docs/temp 요약, 커밋 메시지 확인 후 commit/push
