/* Start Header *****************************************************************/
/*!
\file   EditorComponentRegistry.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   15th November 2025

\brief
UI-specific component registry for the inspector panel.

This registry stores editor-only metadata for component UI presentation. It provides 
display names, render functions and entity operations for all components. Unlike 
the runtime ECS registry, this handles only UI concerns, not memory management.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_COMPONENT_REGISTRY_H
#define EDITOR_COMPONENT_REGISTRY_H

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include "ecs/World.h"

class ComponentUI;

// -----------------------------------------------------------------------------
// Metadata Structure
// -----------------------------------------------------------------------------

struct ComponentUIMetadata {
    std::string DisplayName;    // "Sprite Renderer"
    std::string TypeName;       // "SpriteRenderer2D" 
    std::string FullTypeName;   // "ECS::Components::SpriteRenderer2D"
    bool CanDelete;             // Whether component can be removed from entities

    // std::function wraps any callable object (function pointer, lambda, functor)
    // Takes ComponentUI reference and JSON data, renders the component properties
    std::function<void(ComponentUI&, nlohmann::json&)> RenderUI;

    // Returns default JSON values for creating new component instances
    std::function<nlohmann::json()> GetDefaults;

    // Checks if entity has this component type
    std::function<bool(ECS::World*, ECS::Entity)> HasComponent;

    // Creates new component on entity with provided JSON data
    std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)> AddComponent;

    // Deletes component from entity
    std::function<void(ECS::World*, ECS::Entity)> RemoveComponent;

    // Updates existing component with new JSON data
    std::function<void(ECS::World*, ECS::Entity, const nlohmann::json&)> ApplyToEntity;
};

// -----------------------------------------------------------------------------
// Registry Class
// -----------------------------------------------------------------------------

class ComponentRegistryUI {
public:
    // Returns all registered component metadata for UI display and operations
    static const std::vector<ComponentUIMetadata>& GetAll();

    // Finds component metadata by type name, matches both short and full type names
    static const ComponentUIMetadata* Find(const std::string& typeName);
};

#endif