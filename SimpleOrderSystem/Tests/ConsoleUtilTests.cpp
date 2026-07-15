// Console::ConsoleUtil 순수 함수/표준 입력 헬퍼 gtest 테스트.
// docs/phase/phase5-console-hardening.md 기준.

#include <gtest/gtest.h>

#include "../Console/ConsoleUtil.h"

#include <cctype>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

// 표준 입력을 스텁하기 위한 공용 픽스처.
// SetUp 에서는 아무 것도 하지 않고, SetInput 호출 시점에 cin.rdbuf 를 바꿔치기하며,
// TearDown 에서 반드시 원래 버퍼로 복원해 다른 테스트에 영향을 주지 않는다.
class ConsoleUtilTest : public ::testing::Test {
protected:
    void SetInput(const std::string& text) {
        input_ = std::make_unique<std::istringstream>(text);
        originalCinBuf_ = std::cin.rdbuf(input_->rdbuf());
    }

    void TearDown() override {
        if (originalCinBuf_ != nullptr) {
            std::cin.rdbuf(originalCinBuf_);
            originalCinBuf_ = nullptr;
        }
    }

private:
    std::unique_ptr<std::istringstream> input_;
    std::streambuf* originalCinBuf_ = nullptr;
};

} // namespace

// Trim: 앞뒤 공백/탭을 제거한다.
TEST_F(ConsoleUtilTest, Trim_RemovesLeadingAndTrailingWhitespace) {
    EXPECT_EQ(Console::Trim("  hello  "), "hello");
    EXPECT_EQ(Console::Trim("\thello\t"), "hello");
    EXPECT_EQ(Console::Trim("  hello world  "), "hello world");
}

// Trim: 빈 문자열은 그대로 빈 문자열을 반환한다.
TEST_F(ConsoleUtilTest, Trim_EmptyString_ReturnsEmpty) {
    EXPECT_EQ(Console::Trim(""), "");
}

// Trim: 공백만 있는 문자열은 빈 문자열이 된다.
TEST_F(ConsoleUtilTest, Trim_WhitespaceOnly_ReturnsEmpty) {
    EXPECT_EQ(Console::Trim("    "), "");
    EXPECT_EQ(Console::Trim("\t\t"), "");
}

// TryParseInt64: 정상적인 정수 문자열을 파싱한다.
TEST_F(ConsoleUtilTest, TryParseInt64_ValidPositiveInteger_Succeeds) {
    int64_t value = 0;
    EXPECT_TRUE(Console::TryParseInt64("150", value));
    EXPECT_EQ(value, 150);
}

// TryParseInt64: 음수도 정상적으로 파싱한다.
TEST_F(ConsoleUtilTest, TryParseInt64_NegativeInteger_Succeeds) {
    int64_t value = 0;
    EXPECT_TRUE(Console::TryParseInt64("-42", value));
    EXPECT_EQ(value, -42);
}

// TryParseInt64: 앞뒤 공백이 포함되어도 트림 후 파싱에 성공한다.
TEST_F(ConsoleUtilTest, TryParseInt64_SurroundedByWhitespace_Succeeds) {
    int64_t value = 0;
    EXPECT_TRUE(Console::TryParseInt64("  200  ", value));
    EXPECT_EQ(value, 200);
}

// TryParseInt64: 숫자가 아닌 문자열은 실패한다.
TEST_F(ConsoleUtilTest, TryParseInt64_NonNumeric_Fails) {
    int64_t value = 0;
    EXPECT_FALSE(Console::TryParseInt64("abc", value));
}

// TryParseInt64: 숫자 뒤에 문자가 붙어있으면 실패한다.
TEST_F(ConsoleUtilTest, TryParseInt64_TrailingGarbage_Fails) {
    int64_t value = 0;
    EXPECT_FALSE(Console::TryParseInt64("12ab", value));
}

// TryParseInt64: 빈 문자열은 실패한다.
TEST_F(ConsoleUtilTest, TryParseInt64_EmptyString_Fails) {
    int64_t value = 0;
    EXPECT_FALSE(Console::TryParseInt64("", value));
}

// TryParseDouble: 정상적인 소수 문자열을 파싱한다.
TEST_F(ConsoleUtilTest, TryParseDouble_ValidDecimal_Succeeds) {
    double value = 0.0;
    EXPECT_TRUE(Console::TryParseDouble("0.92", value));
    EXPECT_DOUBLE_EQ(value, 0.92);
}

// TryParseDouble: 정수 형태 문자열도 정상 파싱한다.
TEST_F(ConsoleUtilTest, TryParseDouble_IntegerLikeText_Succeeds) {
    double value = 0.0;
    EXPECT_TRUE(Console::TryParseDouble("5", value));
    EXPECT_DOUBLE_EQ(value, 5.0);
}

// TryParseDouble: 잘못된 형식은 실패한다.
TEST_F(ConsoleUtilTest, TryParseDouble_InvalidFormat_Fails) {
    double value = 0.0;
    EXPECT_FALSE(Console::TryParseDouble("abc", value));
    EXPECT_FALSE(Console::TryParseDouble("", value));
    EXPECT_FALSE(Console::TryParseDouble("0.9x", value));
}

// NowDateCompact: 정확히 8자리 숫자로만 구성된 문자열을 반환한다.
TEST_F(ConsoleUtilTest, NowDateCompact_ReturnsEightDigitString) {
    const std::string date = Console::NowDateCompact();
    ASSERT_EQ(date.size(), static_cast<size_t>(8));
    for (const char c : date) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
    }
}

// ReadLine: 표준 입력을 스텁해서 앞뒤 공백이 트림된 결과를 반환하는지 확인한다.
TEST_F(ConsoleUtilTest, ReadLine_TrimsInjectedInput) {
    SetInput("  hello  \n");
    const std::string result = Console::ReadLine("prompt > ");
    EXPECT_EQ(result, "hello");
}

// ReadLine: 입력 스트림이 고갈되면 빈 문자열을 반환하고 IsInputExhausted 가 true 가 된다.
TEST_F(ConsoleUtilTest, ReadLine_ExhaustedInput_ReturnsEmptyAndMarksExhausted) {
    SetInput(""); // 즉시 EOF
    const std::string result = Console::ReadLine("prompt > ");
    EXPECT_EQ(result, "");
    EXPECT_TRUE(Console::IsInputExhausted());
}

// ReadYesNo: "y" 입력은 true 를 반환한다.
TEST_F(ConsoleUtilTest, ReadYesNo_YInput_ReturnsTrue) {
    SetInput("y\n");
    EXPECT_TRUE(Console::ReadYesNo("prompt > "));
}

// ReadYesNo: "N" 입력(대문자)은 false 를 반환한다.
TEST_F(ConsoleUtilTest, ReadYesNo_UppercaseNInput_ReturnsFalse) {
    SetInput("N\n");
    EXPECT_FALSE(Console::ReadYesNo("prompt > "));
}

// ReadYesNo: 잘못된 입력 후 재시도하여 유효한 값을 받으면 정상적으로 반환한다(무한루프 없음).
TEST_F(ConsoleUtilTest, ReadYesNo_InvalidThenValid_RetriesAndReturns) {
    SetInput("x\ny\n");
    EXPECT_TRUE(Console::ReadYesNo("prompt > "));
}

// ReadYesNo: 입력 스트림이 고갈되면 무한루프에 빠지지 않고 false(취소)를 반환한다.
TEST_F(ConsoleUtilTest, ReadYesNo_ExhaustedInput_ReturnsFalseWithoutInfiniteLoop) {
    SetInput(""); // 즉시 EOF
    EXPECT_FALSE(Console::ReadYesNo("prompt > "));
}
