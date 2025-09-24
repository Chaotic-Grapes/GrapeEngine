#ifndef SCENE_H
#define SCENE_H

#include <memory>
#include <string>
#include "ecs/World.h"

class Scene {
public:
    Scene(std::string name) : m_name(std::move(name)), m_world(std::make_unique<World>()) {}
    virtual ~Scene() = default;

    const std::string& GetName() const { return m_name; }
    World& GetWorld() const { return *m_world; }

    // Called when scene is loaded
    virtual void OnLoad() = 0;

    // Called every frame
    virtual void OnUpdate() {}

    // Called every frame after Update()
    virtual void OnLateUpdate() {}

    // Called when scene is unloaded
    virtual void OnUnload() {}

    void Update() {
	    m_world->_update();
    	OnUpdate();
    }

    void LateUpdate() {
	    m_world->_lateUpdate();
    	OnLateUpdate();
    }

    void Load() {
        OnLoad();
        m_world->_initialize();
    }

    void Unload() {
        m_world->_shutdown();
        OnUnload();
    }

protected:
    Entity CreateEntity() const { return m_world->CreateEntity(); }
private:
    std::string m_name;
    std::unique_ptr<World> m_world;
};

#endif
