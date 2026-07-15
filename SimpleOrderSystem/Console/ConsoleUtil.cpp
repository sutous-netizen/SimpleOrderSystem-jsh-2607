#include "ConsoleUtil.h"
#include "../Model/Types.h"

#include <chrono>
#include <ctime>
#include <cctype>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace Console {

std::string Trim(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

namespace {
std::tm LocalNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmValue{};
    localtime_s(&tmValue, &t);
    return tmValue;
}
} // namespace

std::string NowTimeString() {
    // "YYYY-MM-DD HH:MM:SS" 포맷은 Monitor 계층(OrderService)과 공유하므로
    // Model::FormatLocalTimestamp에 위임한다(계층 간 중복 구현 방지).
    return Model::FormatLocalTimestamp(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::string NowDateCompact() {
    const std::tm tmValue = LocalNow();
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d",
        tmValue.tm_year + 1900, tmValue.tm_mon + 1, tmValue.tm_mday);
    return buffer;
}

bool TryParseInt64(const std::string& text, int64_t& out) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        long long value = std::stoll(trimmed, &idx);
        if (idx != trimmed.size()) {
            return false;
        }
        out = static_cast<int64_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool TryParseDouble(const std::string& text, double& out) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        double value = std::stod(trimmed, &idx);
        if (idx != trimmed.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

namespace {
// ReadLine 이 표준 입력 고갈(EOF)로 실패했는지 추적하는 상태.
// 콘솔 앱은 단일 입력 스레드로 동작하므로 전역 플래그로 충분하다.
bool g_inputExhausted = false;
} // namespace

std::string ReadLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cin.clear();
        g_inputExhausted = true;
        return "";
    }
    g_inputExhausted = false;
    return Trim(line);
}

bool IsInputExhausted() {
    return g_inputExhausted;
}

bool ReadYesNo(const std::string& prompt) {
    while (true) {
        const std::string input = ReadLine(prompt);
        if (IsInputExhausted()) {
            // 입력 스트림이 고갈된 상태에서 무한 재시도하지 않도록 취소로 간주한다.
            return false;
        }
        if (input.empty()) {
            continue;
        }
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(input[0])));
        if (c == 'Y') {
            return true;
        }
        if (c == 'N') {
            return false;
        }
        std::cout << "[Y] 또는 [N] 을 입력하세요.\n";
    }
}

namespace {
// UTF-8 바이트 하나의 화면 표시 폭 기여도를 반환한다.
// continuation byte(0x80~0xBF)는 0(선행 바이트에서 이미 폭을 계산했으므로 무시),
// ASCII(0x00~0x7F)는 1, 그 외 멀티바이트 시퀀스의 시작 바이트는 2로 근사한다.
int DisplayWidthOf(unsigned char byte) {
    if (byte >= 0x80 && byte <= 0xBF) {
        return 0;
    }
    if (byte <= 0x7F) {
        return 1;
    }
    return 2;
}
} // namespace

std::string PadDisplayWidth(const std::string& text, int targetWidth) {
    int width = 0;
    for (const unsigned char byte : text) {
        width += DisplayWidthOf(byte);
    }
    if (width >= targetWidth) {
        return text;
    }
    return text + std::string(static_cast<size_t>(targetWidth - width), ' ');
}

std::string SampleNameOf(Persistence::IDataStore& store, const std::string& sampleId) {
    const auto sampleOpt = store.FindSampleById(sampleId);
    return sampleOpt.has_value() ? sampleOpt->name : sampleId;
}

} // namespace Console
