#include "SampleMenuView.h"
#include "ConsoleUtil.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace Console {

namespace {
std::string FormatNumber(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}
} // namespace

SampleMenuView::SampleMenuView(Persistence::IDataStore& store) : store_(store) {}

void SampleMenuView::Run() {
    while (true) {
        std::cout << "\n[1] 시료 관리\n";
        std::cout << "[1] 시료 등록   [2] 시료 목록   [3] 시료 검색   [0] 뒤로\n";
        const std::string choice = ReadLine("선택 > ");

        if (choice == "0") {
            return;
        } else if (IsInputExhausted()) {
            // 표준 입력이 고갈된 상태에서 무한 재시도하지 않도록 종료한다.
            std::cout << "입력이 종료되었습니다. 뒤로 갑니다.\n";
            return;
        } else if (choice == "1") {
            ShowRegister();
        } else if (choice == "2") {
            ShowList();
        } else if (choice == "3") {
            ShowSearch();
        } else {
            std::cout << "잘못된 선택입니다.\n";
        }
    }
}

void SampleMenuView::ShowRegister() {
    std::cout << "\n[1] 시료 등록\n";

    Model::Sample sample;
    sample.id = ReadLine("시료 ID     > ");
    if (sample.id.empty()) {
        std::cout << "시료 ID 는 비워둘 수 없습니다. 등록을 취소합니다.\n";
        return;
    }

    if (store_.FindSampleById(sample.id).has_value()) {
        std::cout << "이미 등록된 시료 ID 입니다: " << sample.id << "\n";
        return;
    }

    sample.name = ReadLine("이름        > ");
    if (sample.name.empty()) {
        std::cout << "이름은 비워둘 수 없습니다. 등록을 취소합니다.\n";
        return;
    }

    double avgTime = 0.0;
    while (true) {
        const std::string text = ReadLine("평균 생산시간(min/ea) > ");
        if (TryParseDouble(text, avgTime) && avgTime > 0.0) {
            break;
        }
        if (IsInputExhausted()) {
            std::cout << "입력이 종료되었습니다. 등록을 취소합니다.\n";
            return;
        }
        std::cout << "0보다 큰 숫자를 입력하세요.\n";
    }
    sample.avgProductionTimeMin = avgTime;

    double yield = 0.0;
    while (true) {
        const std::string text = ReadLine("수율(0~1)   > ");
        if (TryParseDouble(text, yield) && yield > 0.0 && yield <= 1.0) {
            break;
        }
        if (IsInputExhausted()) {
            std::cout << "입력이 종료되었습니다. 등록을 취소합니다.\n";
            return;
        }
        std::cout << "0 초과 1 이하의 숫자를 입력하세요.\n";
    }
    sample.yieldRate = yield;
    sample.stock = 0;

    std::vector<Model::Sample> samples = store_.LoadSamples();
    samples.push_back(sample);
    store_.SaveSamples(samples);

    std::cout << "\n시료 등록 완료.\n";
    std::cout << "ID   " << sample.id << "\n";
    std::cout << "이름 " << sample.name << "\n";
}

void SampleMenuView::ShowList() {
    std::cout << "\n[2] 시료 목록\n";
    const std::vector<Model::Sample> samples = store_.LoadSamples();
    std::cout << "등록 시료 목록 (총 " << samples.size() << "종)\n";
    PrintSampleTable(samples);
}

void SampleMenuView::ShowSearch() {
    std::cout << "\n[3] 시료 검색\n";
    const std::string keyword = ReadLine("검색어(이름 일부) > ");

    const std::vector<Model::Sample> samples = store_.LoadSamples();
    std::vector<Model::Sample> matched;
    std::string lowerKeyword = keyword;
    std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& sample : samples) {
        std::string lowerName = sample.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lowerName.find(lowerKeyword) != std::string::npos) {
            matched.push_back(sample);
        }
    }

    std::cout << "검색 결과 (" << matched.size() << "건)\n";
    PrintSampleTable(matched);
}

void SampleMenuView::PrintSampleTable(const std::vector<Model::Sample>& samples) const {
    // 시료명 컬럼은 한글이 섞이므로 std::setw(바이트 기준) 대신 PadDisplayWidth(화면 폭 기준)를 사용한다.
    constexpr int kNameColumnWidth = 22;
    std::cout << std::left
               << std::setw(8) << "ID"
               << PadDisplayWidth("시료명", kNameColumnWidth)
               << PadDisplayWidth("평균 생산시간", 14)
               << PadDisplayWidth("수율", 8)
               << PadDisplayWidth("현재 재고", 10) << "\n";

    for (const auto& sample : samples) {
        std::cout << std::left
                   << std::setw(8) << sample.id
                   << PadDisplayWidth(sample.name, kNameColumnWidth)
                   << std::setw(14) << (FormatNumber(sample.avgProductionTimeMin) + " min/ea")
                   << std::setw(8) << FormatNumber(sample.yieldRate)
                   << std::setw(10) << (std::to_string(sample.stock) + " ea") << "\n";
    }
}

} // namespace Console
