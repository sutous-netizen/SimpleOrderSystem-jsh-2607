#include "ApprovalMenuView.h"
#include "ConsoleUtil.h"

#include <iostream>
#include <iomanip>

namespace Console {

ApprovalMenuView::ApprovalMenuView(Persistence::IDataStore& store, Monitor::OrderService& orderService)
    : store_(store), orderService_(orderService) {}

namespace {
std::string SampleNameOf(Persistence::IDataStore& store, const std::string& sampleId) {
    const auto sampleOpt = store.FindSampleById(sampleId);
    return sampleOpt.has_value() ? sampleOpt->name : sampleId;
}
} // namespace

void ApprovalMenuView::Run() {
    std::cout << "\n[3] 주문 승인/거절\n\n";

    const std::vector<Model::Order> pending = orderService_.GetPendingOrders();
    if (pending.empty()) {
        std::cout << "승인 대기 중인 예약이 없습니다.\n";
        return;
    }

    // 고객명/시료명 컬럼은 한글이 섞이므로 std::setw(바이트 기준) 대신 PadDisplayWidth(화면 폭 기준)를 사용한다.
    constexpr int kCustomerColumnWidth = 16;
    constexpr int kSampleColumnWidth = 22;

    std::cout << "승인 대기 중인 예약 목록 (RESERVED)\n";
    std::cout << std::left
               << std::setw(6) << "번호"
               << std::setw(12) << "주문번호"
               << PadDisplayWidth("고객", kCustomerColumnWidth)
               << PadDisplayWidth("시료", kSampleColumnWidth)
               << std::setw(10) << "수량"
               << "상태" << "\n";

    for (size_t i = 0; i < pending.size(); ++i) {
        const auto& order = pending[i];
        std::cout << std::left
                   << std::setw(6) << ("[" + std::to_string(i + 1) + "]")
                   << std::setw(12) << order.orderNo
                   << PadDisplayWidth(order.customerName, kCustomerColumnWidth)
                   << PadDisplayWidth(SampleNameOf(store_, order.sampleId), kSampleColumnWidth)
                   << std::setw(10) << (std::to_string(order.quantity) + " ea")
                   << Model::ToString(order.status) << "\n";
    }

    int64_t selected = 0;
    while (true) {
        const std::string text = ReadLine("\n승인/거절할 번호 > ");
        if (TryParseInt64(text, selected) && selected >= 1 && static_cast<size_t>(selected) <= pending.size()) {
            break;
        }
        if (IsInputExhausted()) {
            std::cout << "입력이 종료되었습니다. 처리를 취소합니다.\n";
            return;
        }
        std::cout << "1 ~ " << pending.size() << " 범위의 번호를 입력하세요.\n";
    }

    const Model::Order& target = pending[static_cast<size_t>(selected) - 1];
    const auto sampleOpt = store_.FindSampleById(target.sampleId);

    std::cout << "\n재고 확인 중...\n";
    if (sampleOpt.has_value()) {
        std::cout << "시료      " << sampleOpt->name << "   현재 재고  " << sampleOpt->stock << " ea\n";
    }
    std::cout << "주문 수량 " << target.quantity << " ea\n";

    if (!ReadYesNo("\n[Y] 승인   [N] 거절\n선택 > ")) {
        const Model::Order rejected = orderService_.RejectOrder(target.orderNo);
        std::cout << "\n거절 완료.\n";
        std::cout << "상태 변경  RESERVED → " << Model::ToString(rejected.status) << "\n";
        std::cout << "주문번호   " << rejected.orderNo << "\n";
        return;
    }

    const Model::Order approved = orderService_.ApproveOrder(target.orderNo);

    std::cout << "\n승인 완료.\n";
    std::cout << "상태 변경  RESERVED → " << Model::ToString(approved.status) << "\n";
    std::cout << "주문번호   " << approved.orderNo << "\n";

    if (approved.status == Model::OrderStatus::PRODUCING) {
        std::cout << "부족분       " << approved.shortage << " ea\n";
        std::cout << "실 생산량    " << approved.actualProductionQty << " ea\n";
        std::cout << "총 생산시간  " << approved.totalProductionTimeMin << " min\n";
    }
}

} // namespace Console
