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
         * @brief Returns the main window of the engine
         * 
         * @return Window
		 */
        Window MainWindow() const;

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
         */
        void Run() const;
    private:
        // Systems currently attached to the Engine
        std::vector<ISystem*> m_systems;
};

    extern Engine* CORE;
}
#endif // ENGINE_H
