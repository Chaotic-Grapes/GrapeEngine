/* Start Header *****************************************************************/
/*!
\file    TestSceneManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the TestSceneManager class, which manages
TestScene instances (legacy test scenes with virtual lifecycle hooks).

TestSceneManager is for ENGINE TESTING ONLY. Production games should use
SceneManager with the pure data Scene class.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef TESTSCENEMANAGER_H
#define TESTSCENEMANAGER_H

#include "scene/TestScene.h"
#include "services/Time.h"
#include <memory>
#include <vector>

namespace Scenes {
    /**
     * @brief Manager for TestScene instances (legacy test scenes only).
     * 
     * TestSceneManager handles TestScene instances with virtual lifecycle hooks.
     * It provides the same API as SceneManager but works with TestScene instead.
     * 
     * @warning This class is for ENGINE TESTING ONLY.
     */
    class TestSceneManager {
    public:
        TestSceneManager() = default;
        ~TestSceneManager() = default;

        TestSceneManager(const TestSceneManager&) = delete;
        TestSceneManager& operator=(const TestSceneManager&) = delete;
        TestSceneManager(TestSceneManager&&) = default;
        TestSceneManager& operator=(TestSceneManager&&) = default;

        /**
         * @brief Adds a test scene and calls its OnLoad() method.
         * @param scene The test scene to add.
         * @return The index of the added scene.
         */
        size_t AddScene(TestScene* scene) {
            scene->OnLoad();
            m_scenes.push_back(std::unique_ptr<TestScene>(scene));

            if (m_active == NPOS && m_pendingActive == NPOS) {
                m_pendingActive = m_scenes.size() - 1;
            }

            return m_scenes.size() - 1;
        }

        /**
         * @brief Removes a test scene by index.
         * @param index The index of the scene to remove.
         * @return True if removed successfully.
         */
        bool RemoveScene(const size_t index) {
            if (index >= m_scenes.size())
                return false;

            if (m_active == index) {
                m_scenes[m_active]->OnExit();
                m_active = NPOS;
            }

            if (m_pendingActive == index) {
                m_pendingActive = NPOS;
            }

            m_scenes[index]->OnUnload();
            m_scenes.erase(m_scenes.begin() + index);

            if (m_active != NPOS && m_active > index)
                m_active--;
            if (m_pendingActive != NPOS && m_pendingActive > index)
                m_pendingActive--;

            return true;
        }

        /**
         * @brief Queues a scene to become active.
         * @param index The index of the scene to activate.
         */
        void SetActive(const size_t index) {
            if (index < m_scenes.size()) {
                m_pendingActive = index;
            }
        }

        /**
         * @brief Gets the currently active test scene.
         * @return Pointer to active scene, or nullptr.
         */
        TestScene* GetActive() const {
            if (m_active == NPOS)
                return nullptr;
            return m_scenes[m_active].get();
        }

        /**
         * @brief Gets the underlying Scene data from active test scene.
         * @return Pointer to Scene, or nullptr.
         */
        Scene* GetActiveScene() const {
            if (m_active == NPOS)
                return nullptr;
            return &m_scenes[m_active]->GetScene();
        }

        /**
         * @brief Gets the total number of test scenes.
         * @return Scene count.
         */
        size_t GetSceneCount() const { return m_scenes.size(); }

        /**
         * @brief Updates the test scene manager and active scene.
         */
        void Update() {
            _processPending();
            if (m_active != NPOS) {
                m_scenes[m_active]->OnUpdate();
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

        void _performTransition(const size_t toIndex) {
            if (toIndex >= m_scenes.size() || m_active == toIndex)
                return;

            if (m_active != NPOS)
                m_scenes[m_active]->OnExit();

            m_active = toIndex;
            m_scenes[m_active]->OnEnter();
        }

        std::vector<std::unique_ptr<TestScene>> m_scenes;
        static constexpr size_t NPOS = static_cast<size_t>(-1);
        size_t m_active = NPOS;
        size_t m_pendingActive = NPOS;
    };
}

#endif // TESTSCENEMANAGER_H
