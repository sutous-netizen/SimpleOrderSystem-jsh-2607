#include "AppController.h"
#include "ConsoleUtil.h"
#include "SampleMenuView.h"
#include "OrderMenuView.h"
#include "ApprovalMenuView.h"
#include "MonitoringView.h"
#include "ProductionLineView.h"
#include "ReleaseMenuView.h"

#include <iostream>

namespace Console {

AppController::AppController(Persistence::IDataStore& store, Monitor::OrderService& orderService)
    : store_(store), orderService_(orderService) {}

void AppController::Run() {
    // 재실행 시 실제 경과 시간을 반영해 완료된 생산을 처리 (docs/01-개요.md 추가요구사항)
    orderService_.CatchUpProduction();

    while (true) {
        ShowMainMenu();
        const std::string choice = ReadLine("\n 선택 > ");

        if (choice == "0") {
            std::cout << "\n프로그램을 종료합니다.\n";
            return;
        } else if (choice == "1") {
            SampleMenuView(store_).Run();
        } else if (choice == "2") {
            OrderMenuView(store_, orderService_).Run();
        } else if (choice == "3") {
            ApprovalMenuView(store_, orderService_).Run();
        } else if (choice == "4") {
            MonitoringView(orderService_).Run();
        } else if (choice == "5") {
            ProductionLineView(orderService_).Run();
        } else if (choice == "6") {
            ReleaseMenuView(store_, orderService_).Run();
        } else {
            std::cout << "\n잘못된 선택입니다. 다시 입력하세요.\n";
        }
    }
}

void AppController::ShowMainMenu() {
    const std::vector<Model::Sample> samples = store_.LoadSamples();
    const std::vector<Model::Order> orders = store_.LoadOrders();

    int64_t totalStock = 0;
    for (const auto& sample : samples) {
        totalStock += sample.stock;
    }

    int64_t producingCount = 0;
    for (const auto& order : orders) {
        if (order.status == Model::OrderStatus::PRODUCING) {
            ++producingCount;
        }
    }

    std::cout << "\n반도체 시료 생산주문관리 시스템\n";
    std::cout << "==================================================\n";
    std::cout << " 시스템 현황  " << NowTimeString() << "\n\n";
    std::cout << " 등록 시료  " << samples.size() << "종     총 재고   " << totalStock << " ea\n";
    std::cout << " 전체 주문  " << orders.size() << "건     생산라인  " << producingCount << "건 대기\n";
    std::cout << "--------------------------------------------------\n";
    std::cout << " [1] 시료 관리        [2] 시료 주문\n";
    std::cout << " [3] 주문 승인/거절    [4] 모니터링\n";
    std::cout << " [5] 생산라인 조회     [6] 출고 처리\n";
    std::cout << " [0] 종료\n";
}

} // namespace Console
