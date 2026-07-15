#pragma once
// [1] 시료 관리 화면 (docs/03-시료관리.md)
// 시료 등록/목록/검색은 도메인 규칙이 거의 없는 단순 CRUD이므로 IDataStore 를 직접 사용한다.

#include "../Persistence/IDataStore.h"

namespace Console {

class SampleMenuView {
public:
    explicit SampleMenuView(Persistence::IDataStore& store);

    // 하위 메뉴 루프 실행 ([1] 등록 [2] 목록 [3] 검색 [0] 뒤로)
    void Run();

private:
    void ShowRegister();
    void ShowList();
    void ShowSearch();
    void PrintSampleTable(const std::vector<Model::Sample>& samples) const;

    Persistence::IDataStore& store_;
};

} // namespace Console
