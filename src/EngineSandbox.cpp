#ifdef _DEBUG
#include <crtdbg.h>
#include "Application.h"

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    Engine::Application engine;
    engine.Run(true);
    return 0;
}
#endif
