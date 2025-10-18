#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <memory>
#include <vector>
#include "scene/Scene.h"

namespace Scenes {
    class SceneManager {
    public:
        using ScenePtr = std::unique_ptr<Scene>;

        /**
         * @brief Adds a new scene to the manager and calls its OnLoad() method.
         * @param scene The scene to add.
         * @return The index of the added scene.
         */
        size_t AddScene(ScenePtr scene) {
            scene->OnLoad();
            m_scenes.push_back(std::move(scene));

            // If there is no active scene, make the first added scene active next frame
            if (m_active == NPOS && m_pendingActive == NPOS) {
                m_pendingActive = m_scenes.size() - 1;
            }

            return m_scenes.size() - 1;
        }

        /**
         * @brief Removes a scene by index.
         * @param index The index of the scene to remove.
         * @return True if the scene was removed, false if the index was invalid.
         */
        bool RemoveScene(size_t index) {
            if (index >= m_scenes.size())
                return false;

            // If active, exit first
            if (m_active == index) {
                m_scenes[m_active]->OnExit();
                m_active = NPOS;
            }

            // If pending, clear it
            if (m_pendingActive == index) {
                m_pendingActive = NPOS;
            }

            // Unload and erase
            m_scenes[index]->OnUnload();
            m_scenes.erase(m_scenes.begin() + index);

            // Fix indices post-erase
            if (m_active != NPOS && m_active > index)
                m_active--;
            if (m_pendingActive != NPOS && m_pendingActive > index)
                m_pendingActive--;

            return true;
        }

        /**
         * @brief Queues a scene to become active on the next Update boundary.
         * @param index The index of the scene to activate.
         */
        void SetActive(size_t index) {
            if (index < m_scenes.size()) {
                m_pendingActive = index;
            }
        }

        /**
         * @brief Immediately sets the active scene by index, performing necessary transitions.
         * @param index The index of the scene to activate.
         * @note This bypasses the pending mechanism and should be used cautiously.
         * @warning Using this method during scene updates may lead to inconsistent states.
         */
        void SetActiveImmediate(size_t index) {
            if (index >= m_scenes.size())
                return;
            
            _performTransition(index);
        }

        /**
         * @brief Gets the currently active scene.
         * @return Pointer to the active scene, or nullptr if none is active.
         */
        Scene* GetActive() {
            if (m_active == NPOS)
                return nullptr;

            return m_scenes[m_active].get();
        }

        /**
         * @brief Gets the currently active scene (const version).
         * @return Const pointer to the active scene, or nullptr if none is active.
         */
        const Scene* GetActive() const {
            if (m_active == NPOS)
                return nullptr;

            return m_scenes[m_active].get();
        }

        /** @brief Gets the index of the currently active scene. 
         * @return Index of the active scene, or size_t(-1) if none is active.
         */
        size_t GetActiveIndex() const  { return m_active; }

        /** @brief Gets the index of the pending active scene.
         * @return Index of the pending active scene, or size_t(-1) if none is pending.
         */
        size_t GetPendingIndex() const { return m_pendingActive; }

        /** @brief Gets the total number of scenes managed.
         * @return Number of scenes.
         */
        size_t GetSceneCount() const   { return m_scenes.size(); }

        /**
         * @brief Gets a scene by index.
         * @param index The index of the scene to retrieve.
         * @return Pointer to the scene, or nullptr if the index is invalid.
         */
        const Scene* GetScene(size_t index) const {
            if (index >= m_scenes.size()) return nullptr;
            return m_scenes[index].get();
        }

        /** @brief Updates the scene manager and the active scene.
         * This processes any pending scene transitions and updates the currently active scene.
         */
        void Update() {
            _processPending();
            if (m_active != NPOS) {
                m_scenes[m_active]->_update(Time::DeltaTime());
            }
        }

    private:
        void _processPending() {
            if (m_pendingActive == NPOS)
                return;

            _performTransition(m_pendingActive);
            m_pendingActive = NPOS;
        }

        void _performTransition(size_t toIndex) {
            if (toIndex >= m_scenes.size() || m_active == toIndex)
                return;

            if (m_active != NPOS)
                m_scenes[m_active]->OnExit();

            m_active = toIndex;
            m_scenes[m_active]->OnEnter();
        }

        std::vector<ScenePtr> m_scenes;
        static constexpr size_t NPOS = size_t(-1);
        size_t m_active = NPOS;
        size_t m_pendingActive = NPOS;
    };
}

#endif
