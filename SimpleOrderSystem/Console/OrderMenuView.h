#pragma once
// [2] 시료 주문 화면 (docs/04-시료주문.md)

#include "../Persistence/IDataStore.h"
#include "../Monitor/OrderService.h"

namespace Console {

class OrderMenuView {
public:
    OrderMenuView(Persistence::IDataStore& store, Monitor::OrderService& orderService);

    // 시료 ID/고객명/수량 입력 -> 확인 -> 예약 접수
    void Run();

private:
    Persistence::IDataStore& store_;
    Monitor::OrderService& orderService_;
};

} // namespace Console
