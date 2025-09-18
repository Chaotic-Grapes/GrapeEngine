#ifndef APPLICATION_H
#define APPLICATION_H

#define CREATE_WORLD() Engine::CORE->CreateWorld()
#define DESTROY_WORLD(world) Engine::CORE->DestroyWorld(world)
#define DESTROY_WORLD_INDEX(index) Engine::CORE->DestroyWorld(index)
#define DESTROY_ALL_WORLDS() Engine::CORE->DestroyAllWorlds()
#define GET_WORLD_COUNT() Engine::CORE->GetWorldCount()

#include <memory>
#include <vector>
#include "ecs/World.h"

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
         * @brief Destroy a specific world by reference
         *
		 * @param world The world to destroy
         */
        void DestroyWorld(World& world);

        /**
         * @brief Destroy a world by index
         *
		 * @param index The index of the world to destroy
         */
        void DestroyWorld(size_t index);

        /**
         * @brief Destroy all worlds
         */
        void DestroyAllWorlds();

        /**
		 * @brief Get the number of active worlds
		 */
        size_t GetWorldCount() const;

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
