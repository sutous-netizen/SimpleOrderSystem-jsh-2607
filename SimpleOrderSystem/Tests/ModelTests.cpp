// Model 계층(OrderStatus/StockState 문자열 변환) 단위 테스트.
// docs/01-개요.md 의 주문 상태 흐름(RESERVED/REJECTED/PRODUCING/CONFIRMED/RELEASE)과
// docs/06-모니터링.md 의 재고 상태(여유/부족/고갈) 표기를 검증한다.
// Model 은 Persistence::IDataStore 와 무관하므로 mock 없이 순수 단위 테스트로 작성한다.

#include <gtest/gtest.h>
#include "../Model/Types.h"

#include <stdexcept>

// OrderStatus 각 값이 규격대로 문자열로 변환되는지 확인한다.
TEST(ModelToStringTest, OrderStatus_ReturnsExpectedStringForEachStatus) {
    EXPECT_EQ(Model::ToString(Model::OrderStatus::RESERVED), std::string("RESERVED"));
    EXPECT_EQ(Model::ToString(Model::OrderStatus::REJECTED), std::string("REJECTED"));
    EXPECT_EQ(Model::ToString(Model::OrderStatus::PRODUCING), std::string("PRODUCING"));
    EXPECT_EQ(Model::ToString(Model::OrderStatus::CONFIRMED), std::string("CONFIRMED"));
    EXPECT_EQ(Model::ToString(Model::OrderStatus::RELEASE), std::string("RELEASE"));
}

// 문자열을 OrderStatus 로 역변환한다.
TEST(ModelOrderStatusFromStringTest, ParsesEachKnownStatusString) {
    EXPECT_TRUE(Model::OrderStatusFromString("RESERVED") == Model::OrderStatus::RESERVED);
    EXPECT_TRUE(Model::OrderStatusFromString("REJECTED") == Model::OrderStatus::REJECTED);
    EXPECT_TRUE(Model::OrderStatusFromString("PRODUCING") == Model::OrderStatus::PRODUCING);
    EXPECT_TRUE(Model::OrderStatusFromString("CONFIRMED") == Model::OrderStatus::CONFIRMED);
    EXPECT_TRUE(Model::OrderStatusFromString("RELEASE") == Model::OrderStatus::RELEASE);
}

// ToString -> FromString 왕복 변환이 원래 값과 일치하는지 확인한다.
TEST(ModelOrderStatusFromStringTest, RoundTripWithToStringYieldsOriginalStatus) {
    const Model::OrderStatus statuses[] = {
        Model::OrderStatus::RESERVED,
        Model::OrderStatus::REJECTED,
        Model::OrderStatus::PRODUCING,
        Model::OrderStatus::CONFIRMED,
        Model::OrderStatus::RELEASE,
    };
    for (const auto& status : statuses) {
        Model::OrderStatus roundTripped = Model::OrderStatusFromString(Model::ToString(status));
        EXPECT_TRUE(roundTripped == status);
    }
}

// 알 수 없는 문자열이면 invalid_argument 예외가 발생해야 한다.
TEST(ModelOrderStatusFromStringTest, ThrowsInvalidArgumentForUnknownString) {
    EXPECT_THROW(Model::OrderStatusFromString("UNKNOWN"), std::invalid_argument);
}

// 빈 문자열이면 invalid_argument 예외가 발생해야 한다.
TEST(ModelOrderStatusFromStringTest, ThrowsInvalidArgumentForEmptyString) {
    EXPECT_THROW(Model::OrderStatusFromString(""), std::invalid_argument);
}

// StockState 각 값이 한글 표기 문자열로 변환되는지 확인한다.
TEST(ModelToStringTest, StockState_ReturnsExpectedKoreanStringForEachState) {
    EXPECT_EQ(Model::ToString(Model::StockState::ABUNDANT), std::string("여유"));
    EXPECT_EQ(Model::ToString(Model::StockState::SHORTAGE), std::string("부족"));
    EXPECT_EQ(Model::ToString(Model::StockState::DEPLETED), std::string("고갈"));
}
