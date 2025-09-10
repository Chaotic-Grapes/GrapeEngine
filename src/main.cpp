#include "Engine.h"
#include "Time.h"

int main() {
	auto* engine = new Engine::Engine();

	engine->Initialize();
    engine->Run();

    return 0;
}
