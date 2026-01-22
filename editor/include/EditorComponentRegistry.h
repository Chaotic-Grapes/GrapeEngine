/* Start Header *****************************************************************/
/*!
\file   EditorComponentRegistry.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   15th November 2025

\brief
Editor UI registry that bridges the native ECS ComponentRegistry with editor operations.

This registry queries the native ECS::ComponentRegistry (which is the single source of truth
for all registered components - both C++ and C#) and provides editor-specific metadata:
- Display names for UI presentation
- Custom render functions for specialized component editors
- Default JSON values for new instances

The native registry is queried at runtime to discover all registered components. For each
component, the editor either uses a hardcoded specialized renderer (C++ components) or a
generic JSON editor (C# components).

This approach ensures:
1. Single source of truth: ECS::ComponentRegistry
2. No redundancy: C# components discovered at runtime are automatically available
3. Separation of concerns: Native registry handles ECS, editor registry handles UI
*/
/* End Header *******************************************************************/

#ifndef EDITOR_COMPONENT_REGISTRY_H
#define EDITOR_COMPONENT_REGISTRY_H

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "ecs/World.h"

class ComponentUI;

// =============================================================================
// Metadata Structure
// =============================================================================

struct ComponentUIMetadata {
    std::string DisplayName;    // "Sprite Renderer"
    std::string TypeName;       // "SpriteRenderer2D" 
    std::string FullTypeName;   // "ECS::Components::SpriteRenderer2D"
    ECS::ComponentTypeId ComponentId; // Unique ID from native registry (for deduplication)
    uint32_t TypeHash;          // FNV-1a hash from native registry
    bool CanDelete;             // Whether component can be removed from entities
    bool IsBuiltin;             // Whether this is a hardcoded C++ component (vs dynamically discovered C# component)

    // Render function for editing component properties
    // For C++ components: calls specialized RenderXXX function
    // For C# components: uses generic JSON editor
    std::function<void(ComponentUI&, nlohmann::json&, ECS::Entity, ECS::World*)> RenderUI;

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

// =============================================================================
// Registry Class
// =============================================================================

class ComponentRegistryUI {
public:
    // Returns all registered component metadata from the native ECS registry.
    // Queries ECS::ComponentRegistry to discover C++ and C# components.
    static const std::vector<ComponentUIMetadata>& GetAll();

    // Finds component metadata by type name (short or full).
    static const ComponentUIMetadata* Find(const std::string& typeName);

    // Rebuild the editor registry by querying the native ECS registry.
    // Called after C# components have been registered.
    static void RebuildFromNativeRegistry();

private:
    // Keep track of which native component IDs have been added to avoid duplicates
    static std::unordered_map<uint32_t, size_t>& _idToEditorIndex();
};

#endif