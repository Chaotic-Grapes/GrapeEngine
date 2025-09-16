#ifndef APPLICATION_H
#define APPLICATION_H

#include <memory>
#include <vector>
#include "World.h"

namespace Engine {
    class Application {
public:
        Application();
        ~Application() = default;

        /**
		 * @brief Create a new world and return a reference to it.
		 */
        World& CreateWorld();

        /**
         * @brief Starts the engine
         *
		 * @param consoleFlag If true, runs with console output enabled
         */
        void Run(bool consoleFlag);

        /**
         * @brief Initializes engine and all attached systems. <br>
         *        Must be called after AttachSystem()
         */
        void Initialize() const;

        /**
         * @brief Ticks all attached systems and others as needed
         */
        void Update() const;

        /**
		 * @brief Close worlds and release resources.
		 */
        void Close();
    private:
		// Worlds created in the application
        std::vector<std::unique_ptr<World>> m_worlds;

        // Flag to indicate if application should stop
		static bool m_shouldStop;

		// Functions to enable/disable console output
        static void _enableConsole();
        static void _disableConsole();
};

    extern Application* CORE;
}

#endif
