#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include <memory>
#include "ISystem.h"
#include "Window.h"
#include "../include/graphics/renderer.hpp"

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

        std::unique_ptr<Renderer> m_renderer; // RAII renderer owned by engine
};

    extern Engine* CORE;
}
#endif // ENGINE_H
