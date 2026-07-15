#pragma once
// [3] 주문 승인/거절 화면 (docs/05-주문승인거절.md)

#include "../Persistence/IDataStore.h"
#include "../Monitor/OrderService.h"

namespace Console {

class ApprovalMenuView {
public:
    ApprovalMenuView(Persistence::IDataStore& store, Monitor::OrderService& orderService);

    // RESERVED 목록 표시 -> 번호 선택 -> 승인/거절
    void Run();

private:
    Persistence::IDataStore& store_;
    Monitor::OrderService& orderService_;
};

} // namespace Console
