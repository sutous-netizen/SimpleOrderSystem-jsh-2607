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

    // 고객명/시료명 컬럼은 한글이 섞이므로 std::setw(바이트 기준) 대신 PadDisplayWidth(화면 폭 기준)를 사용한다.
    constexpr int kCustomerColumnWidth = 16;
    constexpr int kSampleColumnWidth = 22;

    std::cout << "출고 가능 주문 (CONFIRMED)\n";
    std::cout << std::left
               << PadDisplayWidth("번호", 6)
               << PadDisplayWidth("주문번호", 20)
               << PadDisplayWidth("고객", kCustomerColumnWidth)
               << PadDisplayWidth("시료", kSampleColumnWidth)
               << "수량\n";

    for (size_t i = 0; i < releasable.size(); ++i) {
        const auto& order = releasable[i];
        std::cout << std::left
                   << std::setw(6) << ("[" + std::to_string(i + 1) + "]")
                   << std::setw(20) << order.orderNo
                   << PadDisplayWidth(order.customerName, kCustomerColumnWidth)
                   << PadDisplayWidth(SampleNameOf(store_, order.sampleId), kSampleColumnWidth)
                   << (std::to_string(order.quantity) + " ea") << "\n";
    }

    int64_t selected = 0;
    while (true) {
        const std::string text = ReadLine("\n출고할 번호 > ");
        if (TryParseInt64(text, selected) && selected >= 1 && static_cast<size_t>(selected) <= releasable.size()) {
            break;
        }
        if (IsInputExhausted()) {
            std::cout << "입력이 종료되었습니다. 처리를 취소합니다.\n";
            return;
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
