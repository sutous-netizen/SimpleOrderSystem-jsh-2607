#include "OrderMenuView.h"
#include "ConsoleUtil.h"

#include <iostream>

namespace Console {

OrderMenuView::OrderMenuView(Persistence::IDataStore& store, Monitor::OrderService& orderService)
    : store_(store), orderService_(orderService) {}

void OrderMenuView::Run() {
    std::cout << "\n[2] 시료 주문\n\n";

    const std::string sampleId = ReadLine("시료 ID    > ");
    if (sampleId.empty()) {
        std::cout << "시료 ID 는 비워둘 수 없습니다. 주문을 취소합니다.\n";
        return;
    }

    const auto sampleOpt = store_.FindSampleById(sampleId);
    if (!sampleOpt.has_value()) {
        std::cout << "등록되지 않은 시료 ID 입니다: " << sampleId << "\n";
        return;
    }

    const std::string customerName = ReadLine("고객명     > ");
    if (customerName.empty()) {
        std::cout << "고객명은 비워둘 수 없습니다. 주문을 취소합니다.\n";
        return;
    }

    int64_t quantity = 0;
    while (true) {
        const std::string text = ReadLine("주문 수량  > ");
        if (TryParseInt64(text, quantity) && quantity > 0) {
            break;
        }
        std::cout << "0보다 큰 정수를 입력하세요.\n";
    }

    std::cout << "\n입력 내용 확인\n";
    std::cout << "시료   " << sampleOpt->name << " (" << sampleOpt->id << ")\n";
    std::cout << "고객   " << customerName << "\n";
    std::cout << "수량   " << quantity << " ea\n\n";

    if (!ReadYesNo("[Y] 예약 접수   [N] 취소\n선택 > ")) {
        std::cout << "\n주문을 취소했습니다.\n";
        return;
    }

    const Model::Order order = orderService_.PlaceOrder(sampleId, customerName, quantity);

    std::cout << "\n예약 접수 완료.\n\n";
    std::cout << "주문번호  " << order.orderNo << "\n";
    std::cout << "현재 상태 " << Model::ToString(order.status) << "\n\n";
    std::cout << "※ 재고 확인은 [3] 승인 메뉴에서 직접 진행하세요.\n";
}

} // namespace Console
