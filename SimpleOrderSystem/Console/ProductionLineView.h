#pragma once
// [5] 생산라인 조회 화면 (docs/07-생산라인.md)

#include "../Monitor/OrderService.h"

namespace Console {

class ProductionLineView {
public:
    explicit ProductionLineView(Monitor::OrderService& orderService);

    // 생산 큐(FIFO) 현황 출력
    void Run();

private:
    Monitor::OrderService& orderService_;
};

} // namespace Console
