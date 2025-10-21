/* Start Header *****************************************************************/
/*!
\file   TransformSystem.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th October 2025
\brief
Defines the TransformSystem which is responsible for updating WorldTransform
components based on LocalTransform and parent-child hierarchy relationships.

The system ensures that child entities' world transforms are properly computed
by combining their local transforms with their parent's world transform.
*/
/* End Header *******************************************************************/

#pragma once
#include "ecs/World.h"

namespace ECS {
    class TransformSystem {
    public:
        /**
         * @brief Update all WorldTransform components based on LocalTransform and hierarchy
         * @param world The ECS world containing entities and components
         * @param dt Delta time (not used but provided for consistency with other systems)
         */
        static void Update(World& world, float dt);
        
    private:
        /**
         * @brief Recursively update an entity's WorldTransform and all its children
         * @param world The ECS world
         * @param entity The entity to update
         * @param parentMatrix The parent's world transform matrix (identity if no parent)
         */
        static void UpdateEntityRecursive(World& world, Entity entity, const Matrix4x4& parentMatrix);
    };
}
