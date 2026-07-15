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
        } else if (IsInputExhausted()) {
            // 표준 입력이 고갈된 상태에서 무한 재시도하지 않도록 종료한다.
            std::cout << "입력이 종료되었습니다. 뒤로 갑니다.\n";
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
    // 시료명 컬럼은 한글이 섞이므로 std::setw(바이트 기준) 대신 PadDisplayWidth(화면 폭 기준)를 사용한다.
    constexpr int kSampleColumnWidth = 22;

    std::cout << "\n재고 현황\n";
    std::cout << std::left << PadDisplayWidth("시료명", kSampleColumnWidth) << PadDisplayWidth("재고", 10) << "상태\n";

    const auto inventory = orderService_.GetInventoryStatus();
    for (const auto& item : inventory) {
        std::cout << std::left
                   << PadDisplayWidth(item.sample.name, kSampleColumnWidth)
                   << std::setw(10) << (std::to_string(item.sample.stock) + " ea")
                   << Model::ToString(item.state) << "\n";
    }
}

} // namespace Console
