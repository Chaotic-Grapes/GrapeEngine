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
        
        /**
         *  @brief Attaches a system to the engine
         *
		 *  @param system The system to attach
         */
        void AttachSystem(ISystem* system);

        /**
         * @brief Destroys all attached systems
         */
        void DestroySystems() const;

        /**
		 * @brief Ticks all attached systems and others as needed
         */
        void Update() const;

        /**
         * @brief Initializes engine and all attached systems. <br>
         *        Must be called after AttachSystem()
		 */
        void Initialize() const;

        /**
         * @brief Starts the engine
         *
		 * @param consoleFlag If true, runs with console output enabled
         */
        void Run(bool consoleFlag);
    private:
        // Systems currently attached to the Engine
        std::vector<ISystem*> m_systems;

		// Functions to enable/disable console output
        static void _enableConsole();
        static void _disableConsole();
};

    extern Engine* CORE;
}
#endif // ENGINE_H
