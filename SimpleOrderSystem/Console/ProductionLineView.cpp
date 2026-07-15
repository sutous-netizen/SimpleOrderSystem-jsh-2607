#include "ProductionLineView.h"
#include "ConsoleUtil.h"

#include <iostream>
#include <iomanip>

namespace Console {

ProductionLineView::ProductionLineView(Monitor::OrderService& orderService) : orderService_(orderService) {}

void ProductionLineView::Run() {
    std::cout << "\n[5] 생산라인 조회  FIFO 방식\n\n";

    const auto queue = orderService_.GetProductionQueue();
    if (queue.empty()) {
        std::cout << "현재 생산 대기 중인 주문이 없습니다.\n";
        return;
    }

    std::cout << "생산 큐 " << queue.size() << "건 대기 중 (FIFO 순)\n\n";
    std::cout << std::left
               << std::setw(6) << "순서"
               << std::setw(12) << "주문번호"
               << std::setw(22) << "시료"
               << std::setw(10) << "주문량"
               << std::setw(10) << "부족분"
               << std::setw(10) << "실생산량"
               << "총생산시간\n";

    for (size_t i = 0; i < queue.size(); ++i) {
        const auto& item = queue[i];
        std::cout << std::left
                   << std::setw(6) << (std::to_string(i + 1))
                   << std::setw(12) << item.order.orderNo
                   << std::setw(22) << item.sample.name
                   << std::setw(10) << (std::to_string(item.order.quantity) + " ea")
                   << std::setw(10) << (std::to_string(item.order.shortage) + " ea")
                   << std::setw(10) << (std::to_string(item.order.actualProductionQty) + " ea")
                   << (std::to_string(static_cast<int64_t>(item.order.totalProductionTimeMin)) + " min") << "\n";
    }

    std::cout << "\n* 부족분 = 주문량 - 재고, 실생산량 = ceil(부족분 / 수율)\n";
    std::cout << "* 선입선출(FIFO) 방식으로 처리됩니다.\n";
}

} // namespace Console
