#include "ReleaseMenuView.h"
#include "ConsoleUtil.h"

#include <iostream>
#include <iomanip>

namespace Console {

ReleaseMenuView::ReleaseMenuView(Persistence::IDataStore& store, Monitor::OrderService& orderService)
    : store_(store), orderService_(orderService) {}

namespace {
std::string SampleNameOf(Persistence::IDataStore& store, const std::string& sampleId) {
    const auto sampleOpt = store.FindSampleById(sampleId);
    return sampleOpt.has_value() ? sampleOpt->name : sampleId;
}
} // namespace

void ReleaseMenuView::Run() {
    std::cout << "\n[6] 출고 처리\n\n";

    const std::vector<Model::Order> releasable = orderService_.GetReleasableOrders();
    if (releasable.empty()) {
        std::cout << "출고 가능한 주문(CONFIRMED)이 없습니다.\n";
        return;
    }

    std::cout << "출고 가능 주문 (CONFIRMED)\n";
    std::cout << std::left
               << std::setw(6) << "번호"
               << std::setw(12) << "주문번호"
               << std::setw(16) << "고객"
               << std::setw(22) << "시료"
               << "수량\n";

    for (size_t i = 0; i < releasable.size(); ++i) {
        const auto& order = releasable[i];
        std::cout << std::left
                   << std::setw(6) << ("[" + std::to_string(i + 1) + "]")
                   << std::setw(12) << order.orderNo
                   << std::setw(16) << order.customerName
                   << std::setw(22) << SampleNameOf(store_, order.sampleId)
                   << (std::to_string(order.quantity) + " ea") << "\n";
    }

    int64_t selected = 0;
    while (true) {
        const std::string text = ReadLine("\n출고할 번호 > ");
        if (TryParseInt64(text, selected) && selected >= 1 && static_cast<size_t>(selected) <= releasable.size()) {
            break;
        }
        std::cout << "1 ~ " << releasable.size() << " 범위의 번호를 입력하세요.\n";
    }

    const Model::Order& target = releasable[static_cast<size_t>(selected) - 1];
    const Model::Order released = orderService_.ReleaseOrder(target.orderNo);

    std::cout << "\n출고 처리 완료.\n";
    std::cout << "주문번호  " << released.orderNo << "\n";
    std::cout << "출고수량  " << released.quantity << " ea\n";
    std::cout << "처리일시  " << NowTimeString() << "\n";
    std::cout << "상태      CONFIRMED → " << Model::ToString(released.status) << "\n";
}

} // namespace Console
