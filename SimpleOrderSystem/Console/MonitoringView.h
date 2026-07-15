#pragma once
// [4] 모니터링 화면 (docs/06-모니터링.md)

#include "../Monitor/OrderService.h"

namespace Console {

class MonitoringView {
public:
    explicit MonitoringView(Monitor::OrderService& orderService);

    // [1] 주문량 확인   [2] 재고량 확인   [0] 뒤로
    void Run();

private:
    void ShowOrderCounts();
    void ShowInventory();

    Monitor::OrderService& orderService_;
};

} // namespace Console
