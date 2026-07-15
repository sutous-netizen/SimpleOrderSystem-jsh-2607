// 반도체 시료 생산주문관리 시스템 - 진입점
// director 가 각 에이전트(persistent/monitor/console/dummy) 산출물을 통합하는 지점.
//
// Debug 빌드(_DEBUG)에서는 실행 시작 시 Tests/ 하위에 등록된 모든 TC(TestFramework)를
// 자동으로 수행한 뒤 콘솔 애플리케이션을 이어서 실행한다.

#include "Persistence/JsonDataStore.h"
#include "Monitor/OrderService.h"
#include "Console/AppController.h"
#include "Dummy/DummyDataGenerator.h"

#ifdef _DEBUG
#include "Tests/TestFramework.h"
#endif

#include <filesystem>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

// 소스는 서명 있는 UTF-8(BOM)로 저장되어 있으나, Windows 콘솔의 기본 코드페이지는
// 로캘에 따라 CP949/CP437 등으로 설정되어 있어 UTF-8 출력이 그대로 깨져 보인다.
// 콘솔 입출력 코드페이지를 UTF-8(65001)로 맞춰 한글이 항상 정상적으로 표시되게 한다.
void ConfigureConsoleForUtf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

// 실행 파일 위치와 무관하게 항상 프로젝트 data/ 폴더를 사용하도록 보장한다.
void EnsureDataDirectoryExists() {
    std::filesystem::create_directories("data");
}

// "--seed" 인자가 주어지면 기본 더미 데이터셋으로 데이터를 채운다(개발/데모용).
bool HasSeedOption(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--seed") {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    ConfigureConsoleForUtf8();

#ifdef _DEBUG
    const int failedTests = TestFramework::RunAllTests();
    if (failedTests > 0) {
        std::cerr << "\n[_Debug] 실패한 테스트가 " << failedTests << "건 있습니다. 계속 진행합니다.\n";
    }
    std::cout << std::endl;
#endif

    EnsureDataDirectoryExists();

    Persistence::JsonDataStore store("data/samples.json", "data/orders.json");

    if (HasSeedOption(argc, argv)) {
        Dummy::DummyDataGenerator seeder(store);
        seeder.SeedDefaultDataset(/*reset=*/true);
        std::cout << "[--seed] 기본 더미 데이터셋을 생성했습니다.\n";
    }

    Monitor::OrderService orderService(store);
    Console::AppController app(store, orderService);
    app.Run();

    return 0;
}
