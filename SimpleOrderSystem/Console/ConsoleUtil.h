#pragma once
// Console 계층 공용 유틸리티 (문자열 트림, 숫자 파싱, 현재 시각 문자열 등).
// View/Controller 여러 파일에서 공유하는 소소한 헬퍼만 모아둔다.

#include <string>

namespace Console {

// 앞뒤 공백 제거
std::string Trim(const std::string& text);

// 현재 로컬 시각을 "YYYY-MM-DD HH:MM:SS" 형식 문자열로 반환.
std::string NowTimeString();

// 현재 로컬 시각을 "YYYYMMDD" 형식 문자열로 반환 (주문번호 채번용).
std::string NowDateCompact();

// 문자열을 int64_t 로 변환 시도. 실패하면 false 반환.
bool TryParseInt64(const std::string& text, int64_t& out);

// 문자열을 double 로 변환 시도. 실패하면 false 반환.
bool TryParseDouble(const std::string& text, double& out);

// 표준 입력에서 한 줄을 읽어 트림 후 반환.
std::string ReadLine(const std::string& prompt);

// [Y]/[N] 확인 입력을 받는다. Y 계열 입력이면 true.
bool ReadYesNo(const std::string& prompt);

} // namespace Console
