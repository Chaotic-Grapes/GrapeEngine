#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include "ISystem.h"
#include "Window.h"

// TODO: Implement GetSystem(std::string)?
namespace Engine {
    class Engine {
public:
        Engine();
        ~Engine() = default;

        /// Attaches a system to the engine
        void AttachSystem(ISystem* system);

        /// Destroys all attached systems
        void DestroySystems() const;
    
        /// Updates all attached systems
        void Update() const;

        /// Initialize engine and its systems.
        /// This must be called after AttachSystem()
        void Initialize() const;

        /// Start the engine
        void Run();
    private:
        /// Systems currently attached to the Engine
        std::vector<ISystem*> m_systems;

		/// The main window of the engine
        Window m_mainWindow;
};

    extern Engine* CORE;
}
#endif // ENGINE_H
