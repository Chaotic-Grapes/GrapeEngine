#include "Engine.h"
#include "Time.h"

int main() {
	auto* engine = new Engine::Engine();
	engine->AttachSystem(new Time());

	engine->Initialize();
    engine->Run();

	engine->DestroySystems();
    return 0;
}
