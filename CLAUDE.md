# CLAUDE.md

이 파일은 이 저장소에서 작업할 때 Claude Code(claude.ai/code)에게 제공되는 가이드입니다.

## 프로젝트 현황

이 저장소는 현재 뼈대만 갖춘 빈 Visual Studio C++ 프로젝트만 존재하는 상태입니다 — 소스 파일, README, 테스트, 커밋이 아직 하나도 없습니다. `SimpleOrderSystem/SimpleOrderSystem.vcxproj`가 프로젝트를 정의하고 있지만 `<ClCompile>`/`<ClInclude>` 항목은 비어 있습니다.

- 프로젝트 유형: Windows 콘솔 애플리케이션 (`ConfigurationType=Application`, `SubSystem=Console`)
- 언어 표준: C++20 (`LanguageStandard=stdcpp20`)
- 플랫폼 툴셋: `v145`, Win32/x64, Debug/Release 대상
- 대상 플랫폼: Windows 10 이상 (`WindowsTargetPlatformVersion=10.0`)

첫 소스 파일을 추가할 때는 `SimpleOrderSystem.vcxproj.filters`에서 `.cpp` 파일은 "소스 파일" 필터 그룹에, 헤더 파일은 "헤더 파일" 필터 그룹에 넣고(필터 파일은 한글 라벨 사용), `SimpleOrderSystem.vcxproj`에도 대응하는 `<ClCompile>`/`<ClInclude>` 항목을 추가해야 합니다.

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
[xxx] : 기타 정보 표시

commit message 는 아래 내용으로 한다.
- 요약 정보
- 빌드결과
- 테스트 결과 표시