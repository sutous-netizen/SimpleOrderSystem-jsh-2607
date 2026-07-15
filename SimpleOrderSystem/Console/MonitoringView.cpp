#include "MonitoringView.h"
#include "ConsoleUtil.h"

#include <iostream>
#include <iomanip>

namespace Console {

MonitoringView::MonitoringView(Monitor::OrderService& orderService) : orderService_(orderService) {}

void MonitoringView::Run() {
    while (true) {
        std::cout << "\n[4] 모니터링  " << NowTimeString() << "\n\n";
        std::cout << "[1] 주문량 확인   [2] 재고량 확인   [0] 뒤로\n";
        const std::string choice = ReadLine("선택 > ");

        if (choice == "0") {
            return;
        } else if (choice == "1") {
            ShowOrderCounts();
        } else if (choice == "2") {
            ShowInventory();
        } else {
            std::cout << "잘못된 선택입니다.\n";
        }
    }
}

void MonitoringView::ShowOrderCounts() {
    std::cout << "\n상태별 주문 현황\n";
    const auto counts = orderService_.GetOrderCountsByStatus();
    for (const auto& item : counts) {
        std::cout << std::left << std::setw(11) << Model::ToString(item.status) << item.count << "건";
        if (item.status == Model::OrderStatus::PRODUCING) {
            std::cout << "  ← 생산라인 대기";
        }
        std::cout << "\n";
    }
}

void MonitoringView::ShowInventory() {
    std::cout << "\n재고 현황\n";
    std::cout << std::left << std::setw(22) << "시료명" << std::setw(10) << "재고" << "상태\n";

    const auto inventory = orderService_.GetInventoryStatus();
    for (const auto& item : inventory) {
        std::cout << std::left
                   << std::setw(22) << item.sample.name
                   << std::setw(10) << (std::to_string(item.sample.stock) + " ea")
                   << Model::ToString(item.state) << "\n";
    }
}

} // namespace Console
