#ifdef _DEBUG
#include "Engine.h"

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    Engine::Engine engine;
    engine.Run(true);
    return 0;
}
#endif
