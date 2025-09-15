#include "Engine.h"
#include "Time.h"
#include "WindowManager.h"

int main() {
	auto* engine = new Engine::Engine();
	engine->AttachSystem(new Time());
	engine->AttachSystem(new WindowManager());

	engine->Initialize();
    engine->Run();

	engine->DestroySystems();
    return 0;
}
