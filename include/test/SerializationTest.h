/**
 * @file    SerializationTest.h
 * @author  k.danielneozuofeng
 * @date    26/09/2025
 * @brief   Automated test scene for serialization system
 *
 * This header defines the SerializationTestScene class, a sandbox
 * scene used to verify serialization and deserialization of entities
 * and components. It creates test entities, saves and reloads a scene,
 * and compares the results against expected data to ensure correctness.
 */

#ifndef SERIALIZATIONTEST_H
#define SERIALIZATIONTEST_H

#include "ecs/systems/RendererSystem.h"
#include "scene/TestScene.h"
#include "ecs/Entity.h"
#include <vector>
#include <string>
#include <memory>

namespace Sandbox {
    class SerializationTestScene : public Scenes::TestScene {
    public:
        void OnLoad() override;
        void OnUpdate() override;
        void OnFixedUpdate() override {}
        void OnLateUpdate() override {}
        void OnUnload() override;

    private:
        void RunAutomatedTest();
        void LoadPrefabAndVerify();
        void VerifyPrefabData(ECS::Entity entity);
        void PrintTestResults();

        ECS::Entity m_loadedEntity;
        bool m_testPassed = true;
        std::shared_ptr<ECS::RendererSystem> m_rendererSystem;
    };
}

#endif