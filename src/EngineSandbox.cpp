// ******** NOTHING IS TO BE PLACED ABOVE _DEBUG ******** //
#ifdef _DEBUG

#include <crtdbg.h>
#include "Application.h"
#include "SandboxGame.h"

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    Engine::Application engine;
    SandboxGame game;
    engine.Run(game, true);

    return 0;
}

#endif