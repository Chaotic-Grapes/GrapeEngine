#ifndef SERIALIZATIONTEST_H
#define SERIALIZATIONTEST_H

#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include <vector>

namespace Sandbox {
    class SerializationTestScene : public Scene {
    public:
        SerializationTestScene();
        ~SerializationTestScene() override = default;

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

        std::vector<Entity> m_originalEntities;
        bool m_testPassed = true;

        struct ExpectedData {
            std::string name;
            Vector2D position;
            Vector2D scale;
            float rotation;
            std::string texturePath;
            float mass;
            float radius;
        };
        std::vector<ExpectedData> m_expectedData;
    };
}

#endif