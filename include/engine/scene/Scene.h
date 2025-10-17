#ifndef SCENE_H
#define SCENE_H

#include <functional>
#include <vector>
#include <memory>
#include "ecs/World.h"
#include "ecs/Hierarchy.h"
#include "scene/LayerManager.h"

namespace Scenes {
  class Scene {
  public:
      using System = std::function<void(Scene&, float)>;

      ECS::World& GetWorld()                { return m_world; }
      const ECS::World& GetWorld() const    { return m_world; }
      LayerManager& GetLayers()             { return m_layers; }
      const LayerManager& GetLayers() const { return m_layers; }

      void AddSystem(System sys) { m_systems.push_back(std::move(sys)); }

      void Update(float dt) {
          ECS::World::DeferGuard dg(m_world);
          for (auto& s : m_systems)
              s(*this, dt);

          ECS::Hierarchy::UpdateTransforms(m_world);
      }

      template<typename... Cs>
      ECS::Entity CreateOnLayer(uint16_t layerId, Cs&&... cs) {
          ECS::Entity e = m_world.Create(std::forward<Cs>(cs)...);
          m_world.Set<ECS::Layer>(e, ECS::Layer{layerId});
          m_layers.OnLayerSet(e, layerId);

          return e;
      }

      void SetLayer(ECS::Entity e, uint16_t id) {
          if (m_world.Has<ECS::Layer>(e)) {
              auto prev = m_world.Get<ECS::Layer>(e).Id;
              if (prev == id) return;

              m_layers.OnLayerRemoved(e, prev);
          }

          m_world.Set<ECS::Layer>(e, ECS::Layer{id});
          m_layers.OnLayerSet(e, id);
      }

      void RemoveFromLayer(ECS::Entity e) {
          if (!m_world.Has<ECS::Layer>(e)) return;

          auto prev = m_world.Get<ECS::Layer>(e).Id;
          m_layers.OnLayerRemoved(e, prev);
          m_world.Remove<ECS::Layer>(e);
      }

  private:
      ECS::World m_world;
      LayerManager m_layers;
      std::vector<System> m_systems;
  };
}

#endif
