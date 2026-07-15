#pragma once
// Console 계층 공용 유틸리티 (문자열 트림, 숫자 파싱, 현재 시각 문자열 등).
// View/Controller 여러 파일에서 공유하는 소소한 헬퍼만 모아둔다.

#include <string>

#include "../Persistence/IDataStore.h"

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
// 입력 스트림이 고갈(EOF 등)된 경우에도 빈 문자열을 반환하며, 이 경우 IsInputExhausted() 가 true 를 반환한다.
std::string ReadLine(const std::string& prompt);

// 가장 최근 ReadLine 호출이 표준 입력 고갈(EOF)로 인해 실패했는지 여부.
// 재시도 루프(while(true) 형태의 입력 검증)에서 무한루프를 방지하기 위해 반드시 확인해야 한다.
bool IsInputExhausted();

// [Y]/[N] 확인 입력을 받는다. Y 계열 입력이면 true.
// 입력 스트림이 고갈된 경우 무한루프를 피하기 위해 false(취소)를 반환한다.
bool ReadYesNo(const std::string& prompt);

// UTF-8 문자열의 "화면 표시 너비"를 근사 계산해 targetWidth 만큼 오른쪽에 공백을 채운다(왼쪽 정렬).
// std::setw 는 문자열의 바이트 수를 기준으로 패딩하기 때문에, 한글처럼 1글자가 여러 바이트인
// 멀티바이트 UTF-8 문자열이 섞이면 표 컬럼이 어긋난다. 이 함수는 다음 규칙으로 화면 폭을 근사한다.
//   - ASCII(0x00~0x7F) 1바이트 = 화면 폭 1
//   - UTF-8 continuation byte(0x80~0xBF) = 폭 계산에서 제외(선행 바이트에서 이미 폭을 반영)
//   - 그 외(멀티바이트 시퀀스의 시작 바이트, 한글 등) = 화면 폭 2로 근사
// 문자열의 화면 폭이 이미 targetWidth 이상이면 자르지 않고 그대로 반환한다.
std::string PadDisplayWidth(const std::string& text, int targetWidth);

// 시료 ID로 저장소에서 시료명을 조회한다. 시료가 존재하지 않으면 sampleId 자체를 반환한다
// (승인/거절, 출고 처리 화면 등 여러 View가 공유하는 조회 헬퍼).
std::string SampleNameOf(Persistence::IDataStore& store, const std::string& sampleId);

} // namespace Console
