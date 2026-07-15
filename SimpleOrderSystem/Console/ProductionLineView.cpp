#include "ProductionLineView.h"
#include "ConsoleUtil.h"

#include <iostream>
#include <iomanip>
#include <cmath>

namespace Console {

namespace {

// "YYYY-MM-DD HH:MM:SS" 형식의 문자열에서 "HH:MM" 부분만 추출한다.
// 형식이 예상과 다르면(길이 부족 등) 원본 문자열을 그대로 반환한다.
std::string ExtractHourMinute(const std::string& timestamp) {
    constexpr size_t kHourMinuteOffset = 11;
    constexpr size_t kHourMinuteLength = 5;
    if (timestamp.size() < kHourMinuteOffset + kHourMinuteLength) {
        return timestamp;
    }
    return timestamp.substr(kHourMinuteOffset, kHourMinuteLength);
}

} // namespace

ProductionLineView::ProductionLineView(Monitor::OrderService& orderService) : orderService_(orderService) {}

void ProductionLineView::Run() {
    std::cout << "\n[5] 생산라인 조회  FIFO 방식\n\n";

    const auto queue = orderService_.GetProductionQueue();
    if (queue.empty()) {
        std::cout << "현재 생산 대기 중인 주문이 없습니다.\n";
        return;
    }

    // 첫 번째 항목(FIFO 맨 앞)은 "현재 처리 중"으로 별도 강조 표시한다.
    const auto& current = queue.front();
    std::cout << "현재 처리 중\n";
    std::cout << "  주문번호  " << current.order.orderNo
               << "   시료  " << current.sample.name << "\n";
    std::cout << "  주문량    " << current.order.quantity << " ea"
               << "   부족  " << current.order.shortage << " ea"
               << "   실생산량  " << current.order.actualProductionQty << " ea"
               << "  (" << static_cast<int64_t>(current.order.totalProductionTimeMin) << " min)\n";
    std::cout << "  진행      " << static_cast<int>(std::lround(current.progressPercent)) << "%"
               << "   완료 예정  " << ExtractHourMinute(current.estimatedCompletionAt) << "\n\n";

    if (queue.size() > 1) {
        std::cout << "대기 중인 주문 (FIFO 순)\n";
    }

    // 시료명 컬럼은 한글이 섞이므로 std::setw(바이트 기준) 대신 PadDisplayWidth(화면 폭 기준)를 사용한다.
    constexpr int kSampleColumnWidth = 22;

    std::cout << std::left
               << std::setw(6) << "순서"
               << std::setw(12) << "주문번호"
               << PadDisplayWidth("시료", kSampleColumnWidth)
               << std::setw(10) << "주문량"
               << std::setw(10) << "부족분"
               << std::setw(10) << "실생산량"
               << std::setw(12) << "총생산시간"
               << std::setw(8) << "진행률"
               << "예상완료\n";

    for (size_t i = 0; i < queue.size(); ++i) {
        const auto& item = queue[i];
        std::cout << std::left
                   << std::setw(6) << (std::to_string(i + 1))
                   << std::setw(12) << item.order.orderNo
                   << PadDisplayWidth(item.sample.name, kSampleColumnWidth)
                   << std::setw(10) << (std::to_string(item.order.quantity) + " ea")
                   << std::setw(10) << (std::to_string(item.order.shortage) + " ea")
                   << std::setw(10) << (std::to_string(item.order.actualProductionQty) + " ea")
                   << std::setw(12) << (std::to_string(static_cast<int64_t>(item.order.totalProductionTimeMin)) + " min")
                   << std::setw(8) << (std::to_string(static_cast<int>(std::lround(item.progressPercent))) + "%")
                   << ExtractHourMinute(item.estimatedCompletionAt) << "\n";
    }

    std::cout << "\n* 부족분 = 주문량 - 재고, 실생산량 = ceil(부족분 / 수율)\n";
    std::cout << "* 선입선출(FIFO) 방식으로 처리됩니다.\n";
    std::cout << "* 진행률/완료예정은 단일 생산 라인 기준 누적 스케줄링으로 계산됩니다.\n";
}

} // namespace Console
