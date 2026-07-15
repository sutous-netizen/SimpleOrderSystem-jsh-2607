// Console::OrderMenuView / Console::SampleMenuView 통합(cin 스텁 + mock/fake) 테스트.
// "View + 실제 Monitor::OrderService + 가짜 저장소"로 최종 상태를 검증한다.
// docs/phase/phase5-console-hardening.md 기준.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Mocks/MockDataStore.h"
#include "../Console/OrderMenuView.h"
#include "../Console/SampleMenuView.h"
#include "../Monitor/OrderService.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

using ::testing::NiceMock;
using TestMocks::InMemoryDataStore;
using TestMocks::MockDataStore;

Model::Sample MakeSample(const std::string& id, const std::string& name, double avgMin, double yieldRate, int64_t stock) {
    Model::Sample s;
    s.id = id;
    s.name = name;
    s.avgProductionTimeMin = avgMin;
    s.yieldRate = yieldRate;
    s.stock = stock;
    return s;
}

// 표준 입력을 스텁하기 위한 공용 픽스처. mock/fake IDataStore 조합도 함께 준비한다.
class ConsoleViewTest : public ::testing::Test {
protected:
    NiceMock<MockDataStore> mock;
    InMemoryDataStore fake;

    void SetUp() override {
        TestMocks::DelegateToFake(mock, fake);
    }

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

// OrderMenuView: 존재하지 않는 시료 ID 를 입력하면 주문이 생성되지 않는다.
TEST_F(ConsoleViewTest, OrderMenuView_UnknownSampleId_DoesNotCreateOrder) {
    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480) });

    Monitor::OrderService orderService(mock);
    Console::OrderMenuView view(mock, orderService);

    SetInput("S-999\n"); // 등록되지 않은 시료 ID

    testing::internal::CaptureStdout();
    view.Run();
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(fake.LoadOrders().empty());
}

// OrderMenuView: 유효한 입력 후 [N](취소) 응답 시 주문이 생성되지 않는다.
TEST_F(ConsoleViewTest, OrderMenuView_ValidInputThenCancel_DoesNotCreateOrder) {
    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480) });

    Monitor::OrderService orderService(mock);
    Console::OrderMenuView view(mock, orderService);

    SetInput("S-001\nSK하이닉스\n150\nN\n");

    testing::internal::CaptureStdout();
    view.Run();
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(fake.LoadOrders().empty());
}

// OrderMenuView: 유효한 입력 후 [Y] 응답 시 RESERVED 상태의 주문이 실제로 저장된다.
TEST_F(ConsoleViewTest, OrderMenuView_ValidInputThenConfirm_CreatesReservedOrder) {
    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480) });

    Monitor::OrderService orderService(mock);
    Console::OrderMenuView view(mock, orderService);

    SetInput("S-001\nSK하이닉스\n150\nY\n");

    testing::internal::CaptureStdout();
    view.Run();
    testing::internal::GetCapturedStdout();

    const std::vector<Model::Order> orders = fake.LoadOrders();
    ASSERT_EQ(orders.size(), static_cast<size_t>(1));
    EXPECT_EQ(orders[0].sampleId, "S-001");
    EXPECT_EQ(orders[0].customerName, "SK하이닉스");
    EXPECT_EQ(orders[0].quantity, 150);
    EXPECT_TRUE(orders[0].status == Model::OrderStatus::RESERVED);
}

// SampleMenuView: 이미 존재하는 시료 ID 로 재등록을 시도하면 시료 목록이 변하지 않는다.
TEST_F(ConsoleViewTest, SampleMenuView_DuplicateId_DoesNotChangeSampleList) {
    fake.SaveSamples({ MakeSample("S-001", "실리콘 웨이퍼-8인치", 5.0, 1.0, 480) });

    Console::SampleMenuView view(mock);

    // [1] 시료 등록 -> 중복 ID 입력 -> 등록 실패 -> 메뉴로 복귀 -> [0] 종료
    SetInput("1\nS-001\n0\n");

    testing::internal::CaptureStdout();
    view.Run();
    testing::internal::GetCapturedStdout();

    const std::vector<Model::Sample> samples = fake.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(1));
    EXPECT_EQ(samples[0].id, "S-001");
}

// SampleMenuView: 잘못된 수율을 여러 번 입력한 뒤 유효한 값을 입력하면 재시도 루프를 거쳐 정상 등록된다.
TEST_F(ConsoleViewTest, SampleMenuView_InvalidYieldRetries_EventuallyRegistersSample) {
    Console::SampleMenuView view(mock);

    // [1] 등록 -> ID/이름/평균생산시간 정상 입력 -> 수율 "2"(범위초과) "0"(범위미달) "abc"(형식오류) 순으로 실패 후 "0.9" 로 성공 -> [0] 종료
    SetInput("1\nS-100\n테스트시료\n5\n2\n0\nabc\n0.9\n0\n");

    testing::internal::CaptureStdout();
    view.Run();
    testing::internal::GetCapturedStdout();

    const std::vector<Model::Sample> samples = fake.LoadSamples();
    ASSERT_EQ(samples.size(), static_cast<size_t>(1));
    EXPECT_EQ(samples[0].id, "S-100");
    EXPECT_EQ(samples[0].name, "테스트시료");
    EXPECT_DOUBLE_EQ(samples[0].avgProductionTimeMin, 5.0);
    EXPECT_DOUBLE_EQ(samples[0].yieldRate, 0.9);
}
