#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <string>
#include "ecs/World.h"

class Scene final {
public:
    explicit Scene(std::string name) : m_name(std::move(name)), m_world(std::make_unique<World>()) {}

    const std::string& GetName() const { return m_name; }
    World& GetWorld() const { return *m_world; }

    // Called when scene becomes active
	// e.g. add systems unique to this scene
    void Load() const { m_world->_initialize(); }

    // Tear down entities and systems
    void Unload() const { m_world->_shutdown(); }

private:
    std::string m_name;
    std::unique_ptr<World> m_world;
};

#endif
