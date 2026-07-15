#pragma once
// 콘솔 애플리케이션 최상위 컨트롤러 (docs/02-메인메뉴.md)
// 메인 메뉴 루프를 실행하고 각 하위 메뉴 View 로 분기한다.

#include "../Persistence/IDataStore.h"
#include "../Monitor/OrderService.h"

namespace Console {

class AppController {
public:
    AppController(Persistence::IDataStore& store, Monitor::OrderService& orderService);

    // 메인 메뉴 루프. [0] 입력 시 종료.
    void Run();

private:
    void ShowMainMenu();

    Persistence::IDataStore& store_;
    Monitor::OrderService& orderService_;
};

} // namespace Console
