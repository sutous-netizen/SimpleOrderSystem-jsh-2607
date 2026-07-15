#pragma once
// [6] 출고 처리 화면 (docs/08-출고처리.md)

#include "../Persistence/IDataStore.h"
#include "../Monitor/OrderService.h"

namespace Console {

class ReleaseMenuView {
public:
    ReleaseMenuView(Persistence::IDataStore& store, Monitor::OrderService& orderService);

    // CONFIRMED 목록 표시 -> 번호 선택 -> 출고 처리
    void Run();

private:
    Persistence::IDataStore& store_;
    Monitor::OrderService& orderService_;
};

} // namespace Console
