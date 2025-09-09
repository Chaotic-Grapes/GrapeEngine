#ifndef ENGINE_H
#define ENGINE_H

#include <vector>
#include "ISystem.h"

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
        void Update();

        /// Initialize engine and its systems.
        /// This must be called after AttachSystem()
        void Initialize() const;
    private:
        /// Systems currently attached to the Engine
        std::vector<ISystem*> Systems;

        /// The last time the game was updated
        unsigned LastTime;

        /// Flag to check if engine is still running
        bool IsRunning;
    };

    extern Engine* CORE;
}
#endif // ENGINE_H
