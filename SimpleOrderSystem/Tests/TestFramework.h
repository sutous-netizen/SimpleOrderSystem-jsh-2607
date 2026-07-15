#pragma once
// 초경량 자가등록 테스트 프레임워크.
// 외부 의존성 없이 TDD 테스트를 작성하기 위한 용도이며,
// Debug 빌드(_DEBUG)에서 main() 시작 시 RunAllTests() 로 전체 실행된다.
//
// 사용 예:
//   TEST(Sample_Yield_계산) {
//       ASSERT_EQ(1 + 1, 2);
//   }

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace TestFramework {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        Registry().push_back(TestCase{ name, std::move(fn) });
    }
};

// 실행: 등록된 모든 테스트를 실행하고 실패 개수를 반환한다.
inline int RunAllTests() {
    int failed = 0;
    int total = static_cast<int>(Registry().size());
    std::cout << "==== TC 자동 수행 (_Debug) : 총 " << total << "건 ====" << std::endl;
    for (auto& t : Registry()) {
        try {
            t.fn();
            std::cout << "[PASS] " << t.name << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[FAIL] " << t.name << " - " << e.what() << std::endl;
            ++failed;
        } catch (...) {
            std::cout << "[FAIL] " << t.name << " - unknown exception" << std::endl;
            ++failed;
        }
    }
    std::cout << "==== 결과: " << (total - failed) << "/" << total << " 통과 ====" << std::endl;
    return failed;
}

} // namespace TestFramework

#define TF_CONCAT_INNER(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT_INNER(a, b)

#define TEST(name) \
    static void TF_CONCAT(TestFn_, name)(); \
    static ::TestFramework::Registrar TF_CONCAT(TestRegistrar_, name)(#name, TF_CONCAT(TestFn_, name)); \
    static void TF_CONCAT(TestFn_, name)()

#define ASSERT_TRUE(cond) \
    do { if (!(cond)) { \
        std::ostringstream _oss; \
        _oss << "ASSERT_TRUE failed: " #cond << " (" << __FILE__ << ":" << __LINE__ << ")"; \
        throw std::runtime_error(_oss.str()); \
    } } while (0)

#define ASSERT_EQ(actual, expected) \
    do { if (!((actual) == (expected))) { \
        std::ostringstream _oss; \
        _oss << "ASSERT_EQ failed: " #actual " != " #expected \
             << " (actual=" << (actual) << ", expected=" << (expected) << ")" \
             << " (" << __FILE__ << ":" << __LINE__ << ")"; \
        throw std::runtime_error(_oss.str()); \
    } } while (0)
