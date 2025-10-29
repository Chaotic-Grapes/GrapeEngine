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

#include "scene/Scene.h"
#include "ecs/Entity.h"
#include <vector>

namespace Sandbox {
    class SerializationTestScene : public Scenes::Scene {
    public:
        void OnLoad() override;
        void OnUpdate() override;
        void OnFixedUpdate() override {}
        void OnLateUpdate() override {}
        void OnUnload() override;

    private:
        void RunAutomatedTest();
        void CreateTestEntities();
        void SaveScene();
        void LoadScene();
        void VerifyLoadedEntities();
        void PrintTestResults();

        std::vector<ECS::Entity> m_originalEntities;
        bool m_testPassed = true;

        struct ExpectedData {
            std::string name;
            Vector3D position;
            Vector3D scale;
            Quaternion rotation;
            uint32_t textureId;
            float linearVelocityX;
            float linearVelocityY;
        };
        std::vector<ExpectedData> m_expectedData;
    };
}

#endif