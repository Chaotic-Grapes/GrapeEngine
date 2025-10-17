#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include "ecs/World.h"

namespace Scenes {
    class LayerManager {
    public:
        uint16_t CreateOrGetLayer(const std::string& name) {
            auto it = m_nameToId.find(name);
            if (it != m_nameToId.end())
                return it->second;

            uint16_t id = static_cast<uint16_t>(m_idToEntities.size());
            m_nameToId[name] = id;
            m_idToName.push_back(name);
            m_idToEntities.emplace_back();

            return id;
        }

        void OnLayerSet(ECS::Entity e, uint16_t id) {
            _ensureCapacity(id);
            m_idToEntities[id].insert(e);
        }
        void OnLayerRemoved(ECS::Entity e, uint16_t id) {
            if (id < m_idToEntities.size())
                m_idToEntities[id].erase(e);
        }

        const std::unordered_set<ECS::Entity, ECS::EntityHash>& EntitiesIn(uint16_t id) const {
            static const std::unordered_set<ECS::Entity, ECS::EntityHash> EMPTY{};
            if (id >= m_idToEntities.size())
                return EMPTY;

            return m_idToEntities[id];
        }

        std::optional<uint16_t> IdOf(const std::string& name) const {
            auto it = m_nameToId.find(name);
            if (it == m_nameToId.end())
                return std::nullopt;

            return it->second;
        }

    private:
        void _ensureCapacity(uint16_t id) {
            while (m_idToEntities.size() <= id)
                m_idToEntities.emplace_back();
        }

    private:
        std::unordered_map<std::string, uint16_t> m_nameToId;
        std::vector<std::string> m_idToName;
        std::vector<std::unordered_set<ECS::Entity, ECS::EntityHash>> m_idToEntities;
    };
}

#endif
