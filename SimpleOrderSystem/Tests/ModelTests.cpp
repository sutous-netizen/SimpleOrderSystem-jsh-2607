// Model 계층(OrderStatus/StockState 문자열 변환) 단위 테스트.
// docs/01-개요.md 의 주문 상태 흐름(RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE)과
// docs/06-모니터링.md 의 재고 상태(여유/부족/고갈) 표기를 검증한다.

#include "TestFramework.h"
#include "../Model/Types.h"

#include <stdexcept>

TEST(Model_ToString_OrderStatus_각_상태별_문자열_반환) {
    ASSERT_EQ(Model::ToString(Model::OrderStatus::RESERVED), std::string("RESERVED"));
    ASSERT_EQ(Model::ToString(Model::OrderStatus::REJECTED), std::string("REJECTED"));
    ASSERT_EQ(Model::ToString(Model::OrderStatus::PRODUCING), std::string("PRODUCING"));
    ASSERT_EQ(Model::ToString(Model::OrderStatus::CONFIRMED), std::string("CONFIRMED"));
    ASSERT_EQ(Model::ToString(Model::OrderStatus::RELEASE), std::string("RELEASE"));
}

TEST(Model_OrderStatusFromString_문자열을_상태로_역변환) {
    ASSERT_TRUE(Model::OrderStatusFromString("RESERVED") == Model::OrderStatus::RESERVED);
    ASSERT_TRUE(Model::OrderStatusFromString("REJECTED") == Model::OrderStatus::REJECTED);
    ASSERT_TRUE(Model::OrderStatusFromString("PRODUCING") == Model::OrderStatus::PRODUCING);
    ASSERT_TRUE(Model::OrderStatusFromString("CONFIRMED") == Model::OrderStatus::CONFIRMED);
    ASSERT_TRUE(Model::OrderStatusFromString("RELEASE") == Model::OrderStatus::RELEASE);
}

TEST(Model_OrderStatus_왕복변환_ToString_FromString_일치) {
    const Model::OrderStatus statuses[] = {
        Model::OrderStatus::RESERVED,
        Model::OrderStatus::REJECTED,
        Model::OrderStatus::PRODUCING,
        Model::OrderStatus::CONFIRMED,
        Model::OrderStatus::RELEASE,
    };
    for (const auto& status : statuses) {
        Model::OrderStatus roundTripped = Model::OrderStatusFromString(Model::ToString(status));
        ASSERT_TRUE(roundTripped == status);
    }
}

TEST(Model_OrderStatusFromString_알수없는_문자열이면_예외발생) {
    bool threw = false;
    try {
        Model::OrderStatusFromString("UNKNOWN");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Model_OrderStatusFromString_빈문자열이면_예외발생) {
    bool threw = false;
    try {
        Model::OrderStatusFromString("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Model_ToString_StockState_각_상태별_한글_문자열_반환) {
    ASSERT_EQ(Model::ToString(Model::StockState::ABUNDANT), std::string("여유"));
    ASSERT_EQ(Model::ToString(Model::StockState::SHORTAGE), std::string("부족"));
    ASSERT_EQ(Model::ToString(Model::StockState::DEPLETED), std::string("고갈"));
}
