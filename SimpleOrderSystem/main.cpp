// 반도체 시료 생산주문관리 시스템 - 진입점
// director 가 각 에이전트(persistent/monitor/console/dummy) 산출물을 통합하는 지점.
//
// Debug 빌드(_DEBUG)에서는 실행 시작 시 Tests/ 하위에 등록된 모든 TC(TestFramework)를
// 자동으로 수행한 뒤 콘솔 애플리케이션을 이어서 실행한다.

#include "Persistence/JsonDataStore.h"
#include "Monitor/OrderService.h"
#include "Console/AppController.h"

#ifdef _DEBUG
#include "Tests/TestFramework.h"
#endif

#include <filesystem>
#include <iostream>

namespace {

// 실행 파일 위치와 무관하게 항상 프로젝트 data/ 폴더를 사용하도록 보장한다.
void EnsureDataDirectoryExists() {
    std::filesystem::create_directories("data");
}

} // namespace

int main() {
#ifdef _DEBUG
    const int failedTests = TestFramework::RunAllTests();
    if (failedTests > 0) {
        std::cerr << "\n[_Debug] 실패한 테스트가 " << failedTests << "건 있습니다. 계속 진행합니다.\n";
    }
    std::cout << std::endl;
#endif

    EnsureDataDirectoryExists();

    Persistence::JsonDataStore store("data/samples.json", "data/orders.json");
    Monitor::OrderService orderService(store);
    Console::AppController app(store, orderService);
    app.Run();

    return 0;
}
