#pragma once
// 공유 도메인 모델 정의 (Model 계층 공통 계약)
// 모든 에이전트(console/monitor/persistent/dummy)는 이 헤더의 타입을 그대로 사용한다.
// docs/01-개요.md ~ docs/08-출고처리.md 기준.

#include <string>
#include <cstdint>
#include <ctime>

namespace Model {

// "YYYY-MM-DD HH:MM:SS" 형식의 로컬 타임스탬프 문자열로 변환한다.
// Order.createdAt/updatedAt 등 프로젝트 전역에서 공유하는 정규 시각 포맷이며,
// Monitor(도메인 스케줄링)와 Console(화면 표시) 양쪽에서 공용으로 사용한다
// (동일한 포맷 로직이 계층별로 중복 구현되지 않도록 Model에 둔다).
std::string FormatLocalTimestamp(std::time_t t);

// 주문 상태 흐름: RESERVED -> (REJECTED | CONFIRMED | PRODUCING) -> CONFIRMED -> RELEASE
// REJECTED 는 정상 흐름 밖의 상태이며 모니터링에서 제외한다.
enum class OrderStatus {
    RESERVED,
    REJECTED,
    PRODUCING,
    CONFIRMED,
    RELEASE
};

// 문자열 <-> OrderStatus 변환 (JSON 저장/표시에 공용으로 사용)
const std::string& ToString(OrderStatus status);
OrderStatus OrderStatusFromString(const std::string& text);

// 재고 상태 (docs/06-모니터링.md)
enum class StockState {
    ABUNDANT,   // 여유
    SHORTAGE,   // 부족
    DEPLETED    // 고갈
};

const std::string& ToString(StockState state);

// 시료 (docs/03-시료관리.md)
struct Sample {
    std::string id;                 // 시료 ID, 예: "S-001"
    std::string name;               // 시료명
    double avgProductionTimeMin = 0.0; // 평균 생산시간 (min/ea)
    double yieldRate = 1.0;         // 수율 (0 초과 1 이하)
    int64_t stock = 0;              // 현재 재고 수량
};

// 주문 (docs/04-시료주문.md, docs/05-주문승인거절.md)
struct Order {
    std::string orderNo;            // 주문번호, 예: "ORD-20260416-0043"
    std::string sampleId;           // 대상 시료 ID
    std::string customerName;       // 고객명
    int64_t quantity = 0;           // 주문 수량
    OrderStatus status = OrderStatus::RESERVED;
    std::string createdAt;          // 생성 시각 (ISO 8601: "YYYY-MM-DD HH:MM:SS")
    std::string updatedAt;          // 최근 상태 변경 시각

    // 승인 시 재고 부족으로 생산 큐에 등록된 경우에만 채워짐 (docs/07-생산라인.md)
    int64_t shortage = 0;              // 부족분 = 주문 수량 - 재고
    int64_t actualProductionQty = 0;   // 실 생산량 = ceil(부족분 / 수율)
    double totalProductionTimeMin = 0.0; // 총 생산 시간 = 평균 생산시간 * 실 생산량
};

} // namespace Model
