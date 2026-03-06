/* Start Header *****************************************************************/
/*!
\file   InspectorPanel.cpp
\author Foo Rui Qin    (80%)
        Samantha Leong (20%)
\par    ruiqin.foo@digipen.edu
        s.leong@digipen.edu
\date   15th November 2025

\brief
Implementation of the editor panel for viewing and editing game entities and
prefab assets.

This file contains the runtime logic for the inspector panel including
selection handling, JSON conversion, component rendering, property editing,
component addition and removal, prefab creation, prefab synchronization and
updating linked instances. All component UI is forwarded to ComponentWidgets
through a unified system shared by both entities and prefab templates.
*/
/* End Header *******************************************************************/

#include "InspectorPanel.h"
#include "ComponentPropertyEditor.h"
#include "ComponentWidgets.h"
#include "EditorComponentRegistry.h"
#include "EditorECSUtils.h"
#include "core/Logger.h"
#include "serialization/EntitySerializer.h"
#include "EditorFileMenu.h"
#include "core/ProjectPaths.h"
#include "UndoSystem.h"
#include "ecs/World.h"
#include "ecs/Entity.h"
#include "ecs/PrefabManager.h"
#include "ecs/StringTable.h"
#include "helpers/PrefabUtils.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include "EditorStyle.h"
#include "EditorIcons.h"
#include "core/Application.h"
#include "scene/SceneManager.h"

namespace {
    constexpr int kMaxAnimSegments = ECS::Components::SpriteSheetAnimation2D::MaxSegments;

    int BuildSegmentSpans(const ECS::Components::SpriteSheetAnimation2D& anim,
        int totalCols, int totalRows,
        int(&starts)[kMaxAnimSegments], int(&counts)[kMaxAnimSegments]) {
        if (totalCols <= 0 || totalRows <= 0) {
            return 0;
        }

        const int segCount = std::clamp(static_cast<int>(anim.SegmentCount), 0, kMaxAnimSegments);
        int totalCount = 0;
        for (int i = 0; i < segCount; ++i) {
            const int row = std::clamp(anim.SegmentRows[i], 0, totalRows - 1);
            const int startCol = std::clamp(anim.SegmentOffsets[i], 0, totalCols - 1);
            const int available = totalCols - startCol;

            int count = anim.SegmentLengths[i];
            if (count <= 0 || count > available) {
                count = available;
            }
            if (count <= 0) {
                starts[i] = 0;
                counts[i] = 0;
                continue;
            }

            starts[i] = row * totalCols + startCol;
            counts[i] = count;
            totalCount += count;
        }
        return totalCount;
    }

    int ResolveSegmentAbsoluteFrame(const int(&starts)[kMaxAnimSegments], const int(&counts)[kMaxAnimSegments],
        int segmentCount, int localFrame) {
        int cursor = 0;
        for (int i = 0; i < segmentCount; ++i) {
            const int count = counts[i];
            if (count <= 0) continue;
            if (localFrame < cursor + count) {
                return starts[i] + (localFrame - cursor);
            }
            cursor += count;
        }
        return -1;
    }

    // Helper template function to safely add components during deserialization
    // Checks if the component type matches expected name before adding
    template <typename T>
    bool AddComponentIfMatch(ECS::World* world, ECS::Entity instance, const std::string& typeName,
        const std::string& expectedName, nlohmann::json& compData)
    {
        // If the json component name does not match what this function handles we skip it
        // This avoids adding the wrong component type to the entity
        if (typeName != expectedName) return false;

        // Only add the component if the entity does not already have it
        // world Has<T> checks if this entity already contains a component of type T
        if (!Editor::ECSUtils::HasComponent(world, instance, expectedName.c_str())) {
            T value{};
            // from_json fills the new component using values from the json 
            // This allows prefabs and saved scenes to restore component state exactly
            from_json(compData, value);
            Editor::ECSUtils::AddComponent(world, instance, expectedName.c_str(), value);
        }
        // true means this template handled the component successfully
        return true;
    }

    // Helper to check if an entity ID is protected from editing
    // Returns true if entity should NOT be inspected or modified
    bool IsProtectedEntity(ECS::World* world, EntityId entityId) {
        if (!world)
            return false;
        
        // Resolve the entity from its ID
        ECS::Entity entity = world->Resolve(entityId);

        // Not alive, cannot be protected
        if (!world->IsAlive(entity))
            return false;

        // Protect editor cameras from modification
        if (Editor::ECSUtils::HasComponent(world, entity, "CameraEditor3D"))
            return true;

        return false;
    }

    void MarkSceneDirtyIfNeeded(EditorFileMenu* fileMenu) {
        if (fileMenu) {
            fileMenu->MarkSceneDirty();
        }
    }

    void UpdateSpriteAnimationPreview(ECS::World* world, ECS::Entity entity) {
        if (!world || entity.IsNull() || !world->IsAlive(entity))
            return;
        if (!Editor::ECSUtils::HasComponent(world, entity, "SpriteSheetAnimation2D"))
            return;
        if (!Editor::ECSUtils::HasComponent(world, entity, "SpriteRenderer2D"))
            return;

        const auto* anim = Editor::ECSUtils::GetComponentPtr<ECS::Components::SpriteSheetAnimation2D>(world, entity, "SpriteSheetAnimation2D");
        auto* sprite = Editor::ECSUtils::GetComponentPtr<ECS::Components::SpriteRenderer2D>(world, entity, "SpriteRenderer2D");
        if (!anim || !sprite) {
            return;
        }

        if (anim->FrameWidth <= 0 || anim->FrameHeight <= 0 ||
            anim->SheetWidth <= 0 || anim->SheetHeight <= 0)
            return;

        const int totalCols = anim->SheetWidth / anim->FrameWidth;
        const int totalRows = anim->SheetHeight / anim->FrameHeight;
        if (totalCols <= 0 || totalRows <= 0)
            return;

        const bool useSegments = anim->UseSegments && anim->SegmentCount > 0;
        const bool useRow = anim->UseRow && !useSegments;

        int windowStart = 0;
        int windowCount = 0;
        int absoluteFrame = -1;
        int segmentStarts[kMaxAnimSegments] = { 0 };
        int segmentCounts[kMaxAnimSegments] = { 0 };
        int segmentCount = 0;
        if (useSegments) {
            windowCount = BuildSegmentSpans(*anim, totalCols, totalRows, segmentStarts, segmentCounts);
            segmentCount = std::clamp(static_cast<int>(anim->SegmentCount), 0, kMaxAnimSegments);
        }
        else if (useRow) {
            const int row = std::clamp(anim->Row, 0, totalRows - 1);
            const int startCol = std::clamp(anim->FrameOffset, 0, totalCols - 1);
            const int start = row * totalCols + startCol;
            const int totalFrames = totalCols * totalRows;
            const int rowAvailable = totalCols - startCol;
            const int maxFromStart = totalFrames - start;

            int count = anim->FrameLength;
            if (count <= 0) {
                count = rowAvailable;
            }
            else {
                count = std::clamp(count, 1, maxFromStart);
            }

            windowStart = start;
            windowCount = count;
        }
        else {
            const int totalFrames = totalCols * totalRows;
            windowStart = std::clamp(anim->StartFrame, 0, totalFrames - 1);
            windowCount = anim->FrameCount;
            if (windowCount <= 0) {
                windowCount = std::max(1, totalFrames - windowStart);
            }
            else {
                windowCount = std::min(windowCount, totalFrames - windowStart);
            }
        }

        if (windowCount <= 0)
            return;

        int localFrame = 0;
        if (Editor::ECSUtils::HasComponent(world, entity, "AnimationState2D")) {
            const auto* animState = Editor::ECSUtils::GetComponentPtr<ECS::Components::AnimationState2D>(world, entity, "AnimationState2D");
            if (animState) {
                localFrame = animState->CurrentFrame;
            }
        }
        localFrame = std::clamp(localFrame, 0, windowCount - 1);
        if (useSegments) {
            absoluteFrame = ResolveSegmentAbsoluteFrame(segmentStarts, segmentCounts, segmentCount, localFrame);
            if (absoluteFrame < 0) {
                return;
            }
        }
        else {
            absoluteFrame = windowStart + localFrame;
        }
        const int col = absoluteFrame % totalCols;
        const int rowTop = absoluteFrame / totalCols;
        const int rowBottom = (totalRows - 1) - rowTop;

        const float u0 = (col * anim->FrameWidth) / static_cast<float>(anim->SheetWidth);
        const float v0 = (rowBottom * anim->FrameHeight) / static_cast<float>(anim->SheetHeight);
        const float u1 = ((col + 1) * anim->FrameWidth) / static_cast<float>(anim->SheetWidth);
        const float v1 = ((rowBottom + 1) * anim->FrameHeight) / static_cast<float>(anim->SheetHeight);

        if (anim->TextureId != 0)
            sprite->TextureId = anim->TextureId;
        if (anim->NormalTextureId != 0)
            sprite->NormalTextureId = anim->NormalTextureId;
        sprite->Width = anim->FrameWidth;
        sprite->Height = anim->FrameHeight;
        sprite->Tiling = Vector2D{ u1 - u0, v1 - v0 };
        sprite->Offset = Vector2D{ u0, v0 };
    }

    bool SnapshotsEqual(const std::vector<ECS::SerializedComponent>& a,
        const std::vector<ECS::SerializedComponent>& b) {
        if (a.size() != b.size()) {
            return false;
        }

        auto sortById = [](const ECS::SerializedComponent& lhs, const ECS::SerializedComponent& rhs) {
            return lhs.Id < rhs.Id;
        };

        std::vector<ECS::SerializedComponent> left = a;
        std::vector<ECS::SerializedComponent> right = b;
        std::sort(left.begin(), left.end(), sortById);
        std::sort(right.begin(), right.end(), sortById);

        for (size_t i = 0; i < left.size(); ++i) {
            if (left[i].Id != right[i].Id || left[i].Data != right[i].Data) {
                return false;
            }
        }

        return true;
    }
}

// Helper functions for prefab synchronization and component matching
namespace {
    // Some serializers include the namespace in the component type name while others do not, 
    // so we need to be flexible when matching
    constexpr const char* kComponentPrefix = "ECS::Components::";

    // Strips known namespace prefixes from component type names for more flexible matching.
    // e.g. "ECS::Components::LocalTransform" -> "LocalTransform"
    std::string StripComponentPrefix(const std::string& typeName) {
        const std::string prefix = kComponentPrefix;

        // If the type name starts with the known prefix, remove it for matching purposes
        if (typeName.size() >= prefix.size() && typeName.compare(0, prefix.size(), prefix) == 0) {
            return typeName.substr(prefix.size());
        }

        // Otherwise, return the original type name unchanged
        return typeName;
    }

    // Checks if two component type names refer to the same component type,
    // ignoring whether or not either includes the "ECS::Components::" namespace prefix
    bool ComponentTypeMatches(const std::string& lhs, const std::string& rhs) {
        return lhs == rhs || StripComponentPrefix(lhs) == StripComponentPrefix(rhs);
    }

    // Retrieves the root node of a prefab JSON structure
    // Prefabs can appear in two formats:
    // - Wrapped: { "Entity": { "Components": [...], "Children": [...] } }
    // - Flat: { "Components": [...], "Children": [...] }
    const nlohmann::json* GetPrefabRootNode(const nlohmann::json& prefabData) {
        // Wrapped format: root is the "Entity" object
        if (prefabData.contains("Entity") && prefabData["Entity"].is_object()) {
            return &prefabData["Entity"];
        }
        // Flat format: root itself contains "Components" directly
        if (prefabData.contains("Components") && prefabData["Components"].is_array()) {
            return &prefabData;
        }
        // Neither format matched: invalid prefab structure
        return nullptr;
    }

    // Returns a pointer to the "Components" array inside a prefab node,
    // or nullptr if the node doesn't have one
    const nlohmann::json* GetPrefabNodeComponents(const nlohmann::json& prefabNode) {
        if (prefabNode.contains("Components") && prefabNode["Components"].is_array()) {
            return &prefabNode["Components"];
        }
        return nullptr;
    }

    // Checks whether a scene entity is an instance of any prefab in targetHashes
    // Supports two ways an entity can declare its prefab association:
    // - PrefabInstanceMetadata: stores the hash directly as "PrefabHash"
    // - PrefabLink: stores the prefab file path; the hash is computed from it
    bool SceneEntityMatchesPrefabHash(const nlohmann::json& entity, const std::vector<uint32_t>& targetHashes) {
		// If the entity doesn't have a Components array, it cannot be a prefab instance
        if (!entity.contains("Components") || !entity["Components"].is_array()) {
            return false;
        }

		// Check each component for prefab association metadata
        for (const auto& comp : entity["Components"]) {
			// Basic validation to ensure component has expected structure before accessing fields
            if (!comp.contains("TypeName") || !comp.contains("Data")) continue;
            if (!comp["TypeName"].is_string() || !comp["Data"].is_object()) continue;

			// Extract the component type name for matching
            const std::string typeName = comp["TypeName"].get<std::string>();

			// Check for PrefabInstanceMetadata which directly contains the prefab hash
            if (ComponentTypeMatches(typeName, "PrefabInstanceMetadata")) {
                // Hash is stored directly on the component
                if (!comp["Data"].contains("PrefabHash")) continue;
                uint32_t hashValue = 0;

				// Attempt to read the hash value, skip if it's not a valid uint32_t
                try { hashValue = comp["Data"]["PrefabHash"].get<uint32_t>(); }
                catch (...) { continue; }

				// If the hash matches any of the target hashes, this entity is an instance of a relevant prefab
                if (std::find(targetHashes.begin(), targetHashes.end(), hashValue) != targetHashes.end()) {
                    return true;
                }
            }

			// Check for PrefabLink which contains the prefab file path; we compute the hash from it
            else if (ComponentTypeMatches(typeName, "PrefabLink")) {
                // Hash is derived from the prefab file path
				// Validate that the prefabPath field exists and is a string before accessing
                if (!comp["Data"].contains("prefabPath") || !comp["Data"]["prefabPath"].is_string()) continue;

				// Compute the hash from the prefab path using the PrefabManager's hashing function
                const std::string path = comp["Data"]["prefabPath"].get<std::string>();

				// Normalize the path before hashing to ensure consistent hash values regardless of path formatting
                uint32_t hashValue = ECS::PrefabManager::ComputeHash(ECS::PrefabManager::NormalizePath(path));

				// If the computed hash matches any of the target hashes, this entity is an instance of a relevant prefab
                if (std::find(targetHashes.begin(), targetHashes.end(), hashValue) != targetHashes.end()) {
                    return true;
                }
            }
        }

		// No matching prefab association found in any component, so this entity does not match the target prefab hashes
        return false;
    }

	// Applies components from a prefab node to a scene entity JSON object, matching by component type name
    bool ApplyPrefabComponentsToSceneEntity(nlohmann::json& sceneEntity, const nlohmann::json& prefabNode, bool preserveRootTransform) {
		// Validate that the prefab node has a Components array before proceeding
        const nlohmann::json* prefabComponents = GetPrefabNodeComponents(prefabNode);
        if (!prefabComponents) {
            return false;
        }

		// Ensure the scene entity has a Components array to apply to; if not, create an empty one
        if (!sceneEntity.contains("Components") || !sceneEntity["Components"].is_array()) {
            sceneEntity["Components"] = nlohmann::json::array();
        }
        auto& sceneComponents = sceneEntity["Components"];

		// We will track whether any modifications are made to the scene entity's components so we can return that information
        bool modified = false;

		// For each component defined in the prefab node, we will try to find a matching component in the scene entity by type name
        for (const auto& prefabComp : *prefabComponents) {
			// Basic validation to ensure the prefab component has expected structure before accessing fields
            if (!prefabComp.contains("TypeName") || !prefabComp.contains("Data")) continue;
            if (!prefabComp["TypeName"].is_string() || !prefabComp["Data"].is_object()) continue;

			// Extract the component type name from the prefab component for matching
            const std::string prefabTypeName = prefabComp["TypeName"].get<std::string>();

			// For matching purposes, we will compare component type names in a flexible way that ignores the presence 
            // or absence of the "ECS::Components::" namespace prefix
            const std::string prefabShortType = StripComponentPrefix(prefabTypeName);
            bool found = false;

			// Search for a matching component in the scene entity's components by type name
            for (auto& sceneComp : sceneComponents) {
				// Basic validation to ensure the scene component has expected structure before accessing fields
                if (!sceneComp.contains("TypeName") || !sceneComp["TypeName"].is_string()) continue;

				// Extract the component type name from the scene component for matching
                const std::string sceneTypeName = sceneComp["TypeName"].get<std::string>();

				// Check if the component type names match (ignoring namespace prefixes)
                if (!ComponentTypeMatches(sceneTypeName, prefabTypeName)) continue;

				// If this component type matches, we will update the scene component's data to match the prefab component's data
                nlohmann::json newData = prefabComp["Data"];

				// If this is the root entity and the component is a transform, we may want to preserve the existing position and 
                // rotation in the scene to avoid moving the entity unexpectedly when applying the prefab
                if (preserveRootTransform && prefabShortType == "LocalTransform" && sceneComp.contains("Data") && sceneComp["Data"].is_object()) {
                    if (sceneComp["Data"].contains("Position")) {
                        newData["Position"] = sceneComp["Data"]["Position"];
                    }
                    if (sceneComp["Data"].contains("Rotation")) {
                        newData["Rotation"] = sceneComp["Data"]["Rotation"];
                    }
                }

				// Only update the scene component's data if it is different from the prefab component's data to avoid unnecessary 
                // modifications
                if (!sceneComp.contains("Data") || sceneComp["Data"] != newData) {
                    sceneComp["Data"] = newData;
                    modified = true;
                }

                found = true;
                break;
            }

			// If we did not find a matching component in the scene entity, we will add this prefab component to the scene entity's 
            // components
            if (!found) {
                nlohmann::json newComp = nlohmann::json::object();
                newComp["TypeName"] = prefabShortType;
                newComp["Data"] = prefabComp["Data"];
                sceneComponents.push_back(std::move(newComp));
                modified = true;
            }
        }

        return modified;
    }

	// Builds a mapping of parent entity indices to their child entity indices based on the "Hierarchy" array in the scene JSON
    std::unordered_map<size_t, std::vector<size_t>> BuildSceneChildrenMap(const nlohmann::json& sceneJson) {
		// The scene JSON may contain a "Hierarchy" array that defines parent-child relationships between entities by their indices 
        // in the "Entities" array
        std::unordered_map<size_t, std::vector<size_t>> childrenByParent;
        if (!sceneJson.contains("Hierarchy") || !sceneJson["Hierarchy"].is_array()) {
            return childrenByParent;
        }

		// Each entry in the "Hierarchy" array should have a "parent" index and a "child" index
        // We will iterate through this array and build a mapping of parent indices to their child indices for easy lookup when 
        // applying prefab hierarchies
        for (const auto& relation : sceneJson["Hierarchy"]) {
			// Basic validation to ensure the hierarchy relation has expected structure before accessing fields
            if (!relation.contains("child") || !relation.contains("parent")) continue;
            if (!relation["child"].is_number_unsigned() || !relation["parent"].is_number_unsigned()) continue;

			// Extract the parent and child indices from the hierarchy relation
            const size_t child = relation["child"].get<size_t>();
            const size_t parent = relation["parent"].get<size_t>();
            childrenByParent[parent].push_back(child);
        }

		// Return the mapping of parent entity indices to their child entity indices for use in prefab hierarchy application
        return childrenByParent;
    }

	// Recursively applies components from a prefab hierarchy to a scene entity and its children, matching by component type name
    bool ApplyPrefabHierarchyToSceneEntity(nlohmann::json& entities, const std::unordered_map<size_t, std::vector<size_t>>& sceneChildrenByParent,
        size_t sceneEntityIndex, const nlohmann::json& prefabNode, bool preserveRootTransform)
    {
		// Validate that the scene entity index is within bounds and that the target scene entity is an object before proceeding
        if (sceneEntityIndex >= entities.size()) {
            return false;
        }
        if (!entities[sceneEntityIndex].is_object()) {
            return false;
        }

		// First, apply components from the current prefab node to the target scene entity
        bool modified = ApplyPrefabComponentsToSceneEntity(entities[sceneEntityIndex], prefabNode, preserveRootTransform);

		// Then, if the prefab node has children, we will recursively apply the corresponding child prefab nodes to the child scene entities
        const bool hasPrefabChildren = prefabNode.contains("Children") && prefabNode["Children"].is_array();
        if (!hasPrefabChildren) {
            return modified;
        }

		// Look up the child scene entities of the current scene entity using the mapping we built from the scene's "Hierarchy" array
        const auto sceneIt = sceneChildrenByParent.find(sceneEntityIndex);
        const size_t sceneChildCount = (sceneIt != sceneChildrenByParent.end()) ? sceneIt->second.size() : 0;
        const size_t prefabChildCount = prefabNode["Children"].size();
        const size_t applyCount = std::min(prefabChildCount, sceneChildCount);

		// If the prefab and scene child counts do not match, we will log a warning and only apply to the overlapping children to 
        // avoid out-of-bounds errors
        if (prefabChildCount != sceneChildCount) {
            LOG_WARNING("Prefab/scene child count mismatch at scene entity index " << sceneEntityIndex
                << " (prefab = " << prefabChildCount << ", scene = " << sceneChildCount << "); applying overlapping children only");
        }

		// Recursively apply each child prefab node to the corresponding child scene entity
        for (size_t i = 0; i < applyCount; i++) {
			// Look up the index of the child scene entity from the mapping; if the mapping is missing or malformed we will skip to 
            // avoid errors
            const size_t childSceneIndex = sceneIt->second[i];
            const auto& childPrefabNode = prefabNode["Children"][i];

			// Recursively apply the child prefab node to the child scene entity; if any modifications are made, we will mark the parent as
			// modified so that the updated scene JSON will be saved back to disk
            if (ApplyPrefabHierarchyToSceneEntity(entities, sceneChildrenByParent, childSceneIndex, childPrefabNode, false)) {
                modified = true;
            }
        }

        return modified;
    }

	// Main function to update all instances of a prefab in a scene file by applying the prefab's components to matching entities based on 
    // prefab hash association
    void UpdatePrefabInSceneFile(const std::filesystem::path& scenePath, const nlohmann::json& prefabData, const std::vector<uint32_t>& targetHashes) {
		// Open the scene JSON file from disk
        std::ifstream inFile(scenePath);
        if (!inFile.is_open()) {
            return;
        }

		// Read the entire scene JSON from the file; if parsing fails we will close the file and exit to avoid errors
        nlohmann::json sceneJson;
        try {
            inFile >> sceneJson;
        }
        catch (...) {
            inFile.close();
            return;
        }
        inFile.close();

		// Validate that the scene JSON has an "Entities" array before proceeding, as we need this to find and update prefab instances; if it's
		// missing or malformed we will exit to avoid errors
        if (!sceneJson.contains("Entities") || !sceneJson["Entities"].is_array()) {
            return;
        }

		// Retrieve the root node of the prefab JSON structure, which contains the components and children to apply; if the prefab JSON is missing
		// the expected structure we will exit to avoid errors
        const nlohmann::json* prefabRootNode = GetPrefabRootNode(prefabData);
        if (!prefabRootNode) {
            return;
        }

		// We will iterate through all entities in the scene and look for those that are instances of the target prefab(s) based on their prefab 
        // hash association
        auto& entities = sceneJson["Entities"];
        const auto sceneChildrenByParent = BuildSceneChildrenMap(sceneJson);
        bool sceneModified = false;

		// For each entity in the scene, we will check if it is an instance of any of the target prefabs by looking for prefab association metadata in its
		// components; if it is an instance, we will apply the prefab's components to the scene entity and its children according to the prefab hierarchy
        for (size_t i = 0; i < entities.size(); i++) {
            if (!entities[i].is_object()) continue;
            if (!SceneEntityMatchesPrefabHash(entities[i], targetHashes)) continue;

            if (ApplyPrefabHierarchyToSceneEntity(entities, sceneChildrenByParent, i, *prefabRootNode, true)) {
                sceneModified = true;
            }
        }

		// If any modifications were made to the scene JSON, we will write the updated JSON back to disk to save the changes
        if (sceneModified) {
            std::ofstream outFile(scenePath);
            if (!outFile.is_open()) {
                return;
            }
            outFile << sceneJson.dump(4);
        }
    }
}

// -------------------------------------------------------------------------
// Lifecycle Management
// -------------------------------------------------------------------------

// Set up fonts and world pointer so the inspector can render UI and talk to ECS
void InspectorPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;

    // Forward fonts into the smaller ComponentUI helper so it can draw fields
    m_componentUI.Initialize(mainFont, boldFont, symbolsFont);
    
    // NOTE: Do NOT call RebuildFromNativeRegistry() here!
    // The native registry won't have C# components yet (they're loaded on a background thread)
    // Instead, we rebuild after LoadAssembly() succeeds in EditorMain
}

// Update the world context and clear any stale selection from a previous world
void InspectorPanel::SetWorld(ECS::World* world) {
    m_world = world;
    ClearSelection();
}

// -------------------------------------------------------------------------
// Selection Management
// -------------------------------------------------------------------------

// Switch inspector into entity mode and validate the entity we want to inspect
void InspectorPanel::InspectEntity(EntityId id) {
    // Block inspection of protected system entities
    if (IsProtectedEntity(m_world, id)) {
        m_mode = InspectionMode::None;
        m_entityId = 0; // Clear selection
        m_editState.isEditing = false; // Reset edit state
        m_editState.startComponents.clear();
        m_editState.hasSnapshot = false;
        return;
    }

    const EntityId previousId = m_entityId;

    // IMPORTANT: Clear edit state when entity selection changes
    // This prevents the inspector from comparing the new entity's transform
    // against the previous entity's captured transform
    if (previousId != id) {
        m_editState.isEditing = false;
        m_editState.entityId = 0;
        m_editState.startComponents.clear();
        m_editState.hasSnapshot = false;
    }

    m_entityId = id;

    // If we do not have a world there is nothing to inspect
    if (!m_world) {
        m_mode = InspectionMode::None;
        return;
    }

    // Wrap the ID into an ECS::Entity handle (use Resolve() instead of hardcoding generation 0)
    ECS::Entity e = m_world->Resolve(id);
    if (!m_world->IsAlive(e)) {
        // Entity might have been deleted so we reset the mode
        m_mode = InspectionMode::None;
        return;
    }

    // Ensure all entities have Transform component (mandatory)
    // We use the registry to check and auto add defaults when missing
    const auto* transformMeta = ComponentRegistryUI::Find("LocalTransform");
    if (transformMeta && !transformMeta->HasComponent(m_world, e)) {
        transformMeta->AddComponent(m_world, e, transformMeta->GetDefaults());
    }

    m_mode = InspectionMode::Entity;
}

// Load a prefab file from disk and switch into prefab editing mode
void InspectorPanel::InspectPrefab(const std::string& path) {
    m_editState.isEditing = false;
    m_editState.entityId = 0;
    m_editState.startComponents.clear();
    m_editState.hasSnapshot = false;
    m_selectedPrefabNodePath.clear();

    // Usual checks
    if (path.empty()) {
        m_statusMessage = "Failed: No prefab path";
        m_statusTimer = 3.0f;
        return;
    }

    // Try to open the prefab JSON file
    std::ifstream file(path);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot open prefab";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot open prefab file: " << path);
        return;
    }

    try {
        // Read entire file into a string so we can detect empty files
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Handle empty files
        // If the file is blank we create a prefab that at least has a Transform
        if (content.empty() || content.find_first_not_of(" \t\n\r") == std::string::npos) {
            const auto* transformMeta = ComponentRegistryUI::Find("LocalTransform");
            nlohmann::json defaultTransform = transformMeta ? transformMeta->GetDefaults() : nlohmann::json::object();

            // Build a minimal prefab JSON structure with a single Transform component
            m_prefabData = nlohmann::json{
                {"Components", nlohmann::json::array({
                    {{"TypeName", "ECS::Components::LocalTransform"}, {"Data", defaultTransform}}
                })}
            };

            // The file was empty so we are generating a new prefab structure
            // Set lastSavedPrefabHash to 0 so the next save is forced
            // This establishes a valid on-disk baseline for hash comparison
            m_prefabPath = path;
            m_lastSavedPrefabHash = 0;
            m_mode = InspectionMode::Prefab;

            // Immediately write this new prefab back to disk
            _savePrefabData();

            m_statusMessage = "Opened empty prefab, added Transform";
            m_statusTimer = 2.0f;
            return;
        }

        // Non empty file path
        // Parse JSON and set up state for editing
        m_prefabData = nlohmann::json::parse(content);
        m_prefabPath = path;

        /*
        Take the entire prefab JSON, convert it to a string
        Hash the string (turn it into a number), then store that number

        This hash acts like a fingerprint of the prefab at the moment we saved it
        Next time we try to save, we compute a new hash and compare it with this one
        If the hashes match, it means nothing changed, so we skip rewriting the file
        If they differ, it means the prefab was modified, so we save again
        */
        m_lastSavedPrefabHash = std::hash<std::string>{}(m_prefabData.dump());
        m_mode = InspectionMode::Prefab;
    }
    catch (const std::exception& e) {
        // Any parse or other exception means the JSON is invalid
        m_statusMessage = "Failed: Invalid JSON in prefab";
        m_statusTimer = 3.0f;
        m_mode = InspectionMode::None;
        LOG_ERROR("Failed to parse prefab JSON: " << e.what());
    }
}

// Reset inspector so nothing is selected and state is clean
void InspectorPanel::ClearSelection() {
    m_mode = InspectionMode::None;
    m_entityId = 0;
    m_editState.isEditing = false;
    m_editState.entityId = 0;
    m_editState.startComponents.clear();
    m_editState.hasSnapshot = false;
    m_prefabPath.clear();
    m_prefabData = {};
    m_selectedPrefabNodePath.clear();
    m_componentsToDelete.clear();
}

// Request that the inspector window gets focused on the next render pass
void InspectorPanel::RequestFocus() {
    m_focusOnNextRender = true;
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Draw the inspector window based on current mode (none, entity, prefab)
void InspectorPanel::Render() {
    // Use main editor font for the whole inspector window
    ImGui::PushFont(m_mainFont);

    // Window name changes depending on what we are editing
    const bool isPrefab = (m_mode == InspectionMode::Prefab);
    const char* windowTitle = isPrefab ? "Prefab Editor" : "Property Editor";

	// Focus the window if requested (e.g. after clicking on an entity in the hierarchy)
    if (m_focusOnNextRender) {
        ImGui::SetNextWindowFocus();
        m_focusOnNextRender = false;
    }
    ImGui::Begin(windowTitle);

    // Keyboard shortcuts for common inspector actions
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            m_focusComponentFilter = true; // Focus component/property filter
        }
        if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            m_openAddComponentPopup = true; // Open Add Component popup
            m_focusAddComponentSearch = true; // Focus search input inside popup
        }
    }

    if (m_mode == InspectionMode::None) {
        ImGui::TextDisabled("No selection");
    }
    else if (m_mode == InspectionMode::Entity) {
        _renderEntityInspector();
    }
    else if (m_mode == InspectionMode::Prefab) {
        _renderPrefabInspector();
    }

    // Always draw status bar at the bottom to show feedback messages
    _renderStatusBar();

    ImGui::End();
    ImGui::PopFont();
}

// -------------------------------------------------------------------------
// Entity Inspector Implementation
// -------------------------------------------------------------------------

// Top level entry for entity mode
// Draws header, components and Add Component area for the selected entity
void InspectorPanel::_renderEntityInspector() {
    if (!m_world) {
        ImGui::TextDisabled("No entity selected");
        return;
    }

    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (!m_world->IsAlive(entity)) {
        ImGui::TextDisabled("Entity invalid");
        return;
    }

    _renderEntityHeader(entity);
    _renderEntityComponents(entity);
    _renderAddComponentButton(entity);
}

// Draw entity name, ID and prefab link information at the top of the inspector
void InspectorPanel::_renderEntityHeader(ECS::Entity entity) {
    // Try to read the Name component for display
    std::string entityName = "Unnamed";
    if (const auto* nameComp = Editor::ECSUtils::GetNamePtr(m_world, entity)) {
        std::string resolved = ECS::StringTable::Resolve(nameComp->Value);
        if (!resolved.empty()) {
            entityName = resolved;
        }
    }

    // Show the basic header line: Entity <Name> (ID)
    ImGui::Text("Entity ");
    ImGui::SameLine();
    ImGui::TextDisabled("%s (ID: %u)", entityName.c_str(), (unsigned)m_entityId);

    // If this entity came from a prefab show the link and an Open Prefab button
    if (m_world && m_world->Has<ECS::Components::PrefabInstanceMetadata>(entity)) {
        const auto& meta = m_world->Get<ECS::Components::PrefabInstanceMetadata>(entity);
        ImGui::Separator();
        ImGui::Text("Prefab Instance");

        // Show the prefab path (looked up from PrefabManager)
        std::string prefabPath;
        if (m_prefabManager) {
            prefabPath = m_prefabManager->GetPrefabPath(meta.PrefabHash);
        }
        
        ImGui::SameLine();
        if (!prefabPath.empty()) {
            ImGui::TextDisabled("%s", std::filesystem::path(prefabPath).filename().string().c_str());
        } else {
            ImGui::TextDisabled("0x%08X", meta.PrefabHash);
        }

        // Show modification indicator if modified
        if (PrefabUtils::IsModified(meta)) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "(Modified)");
        }

        // Button to open the original prefab template for editing (only if we have path)
        if (!prefabPath.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Open Prefab")) {
                // Switch inspector into prefab mode using the linked path
                InspectPrefab(prefabPath);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Opens the prefab template file for editing");
            }
        }

        ImGui::Separator();
    }
    // If NOT a prefab instance, show an empty prefab slot with drag - drop
    else {
        ImGui::Separator();
        ImGui::Text("Prefab Link");
        ImGui::SameLine();
        ImGui::TextDisabled("None (drag .prefab here to link)");

        // Allow drag and drop of assets onto this row
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                // payload->Data is a char* containing the file path
                std::string droppedPath = static_cast<const char*>(payload->Data);

                // Only allow .prefab files to be dropped here
                if (std::filesystem::path(droppedPath).extension() == ".prefab") {
                    // Add PrefabInstanceMetadata with registered hash
                    uint32_t hash = 0;
                    if (m_prefabManager) {
                        hash = m_prefabManager->RegisterPrefab(droppedPath);
                        m_prefabManager->TrackInstance(entity, hash);
                    } else {
                        hash = ECS::PrefabManager::ComputeHash(
                            ECS::PrefabManager::NormalizePath(droppedPath)
                        );
                    }

                    ECS::Components::PrefabInstanceMetadata meta;
                    meta.PrefabHash = hash;
                    meta.Flags = 0;
                    m_world->Add<ECS::Components::PrefabInstanceMetadata>(entity, meta);

                    m_statusMessage = "Prefab linked to entity";
                    m_statusTimer = 2.0f;
                }
                else {
                    m_statusMessage = "Not a prefab: drop a .prefab file";
                    m_statusTimer = 2.0f;
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
                const char* data = static_cast<const char*>(payload->Data);
                const char* end = data + payload->DataSize;
                while (data < end) {
                    std::string path(data);
                    data += path.size() + 1;
                    if (path.empty()) continue;
                    if (std::filesystem::path(path).extension() != ".prefab") continue;

                    // Add PrefabInstanceMetadata with registered hash
                    uint32_t hash = 0;
                    if (m_prefabManager) {
                        hash = m_prefabManager->RegisterPrefab(path);
                        m_prefabManager->TrackInstance(entity, hash);
                    } else {
                        hash = ECS::PrefabManager::ComputeHash(
                            ECS::PrefabManager::NormalizePath(path)
                        );
                    }

                    ECS::Components::PrefabInstanceMetadata meta;
                    meta.PrefabHash = hash;
                    meta.Flags = 0;
                    m_world->Add<ECS::Components::PrefabInstanceMetadata>(entity, meta);

                    m_statusMessage = "Prefab linked to entity";
                    m_statusTimer = 2.0f;
                    break;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();
    }
}

// Render all components on an entity using JSON as a temporary editable buffer
void InspectorPanel::_renderEntityComponents(ECS::Entity entity) {
    // Footer needs 2 lines: one for button row, one for status message
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Component/property filter input for the inspector list
    if (m_focusComponentFilter) {
        ImGui::SetKeyboardFocusHere();
        m_focusComponentFilter = false;
    }
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::InputTextWithHint("##ComponentFilter", "Filter components/fields...", m_componentFilterBuffer, sizeof(m_componentFilterBuffer))) {
        m_componentFilter = m_componentFilterBuffer;
    }

    // Use smaller padding inside the scrolling region for components
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("EntityComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Convert the entity into JSON so we can use the same UI path as prefabs
    // UI edits this JSON, then we sync the edits back into the ECS
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);

    // LOG_DEBUG("[InspectorPanel] Serialized entity " << entity.Index << ": " << entityJson.dump(2));

    // Make sure the JSON has a component list we can iterate
    if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
        ImGui::Dummy(ImVec2(0, 4));

        bool wasEdited = false;

        static nlohmann::json editStartState;
        static bool isEditing = false;
        
        // Track which components were actually modified this frame
        std::unordered_set<std::string> modifiedComponents;

        // Apply property filter so per-field rows can be narrowed
        if (m_componentFilter.empty()) {
            EditorUI::ClearPropertyFilter();
        } else {
            EditorUI::SetPropertyFilter(m_componentFilter);
        }

        // Precompute the component name/type filter once per frame
        std::string filterLower;
        if (!m_componentFilter.empty()) {
            filterLower = m_componentFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        // First pass: draw every component using registry metadata
        for (auto& componentEntry : entityJson["Components"]) {
            // Basic validation
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;
            std::string typeName = componentEntry["TypeName"];

            // Look up metadata which tells us how to draw this component
            const auto* meta = ComponentRegistryUI::Find(typeName);

            // To log missing metadata for debugging
            // if (!meta) {
            //     LOG_WARNING("[InspectorPanel] No UI metadata for component '" << typeName << "' while rendering entity " << entity.Index);
            // }
            if (meta) {
                auto& data = componentEntry["Data"];

                size_t hashBefore = std::hash<std::string>{}(data.dump());

                // Component name filter to reduce noise in long inspectors
                if (!filterLower.empty()) {
                    std::string nameLower = meta->DisplayName;
                    std::string typeLower = meta->TypeName;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (nameLower.find(filterLower) == std::string::npos && typeLower.find(filterLower) == std::string::npos) {
                        continue;
                    }
                }

                const nlohmann::json defaults = meta->GetDefaults(); // Defaults used for reset + per-field reset

                // UI renderer callback: InspectorPanel calls this and forwards to ComponentWidgets
                // via the ComponentUI helper to draw the actual fields
                _renderComponentSection(meta->DisplayName, meta->TypeName, data,
                    [this, meta, entity](nlohmann::json& d) { meta->RenderUI(m_componentUI, d, entity, m_world); }, meta->CanDelete, &defaults);

                size_t hashAfter = std::hash<std::string>{}(data.dump());

                if (hashBefore != hashAfter) {
                    if (!isEditing) {
                        editStartState = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
                        isEditing = true;
                    }
                    wasEdited = true;
                    modifiedComponents.insert(typeName); // Track which component changed
                }

                ImGui::Dummy(ImVec2(0, 4));
            }
        }

        // Clear the property filter after rendering component rows
        EditorUI::ClearPropertyFilter();

        // Process deferred deletions after UI loop so we don't mutate while iterating
        // Also remove deleted components from the JSON buffer so they are not re-applied below
        for (const auto& type : m_componentsToDelete) {
            // Pull out Components array (early-continue instead of nesting)
            if (!entityJson.contains("Components")) continue;
            if (!entityJson["Components"].is_array()) continue;

            auto& comps = entityJson["Components"];

            // Manual iterator loop because we may erase while iterating
            for (auto it = comps.begin(); it != comps.end(); ) {
                // Bail early if no valid TypeName
                bool hasTypeName = it->contains("TypeName") && (*it)["TypeName"].is_string();
                if (!hasTypeName) {
                    it++;
                    continue;
                }

                std::string tn = (*it)["TypeName"];

                // Check short or fully-qualified names
                bool matches = (tn == type) || (tn == "ECS::Components::" + type);
                if (matches) {
                    it = comps.erase(it);   // Erase returns next iterator
                    continue;               // Do not increment manually
                }

                // Nothing erased
                it++;
            }

            // Remove actual ECS component last
            _removeComponentFromEntity(type);
        }

        m_componentsToDelete.clear();

        if (!m_editState.isEditing && ImGui::IsAnyItemActive()) {
            m_editState.isEditing = true;
            m_editState.entityId = entity.Index;

            // Snapshot current component state before edits begin.
            if (m_undoSystem) {
                m_editState.startComponents = m_world->CaptureEntityComponents(entity);
                m_editState.hasSnapshot = true;
            }
        }

        // Keep sprite preview in sync even when the animation UI is collapsed.
        UpdateSpriteAnimationPreview(m_world, entity);

        // Second pass: push any edited JSON values back into ECS components
        // IMPORTANT: Only apply components that were actually modified to prevent
        // unnecessarily overwriting entity data every frame (which can cause teleporting)
        for (const auto& componentEntry : entityJson["Components"]) {
            // Validate again; same same
            if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
            if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

            std::string typeName = componentEntry["TypeName"];
            
            // Only apply if this component was modified this frame
            if (modifiedComponents.find(typeName) == modifiedComponents.end()) {
                continue; // Skip unmodified components
            }

            const auto* meta = ComponentRegistryUI::Find(typeName);
            // Apply edited JSON to the actual ECS component
            if (meta) {
                LOG_DEBUG("[Inspector] Applying modified component: " << typeName << " to entity " << entity.Index);
                meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);
            }
        }

        // Re-serialize after applying changes so the displayed values stay fresh
        // This ensures if the component was modified by transform system or other systems,
        // the inspector shows the current values rather than stale cached values
        if (wasEdited) {
            entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
        }

        // Record undo when editing finishes
        if (m_editState.isEditing && !ImGui::IsAnyItemActive()) {
            if (m_undoSystem && m_editState.hasSnapshot) {
                // Capture post-edit state and push a generic snapshot command.
                auto endSnapshot = m_world->CaptureEntityComponents(entity);
                if (!SnapshotsEqual(m_editState.startComponents, endSnapshot)) {
                    auto command = std::make_unique<Editor::EntityComponentsSnapshotCommand>(
                        m_world, entity, std::move(m_editState.startComponents), std::move(endSnapshot)
                    );
                    m_undoSystem->ExecuteCommand(std::move(command));
                }
            }

            m_editState.isEditing = false;
            m_editState.hasSnapshot = false;
            m_editState.startComponents.clear();
        }

        // MARK SCENE AS DIRTY if anything was edited
        if (wasEdited) {
            MarkSceneDirtyIfNeeded(m_fileMenu);
        }
    }
    // else {
    //     // Debug: show why components aren't rendering
    //     ImGui::TextDisabled("No Components array in serialized entity");
    //     LOG_ERROR("[InspectorPanel] Entity " << entity.Index << " has no Components array. JSON: " << entityJson.dump(2));
    // }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Render any drag/drop validation feedback triggered by component fields
    m_componentUI.RenderAssetDropFeedbackPopup();
}

// Renders the Add Component button row at the bottom of the inspector
void InspectorPanel::_renderAddComponentButton(ECS::Entity entity) {
    // Button to open the Add Component popup menu
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
        m_focusAddComponentSearch = true; // Focus search when the button opens the popup
    }

    // Button to save this entity and its components as a prefab
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
    if (ImGui::Button("Save as Prefab")) {
        _saveEntityAsPrefab(entity);
    }
    ImGui::PopStyleColor(3);

    // Popup menu listing all available components from the registry
    if (m_openAddComponentPopup) {
        ImGui::OpenPopup("AddComponentMenu");
        m_openAddComponentPopup = false;
    }
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Reset search when popup first appears
        if (ImGui::IsWindowAppearing()) {
            m_addComponentSearchBuffer[0] = '\0';
            m_addComponentSearchFilter.clear();
        }

        // Search input (auto-focused when popup opens)
        if (m_focusAddComponentSearch || ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            m_focusAddComponentSearch = false;
        }
        if (ImGui::InputTextWithHint("##AddCompSearch", "Search...", m_addComponentSearchBuffer, sizeof(m_addComponentSearchBuffer))) {
            m_addComponentSearchFilter = m_addComponentSearchBuffer;
        }

        ImGui::Separator();

        // Get registry and create sorted list
        const auto& registry = ComponentRegistryUI::GetAll();
        
        // Debug: if registry is empty, show message
        if (registry.empty()) {
            ImGui::TextDisabled("No components registered");
            ImGui::EndPopup();
            return;
        }

        std::vector<size_t> sortedIndices;
        for (size_t i = 0; i < registry.size(); ++i) {
            sortedIndices.push_back(i);
        }

        // Sort alphabetically by DisplayName
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
            return registry[a].DisplayName < registry[b].DisplayName;
            });

        // Limit the popup height and make the list scrollable
        // float avail = ImGui::GetContentRegionAvail().y;
        float maxListHeight = 400.f;
        if (maxListHeight < 120.0f) maxListHeight = 120.0f;

        ImGui::BeginChild("AddComponentList", ImVec2(0, maxListHeight), false, ImGuiWindowFlags_None);

        // Helper to lowercase strings for case-insensitive search
        auto toLower = [](const std::string& s) {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
            return out;
        };

        std::string filterLower = toLower(m_addComponentSearchFilter);

        // Iterate over sorted components
        for (size_t idx : sortedIndices) {
            const auto& meta = registry[idx];

            // Apply search filter if present
            if (!filterLower.empty()) {
                std::string nameLower = toLower(meta.DisplayName);
                std::string typeLower = toLower(meta.TypeName);
                if (nameLower.find(filterLower) == std::string::npos && typeLower.find(filterLower) == std::string::npos) {
                    continue;
                }
            }

            // Check if the entity already has this component
            bool hasComponent = meta.HasComponent(m_world, entity);

            // If entity already has the component, disable this menu entry
            if (hasComponent) {
                ImGui::BeginDisabled();
            }

            // Draw the menu item (internal helper attaches the component)
            _renderComponentMenuItem(meta.DisplayName.c_str(), meta.TypeName.c_str());

            // Re-enable UI after drawing a disabled item
            if (hasComponent) {
                ImGui::EndDisabled();
            }
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Prefab Inspector Implementation
// -------------------------------------------------------------------------

// Top level entry for prefab mode
// Draws prefab header, components and prefab specific actions
void InspectorPanel::_renderPrefabInspector() {
    _renderPrefabHeader();
    _renderPrefabComponents();
    _renderPrefabActions();
}

// Show which prefab file we are editing and warn that it affects all instances
void InspectorPanel::_renderPrefabHeader() {
    ImGui::Text("Editing Template");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", std::filesystem::path(m_prefabPath).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextWrapped("Changes to this prefab will auto-save and update ALL instances");
    ImGui::Separator();
}

// Check if the loaded prefab JSON has a hierarchical structure (i.e. contains nested entities)
bool InspectorPanel::_isHierarchicalPrefab() const {
    return m_prefabData.contains("Entity") && m_prefabData["Entity"].is_object();
}

// Get a pointer to the currently selected prefab node in the JSON structure based on m_selectedPrefabNodePath
nlohmann::json* InspectorPanel::_getSelectedPrefabNode() {
	// If this is a hierarchical prefab, we need to traverse the JSON tree based on the selected path
    if (_isHierarchicalPrefab()) {
		// Start at the root "Entity" node
        nlohmann::json* node = &m_prefabData["Entity"];

		// Traverse down the "Children" arrays according to the indices in m_selectedPrefabNodePath
        for (size_t idx : m_selectedPrefabNodePath) {
			// If at any point the expected "Children" array is missing or the index is out of bounds, we 
            // reset the selection to root
            if (!node->contains("Children") || !(*node)["Children"].is_array() || idx >= (*node)["Children"].size()) {
                m_selectedPrefabNodePath.clear();
                return &m_prefabData["Entity"];
            }
			// Move the pointer down to the selected child node
            node = &(*node)["Children"][idx];
        }

		// After traversing the path, node points to the currently selected prefab node, which we return
        return node;
    }

	// If this is not a hierarchical prefab, we just return the root prefab object for editing
    if (m_prefabData.is_object()) {
        return &m_prefabData;
    }
    return nullptr;
}

// Const version of _getSelectedPrefabNode for read-only access
const nlohmann::json* InspectorPanel::_getSelectedPrefabNode() const {
	// Same traversal logic as non-const version, but returns a const pointer for read-only access
    if (_isHierarchicalPrefab()) {
        const nlohmann::json* node = &m_prefabData["Entity"];
        for (size_t idx : m_selectedPrefabNodePath) {
            if (!node->contains("Children") || !(*node)["Children"].is_array() || idx >= (*node)["Children"].size()) {
                return &m_prefabData["Entity"];
            }
            node = &(*node)["Children"][idx];
        }
        return node;
    }

    if (m_prefabData.is_object()) {
        return &m_prefabData;
    }
    return nullptr;
}

// Get a pointer to the "Components" array of the currently selected prefab node, optionally creating it 
// if it doesn't exist
nlohmann::json* InspectorPanel::_getSelectedPrefabComponents(bool createIfMissing) {
	// First get the currently selected node in the prefab JSON structure
    nlohmann::json* node = _getSelectedPrefabNode();
    if (!node || !node->is_object()) {
        return nullptr;
    }

	// Ensure there is a "Components" array we can write to; if not, create an empty one (for new nodes)
    if (!node->contains("Components")) {
        if (!createIfMissing) return nullptr;
        (*node)["Components"] = nlohmann::json::array();
    }
	// If "Components" exists but is not an array, this is a malformed prefab structure; we log an error and reset it
    if (!(*node)["Components"].is_array()) {
        if (!createIfMissing) return nullptr;
        (*node)["Components"] = nlohmann::json::array();
    }
	// Finally return a pointer to the "Components" array of the selected node, which the caller can read from 
    // or write to
    return &(*node)["Components"];
}

// Const version of _getSelectedPrefabComponents for read-only access
const nlohmann::json* InspectorPanel::_getSelectedPrefabComponents() const {
    // SAME THING
    const nlohmann::json* node = _getSelectedPrefabNode();
    if (!node || !node->is_object()) {
        return nullptr;
    }
    if (!node->contains("Components") || !(*node)["Components"].is_array()) {
        return nullptr;
    }
    return &(*node)["Components"];
}

// Helper to extract a display name for a prefab node by looking for a Name component in its Components list
std::string InspectorPanel::_getPrefabNodeDisplayName(const nlohmann::json& node) const {
	// Look for a Name or ECS::Components::Name component in this node's Components array to use as display name
    if (node.contains("Components") && node["Components"].is_array()) {
		// We loop through all components of this prefab node to find a Name component, which we use as the display 
        // name in the UI
        for (const auto& comp : node["Components"]) {
			// Basic validation to make sure this component has the expected structure before we try to read it
            if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;
            if (!comp.contains("Data") || !comp["Data"].is_object()) continue;

			// Check if this component is a Name component (either short or fully-qualified)
            const std::string typeName = comp["TypeName"].get<std::string>();

			// If this is a Name component, we look for a "Value" field inside its "Data" object, which should be the 
            // actual name string
            if (typeName == "Name" || typeName == "ECS::Components::Name") {
				// If the Value field exists and is a string, we return it as the display name for this prefab node
                // If it's empty, we fall back to a default name below
                if (comp["Data"].contains("Value") && comp["Data"]["Value"].is_string()) {
                    const std::string value = comp["Data"]["Value"].get<std::string>();
                    if (!value.empty()) {
                        return value;
                    }
                }
				// If we found a Name component but it doesn't have a valid Value, we stop looking further and return a 
                // default name
                break;
            }
        }
    }
	// If no Name component found, we return a default name based on whether this is the root node or a child node
    return "Entity";
}

// Recursively build a flat list of all prefab nodes in the JSON structure for display in the child selector dropdown
std::vector<InspectorPanel::PrefabNodeSelectionItem> InspectorPanel::_buildPrefabNodeSelectionItems() const {
    std::vector<PrefabNodeSelectionItem> items;

	// If this prefab doesn't have a hierarchical structure, we just return an empty list and the UI will show a 
    // single "Root" node
    if (!_isHierarchicalPrefab()) {
        return items;
    }
    
	// Start with the root node
    const nlohmann::json* root = &m_prefabData["Entity"];
    items.push_back({ {}, "Root", 0 });

	// Recursive lambda to visit each node in the prefab JSON tree and add it to the items list with its path and depth for indentation
    std::function<void(const nlohmann::json&, const std::vector<size_t>&, const std::string&, int)> visit;
    visit = [&](const nlohmann::json& node, const std::vector<size_t>& path, const std::string& pathLabel, int depth) {
        if (!node.contains("Children") || !node["Children"].is_array()) {
            return;
        }

		// Loop through each child of this node
        for (size_t i = 0; i < node["Children"].size(); i++) {
			// Basic validation to ensure this child is an object before we try to read it
            const auto& child = node["Children"][i];
            if (!child.is_object()) continue;

			// Build the path to this child node by appending the current index to the parent's path
            std::vector<size_t> childPath = path;
            childPath.push_back(i);

			// Get a display name for this child node (looking for a Name component or defaulting to "Entity")
            const std::string childName = _getPrefabNodeDisplayName(child);
            const std::string childPathLabel = pathLabel + "/" + childName;
            items.push_back({ childPath, childPathLabel, depth + 1 });

			// Recursively visit this child's children to build their paths and labels as well
            visit(child, childPath, childPathLabel, depth + 1);
        }
    };

	// Start the recursive visitation with the root node, an empty path, "Root" label, and depth 0
    visit(*root, {}, "Root", 0);

	// After this function runs, we have a flat list of all prefab nodes with their corresponding paths and display names, 
    // which we can use to populate the child selector dropdown in the UI
    return items;
}

// Render all component definitions stored inside the prefab JSON
void InspectorPanel::_renderPrefabComponents() {
	// First we get a pointer to the currently selected prefab node's "Components" array in the JSON structure
    if (_isHierarchicalPrefab()) {
		// For hierarchical prefabs, we render a child selector dropdown to allow selecting which node's components to edit
        const std::vector<PrefabNodeSelectionItem> nodeItems = _buildPrefabNodeSelectionItems();
		// We look through the list of nodes to find the currently selected one so we can show its name in the UI
        std::string currentLabel = "Root";
        for (const auto& item : nodeItems) {
            if (item.Path == m_selectedPrefabNodePath) {
                currentLabel = item.Label;
                break;
            }
        }

		// Button to open the child selector popup
        if (ImGui::Button("Select Child")) {
            ImGui::OpenPopup("PrefabChildSelector");
        }

		// Popup menu to select which child node's components to edit; shows a hierarchical list of all nodes in the prefab with 
        // indentation
        if (ImGui::BeginPopup("PrefabChildSelector")) {
			// Iterate through all prefab nodes and show them in the popup with indentation based on their depth in the hierarchy
            for (size_t itemIndex = 0; itemIndex < nodeItems.size(); itemIndex++) {
                const auto& item = nodeItems[itemIndex];
                const bool isSelected = (item.Path == m_selectedPrefabNodePath);
                const std::string indent(static_cast<size_t>(std::max(item.Depth - 1, 0) * 2), ' ');
                const std::string popupLabel = indent + item.Label;

				// We use PushID with the item index to ensure unique IDs for each selectable, since labels can be duplicated in a hierarchy
                ImGui::PushID(static_cast<int>(itemIndex));

				// Each item is a selectable entry; when clicked, we update m_selectedPrefabNodePath to point to the selected node's path 
                // in the JSON structure
                if (ImGui::Selectable(popupLabel.c_str(), isSelected)) {
                    m_selectedPrefabNodePath = item.Path;
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

		// If we are not at the root node, show a "Back to Root" button that clears the selection path and goes back to editing the root 
        // prefab node
        const bool disableBackToRoot = m_selectedPrefabNodePath.empty();
        if (disableBackToRoot) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Back to Root")) {
            m_selectedPrefabNodePath.clear();
        }
        if (disableBackToRoot) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Editing: %s", currentLabel.c_str());
        ImGui::Dummy(ImVec2(0, 4));
    }

    // Footer needs 2 lines: one for buttons, one for status message
    float childHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2;

    // Component/property filter input for prefab inspector list
    if (m_focusComponentFilter) {
        ImGui::SetKeyboardFocusHere();
        m_focusComponentFilter = false;
    }

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::InputTextWithHint("##ComponentFilterPrefab", "Filter components/fields...", m_componentFilterBuffer, sizeof(m_componentFilterBuffer))) {
        m_componentFilter = m_componentFilterBuffer;
    }

    // Use smaller padding inside the scrolling region for components
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 8));
    ImGui::BeginChild("PrefabComponents", ImVec2(0, childHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

	// Get the Components array for the currently selected prefab node; if it doesn't exist, we won't draw anything
    nlohmann::json* componentsPtr = _getSelectedPrefabComponents(false);

    // Prefab JSON must have a Components array or there is nothing to draw
    if (!componentsPtr) {
        ImGui::TextDisabled("No components in selected prefab node");
        ImGui::EndChild();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::Dummy(ImVec2(0, 4));
    auto& components = *componentsPtr;

    // Apply property filter so per-field rows can be narrowed
    if (m_componentFilter.empty()) {
        EditorUI::ClearPropertyFilter();
    } else {
        EditorUI::SetPropertyFilter(m_componentFilter);
    }

    // SORT COMPONENTS: Transform first, then alphabetical by TypeName
    // Create a sorted list of indices so we don't modify the actual JSON array order
    std::vector<size_t> sortedIndices;
    for (size_t i = 0; i < components.size(); i++) {
        sortedIndices.push_back(i);
    }

    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
        // Get type names for comparison
        std::string typeA = components[a].value("TypeName", "");
        std::string typeB = components[b].value("TypeName", "");

        // Helper to identify Name
        auto isName = [](const std::string& type) {
            return (type == "ECS::Components::Name" || type == "Name");
        };

        // Helper to identify Transform
        auto isTransform = [](const std::string& type) {
            return (type == "ECS::Components::LocalTransform" || type == "LocalTransform");
        };

        bool aIsName = isName(typeA);
        bool bIsName = isName(typeB);
        bool aIsTransform = isTransform(typeA);
        bool bIsTransform = isTransform(typeB);

        // Transform always first
        if (aIsTransform && !bIsTransform) return true;
        if (!aIsTransform && bIsTransform) return false;
        if (aIsTransform && bIsTransform) return false;

        // Name always second
        if (aIsName && !bIsName) return true;
        if (!aIsName && bIsName) return false;
        if (aIsName && bIsName) return false; 

        // Strip "ECS::Components::" prefix for cleaner alphabetical sorting
        auto stripPrefix = [](const std::string& name) -> std::string {
            const std::string prefix = "ECS::Components::";
            if (name.find(prefix) == 0) {
                return name.substr(prefix.length());
            }
            return name;
        };

        // Everything else alphabetical
        return stripPrefix(typeA) < stripPrefix(typeB);
    });

    // Draw each component in sorted order using metadata rules
    std::string filterLower;
    if (!m_componentFilter.empty()) {
        filterLower = m_componentFilter;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }
    for (size_t idx : sortedIndices) {
        auto& componentEntry = components[idx];

        // Validate each JSON entry
        if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
        if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

        std::string typeName = componentEntry["TypeName"];
        const auto* meta = ComponentRegistryUI::Find(typeName);
        if (!meta) continue;

        // Component name filter to reduce noise in long inspectors
        if (!filterLower.empty()) {
            std::string nameLower = meta->DisplayName;
            std::string typeLower = meta->TypeName;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (nameLower.find(filterLower) == std::string::npos && typeLower.find(filterLower) == std::string::npos) {
                continue;
            }
        }

        // Render a collapsible UI section for this component
        // After any edit we call _savePrefabData so the file updates immediately
        auto& data = componentEntry["Data"];
        const nlohmann::json defaults = meta->GetDefaults(); // Defaults used for reset + per-field reset
        _renderComponentSection(meta->DisplayName, meta->TypeName, data,
            // UI renderer callback: InspectorPanel forwards JSON to ComponentWidgets
            // Prefabs do not have live ECS so we save as soon as data changes
            [this, meta](nlohmann::json& d) { meta->RenderUI(m_componentUI, d, ECS::Entity{}, nullptr); _savePrefabData(); },
            meta->CanDelete,
            &defaults
        );
        ImGui::Dummy(ImVec2(0, 4));
    }

    // Clear the property filter after rendering component rows
    EditorUI::ClearPropertyFilter();

    // Process deferred deletions
    for (const auto& componentType : m_componentsToDelete) {
        _removeComponentFromPrefab(componentType);
    }
    m_componentsToDelete.clear();

    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Render any drag/drop validation feedback triggered by prefab fields
    m_componentUI.RenderAssetDropFeedbackPopup();
}

// Renders the action row shown when editing a prefab
void InspectorPanel::_renderPrefabActions() {
    ImGui::Separator();

    // Button to open the Add Component popup for prefabs
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentMenu");
        m_focusAddComponentSearch = true; // Focus search when the button opens the popup
    }

    // Popup list of components that can be added to the prefab
    if (m_openAddComponentPopup) {
        ImGui::OpenPopup("AddComponentMenu");
        m_openAddComponentPopup = false;
    }
    if (ImGui::BeginPopup("AddComponentMenu")) {
        ImGui::PushFont(m_boldFont);
        ImGui::Text("Components");
        ImGui::PopFont();
        ImGui::Separator();

        // Reset search when popup first appears
        if (ImGui::IsWindowAppearing()) {
            m_addComponentSearchBuffer[0] = '\0';
            m_addComponentSearchFilter.clear();
        }

        // Search input (auto-focused when popup opens)
        if (m_focusAddComponentSearch || ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            m_focusAddComponentSearch = false;
        }
        if (ImGui::InputTextWithHint("##AddCompSearchPrefab", "Search...", m_addComponentSearchBuffer, sizeof(m_addComponentSearchBuffer))) {
            m_addComponentSearchFilter = m_addComponentSearchBuffer;
        }

        ImGui::Separator();

        // Get registry and create sorted list
        const auto& registry = ComponentRegistryUI::GetAll();
        std::vector<size_t> sortedIndices;
        for (size_t i = 0; i < registry.size(); ++i) {
            sortedIndices.push_back(i);
        }

        // Sort alphabetically by DisplayName
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
            return registry[a].DisplayName < registry[b].DisplayName;
            });

        // Limit popup height and make the list scrollable

        // float avail = ImGui::GetContentRegionAvail().y;
        float maxListHeight = 400.0f;
        if (maxListHeight < 120.0f) maxListHeight = 120.0f;
        ImGui::BeginChild("AddComponentListPrefab", ImVec2(0, maxListHeight), false, ImGuiWindowFlags_None);

        // Helper to lowercase strings for case-insensitive search
        auto toLower = [](const std::string& s) {
            std::string out = s;
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
            return out;
        };

        std::string filterLower = toLower(m_addComponentSearchFilter);

        // Iterate over sorted components
        for (size_t idx : sortedIndices) {
            const auto& meta = registry[idx];

            // Apply search filter if present
            if (!filterLower.empty()) {
                std::string nameLower = toLower(meta.DisplayName);
                std::string typeLower = toLower(meta.TypeName);
                if (nameLower.find(filterLower) == std::string::npos && typeLower.find(filterLower) == std::string::npos) {
                    continue;
                }
            }

            // Check if the prefab already defines this component
            bool hasComponent = _prefabHasComponent(meta.TypeName);

            // Disable menu entries for components that already exist in prefab
            if (hasComponent) {
                ImGui::BeginDisabled();
            }

            // Draw the menu item and handle adding the component when clicked
            _renderComponentMenuItem(meta.DisplayName.c_str(), meta.TypeName.c_str());

            if (hasComponent) {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // This pushes all current prefab data onto every instance in the scene
    if (ImGui::Button("Apply to All Instances")) {
        _applyPrefabToInstances();
    }
}

// -------------------------------------------------------------------------
// Component Menu Management
// -------------------------------------------------------------------------

// Draws a single menu item inside the Add Component popup
void InspectorPanel::_renderComponentMenuItem(const char* displayName, const char* componentType) {
    if (ImGui::MenuItem(displayName)) {

        bool added = false;
        // If we are editing a live entity
        if (m_mode == InspectionMode::Entity) {
            added = _addComponentToEntity(componentType);
        }
        // If we are editing a prefab template
        else if (m_mode == InspectionMode::Prefab) {
            added = _addComponentToPrefab(componentType);
        }

        // Close popup if a valid component was added
        if (added) {
            ImGui::CloseCurrentPopup();
        }
    }
}

// -------------------------------------------------------------------------
// Entity Component Management
// -------------------------------------------------------------------------

// Attach a new component to the currently selected entity
// We use metadata to create default JSON for the component and then apply it to the ECS
bool InspectorPanel::_addComponentToEntity(const std::string& componentType) {
    if (!m_world) return false;
    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (!m_world->IsAlive(entity)) return false;

    std::vector<ECS::SerializedComponent> beforeSnapshot;
    if (m_undoSystem) {
        // Capture full entity state so add/remove can be undone as a single step.
        beforeSnapshot = m_world->CaptureEntityComponents(entity);
    }

    // Shape components are mutually exclusive
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        _removeComponentFromEntity("ShapeCircle2D", false);
        _removeComponentFromEntity("ShapeBox2D", false);
        _removeComponentFromEntity("ShapeLine2D", false);
    }

    // Look up this component's metadata so we know how to create it
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (meta) {
        // Add to ECS with default JSON values
        meta->AddComponent(m_world, entity, meta->GetDefaults());
        m_statusMessage = std::string("Added ") + componentType;
        m_statusTimer = 3.0f;

        // DEBUG: dump all known native components and whether this entity has them
        {
            auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
            for (auto aid : allIds) {
                const auto& ameta = ECS::ComponentRegistry::Meta(aid);
                std::string aname = ECS::ComponentRegistry::GetComponentNameFromHash(ameta.TypeHash);
                if (aname.empty()) {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "Component_0x%08x", ameta.TypeHash);
                    aname = buffer;
                }
                bool has = m_world->HasById(entity, aid);
                LOG_INFO("[DebugComponents] Entity " << entity.Index << " HasById id=" << aid << " name=" << aname << " -> " << (has ? "YES" : "NO"));
                if (has) {
                    void* ptr = m_world->GetRawComponentPtr(entity, aid);
                    LOG_INFO("[DebugComponents]    ptr=" << ptr);
                }
            }
        }

        // MARK SCENE AS DIRTY
        MarkSceneDirtyIfNeeded(m_fileMenu);

        if (m_undoSystem) {
            // Record a snapshot diff for undo/redo of the add operation.
            auto afterSnapshot = m_world->CaptureEntityComponents(entity);
            if (!SnapshotsEqual(beforeSnapshot, afterSnapshot)) {
                auto command = std::make_unique<Editor::EntityComponentsSnapshotCommand>(
                    m_world, entity, std::move(beforeSnapshot), std::move(afterSnapshot)
                );
                m_undoSystem->ExecuteCommand(std::move(command));
            }
        }

        return true;
    }
    return false;
}

// Remove a component from the entity unless it is the Transform which must always exist
void InspectorPanel::_removeComponentFromEntity(const std::string& componentType, bool recordUndo) {
    if (!m_world) return;
    ECS::Entity entity = m_world->Resolve(m_entityId);
    if (!m_world->IsAlive(entity)) return;

    // Transform and Layer cannot be removed
    if (componentType == "LocalTransform" || componentType == "Layer") return;

    std::vector<ECS::SerializedComponent> beforeSnapshot;
    if (recordUndo && m_undoSystem) {
        // Capture full entity state so removal can be undone cleanly.
        beforeSnapshot = m_world->CaptureEntityComponents(entity);
    }

    // // Look up this component's metadata
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (meta) {
        // Remove from ECS
        meta->RemoveComponent(m_world, entity);
        m_statusMessage = std::string("Removed ") + componentType;
        m_statusTimer = 3.0f;

        // MARK SCENE AS DIRTY
        MarkSceneDirtyIfNeeded(m_fileMenu);

        if (recordUndo && m_undoSystem) {
            // Record a snapshot diff for undo/redo of the removal.
            auto afterSnapshot = m_world->CaptureEntityComponents(entity);
            if (!SnapshotsEqual(beforeSnapshot, afterSnapshot)) {
                auto command = std::make_unique<Editor::EntityComponentsSnapshotCommand>(
                    m_world, entity, std::move(beforeSnapshot), std::move(afterSnapshot)
                );
                m_undoSystem->ExecuteCommand(std::move(command));
            }
        }
    }
}

// Check whether an entity has a component using metadata rules
bool InspectorPanel::_entityHasComponent(EntityId id, const std::string& componentType) {
    ECS::Entity entity = m_world->Resolve(id);
    if (!m_world->IsAlive(entity)) return false;

    // Self-explanatory
    const auto* meta = ComponentRegistryUI::Find(componentType);
    return meta ? meta->HasComponent(m_world, entity) : false;
}

// -------------------------------------------------------------------------
// Prefab Component Management
// -------------------------------------------------------------------------

// Add a component entry to the prefab JSON
// Prefabs are stored and edited entirely through JSON so we modify the data directly
bool InspectorPanel::_addComponentToPrefab(const std::string& componentType) {
	// First we get a pointer to the currently selected prefab node's Components array in the JSON structure, 
    // creating it if it doesn't exist
    nlohmann::json* componentsPtr = _getSelectedPrefabComponents(true);
    if (!componentsPtr) return false;

    // No duplicates allowed
    if (_prefabHasComponent(componentType)) return false;

    // Shape components are mutually exclusive
    if (componentType == "ShapeCircle2D" || componentType == "ShapeBox2D" || componentType == "ShapeLine2D") {
        _removeComponentFromPrefab("ShapeCircle2D");
        _removeComponentFromPrefab("ShapeBox2D");
        _removeComponentFromPrefab("ShapeLine2D");
    }

    // Look up metadata for defaults and full type name
    const auto* meta = ComponentRegistryUI::Find(componentType);
    if (!meta) return false;

    // Append new component entry into prefab JSON
    componentsPtr->push_back({ {"TypeName", meta->FullTypeName}, {"Data", meta->GetDefaults()} });

    // Prefab data changed so we sync the file right away
    _savePrefabData();
    m_statusMessage = std::string("Added ") + componentType;
    m_statusTimer = 2.0f;
    return true;
}

// Removes a component entry from the prefab JSON
// Prefabs store components as JSON objects so we search the Components array by TypeName
// Some entries store the short name while others store the fully qualified ECS type so we check for both
void InspectorPanel::_removeComponentFromPrefab(const std::string& componentType) {
	// First we get a pointer to the currently selected prefab node's Components array in the JSON structure; 
    // if it doesn't exist, there is nothing to remove
    nlohmann::json* componentsPtr = _getSelectedPrefabComponents(false);
    if (!componentsPtr) return;
    auto& components = *componentsPtr;

    // Iterate over each component entry to find a matching TypeName
    for (auto it = components.begin(); it != components.end(); it++) {
        // Validate entry
        if (!(*it).contains("TypeName") || !(*it)["TypeName"].is_string()) continue;

        // Match short name or fully qualified name
        std::string typeName = (*it)["TypeName"];
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) {
            // Remove the component from the prefab JSON
            components.erase(it);
            m_statusMessage = std::string("Removed ") + componentType;
            m_statusTimer = 2.0f;
            // Prefab changed so we save immediately
            _savePrefabData();
            return;
        }
    }
}

// Checks whether the prefab JSON already contains a component of this type
bool InspectorPanel::_prefabHasComponent(const std::string& componentType) {
	// First we get a pointer to the currently selected prefab node's Components array in the JSON structure; 
    // if it doesn't exist, there are no components
    const nlohmann::json* componentsPtr = _getSelectedPrefabComponents();
    if (!componentsPtr) return false;

    // Search each component entry
    for (const auto& comp : *componentsPtr) {
        // Validate type name
        if (!comp.contains("TypeName") || !comp["TypeName"].is_string()) continue;
        std::string typeName = comp["TypeName"];
        // Match short name or fully qualified name
        if (typeName == componentType || typeName == "ECS::Components::" + componentType) return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// Prefab Data Management
// -------------------------------------------------------------------------

// Saves the current prefab JSON to disk
// We hash the JSON before saving so we avoid rewriting the file when nothing changed
void InspectorPanel::_savePrefabData() {
    // Must have a valid prefab path to write to
    if (m_prefabPath.empty()) return;

    // Turn the entire prefab JSON into a string and hash it
    size_t currentHash = std::hash<std::string>{}(m_prefabData.dump());

    // If the hash matches our previous saved version the file is already up to date
    if (currentHash == m_lastSavedPrefabHash) return;

    // Try writing the prefab JSON to the file
    std::ofstream file(m_prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot write to prefab file";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot write to prefab file: " << m_prefabPath);
        return;
    }

    // Pretty print with indent of 4
    file << m_prefabData.dump(4);
    file.close();

    // Update the stored hash so we know this version is saved
    m_lastSavedPrefabHash = currentHash;
}

// Saves a live entity as a new prefab file
// We convert the entity into JSON and write only the Components array
void InspectorPanel::_saveEntityAsPrefab(ECS::Entity entity) {
    if (!m_world || !m_world->IsAlive(entity)) return;

    // Pick a base name for the prefab file
    std::string entityName = "Entity";
    if (const auto* nameComp = Editor::ECSUtils::GetNamePtr(m_world, entity)) {
        std::string resolved = ECS::StringTable::Resolve(nameComp->Value);
        if (!resolved.empty()) {
            entityName = resolved;
        }

        // Replace characters that are illegal or unsafe in file paths
        std::replace_if(entityName.begin(), entityName.end(),
            [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');
    }

    // Ensure prefab directory exists under the active project assets
    std::filesystem::path prefabDir = std::filesystem::path(Engine::ProjectPaths::GetAssetsPath()) / "Prefabs";
    std::filesystem::create_directories(prefabDir);

    // Pick a file name that does not overwrite an existing prefab
    std::filesystem::path prefabPath = prefabDir / (entityName + ".prefab");
    int suffix = 1;

    // If file already exists: Name.prefab, Name_1.prefab, Name_2.prefab, so on and so forth
    // If file doesn't exist: Name.prefab
    while (std::filesystem::exists(prefabPath)) {
        prefabPath = prefabDir / (entityName + "_" + std::to_string(suffix) + ".prefab");
        suffix++;
    }

    // Convert entity to JSON and extract just the Components list
    nlohmann::json entityJson = Serialization::EntitySerializer::SerializeEntityHierarchy(*m_world, entity);

    // Force root transform to identity (Position = 0, Rotation = 0) but keep Scale
    // This ensures that the prefab definition itself doesn't carry the instance's world position
    if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
		// Find the LocalTransform component
        for (auto& comp : entityJson["Components"]) {
			// Validate component entry
            if (!comp.contains("TypeName") || !comp.contains("Data")) continue;
            std::string typeName = comp["TypeName"];

			// Match LocalTransform by short or full name
            if (typeName == "LocalTransform" || typeName == "ECS::Components::LocalTransform") {
                // Reset Position to (0,0,0)
                if (comp["Data"].contains("Position")) {
                    comp["Data"]["Position"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f} };
                }
                // Reset Rotation to Identity (0,0,0,1)
                if (comp["Data"].contains("Rotation")) {
                    comp["Data"]["Rotation"] = { {"X", 0.0f}, {"Y", 0.0f}, {"Z", 0.0f}, {"W", 1.0f} };
                }
                break;
            }
        }
    }

    nlohmann::json prefabData;
    // If entity has children, save the full hierarchy in new format
    if (entityJson.contains("Children")) {
        prefabData["Entity"] = entityJson;
    }
    // No children, use old format
    else {
        prefabData["Components"] = entityJson["Components"];
    }

    // Write the prefab file
    std::ofstream file(prefabPath);
    if (!file.is_open()) {
        m_statusMessage = "Failed: Cannot create prefab file";
        m_statusTimer = 3.0f;
        LOG_ERROR("Cannot create prefab file: " << prefabPath);
        return;
    }

    // Same same
    file << prefabData.dump(4);
    file.close();

    m_statusMessage = "Saved as " + prefabPath.filename().string() + " in Assets\\Prefabs";
    m_statusTimer = 3.0f;
    LOG_INFO("Entity saved as prefab: " << prefabPath);
}

// Applies the current prefab JSON to every entity that is linked to this prefab
void InspectorPanel::_applyPrefabToInstances() {
    if (!m_world) return;

    // Make sure the prefab file on disk is up to date
    _savePrefabData();

	// Get the root node of the prefab JSON which contains the actual component definitions we need to apply
    const nlohmann::json* prefabRootNode = GetPrefabRootNode(m_prefabData);
    if (!prefabRootNode) {
        m_statusMessage = "Failed: Prefab has no valid root data";
        m_statusTimer = 2.0f;
        return;
    }

    // Compute hashes for robust matching
    std::vector<uint32_t> targetHashes;
    
    // 1. Absolute path hash
    uint32_t absHash = ECS::PrefabManager::ComputeHash(
        ECS::PrefabManager::NormalizePath(m_prefabPath)
    );
    targetHashes.push_back(absHash);

    // 2. Relative path hash
    std::string relativePath = Engine::ProjectPaths::ToRelativePath(m_prefabPath);
    if (!relativePath.empty()) {
        uint32_t relHash = ECS::PrefabManager::ComputeHash(
            ECS::PrefabManager::NormalizePath(relativePath)
        );
        // Only add if different to avoid duplicates
        if (relHash != absHash) {
            targetHashes.push_back(relHash);
        }
    }

    // Iterate over every entity that has a PrefabInstanceMetadata component
    int count = 0;
    m_world->Each<ECS::Components::PrefabInstanceMetadata>([&](ECS::Entity entity, ECS::Components::PrefabInstanceMetadata& meta) {
        // Check if the entity's hash matches any of our target hashes
        if (std::find(targetHashes.begin(), targetHashes.end(), meta.PrefabHash) != targetHashes.end()) {
			// This entity is an instance of our prefab, so we apply the prefab data to it
            _applyPrefabHierarchyToEntity(entity, *prefabRootNode, true);
            count++;
        }
    });

    std::string activeScenePath;
    if (Engine::CORE) {
        auto* activeScene = Engine::CORE->GetSceneManager().GetActive();
        if (activeScene) {
            activeScenePath = ECS::PrefabManager::NormalizePath(activeScene->GetPath());
        }
    }
    
    // Scan all scene files
    int fileCount = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(Engine::ProjectPaths::GetProjectRoot())) {
        if (!entry.is_regular_file()) continue;
        
        auto ext = entry.path().extension().string();
        if (ext != ".scn" && ext != ".scene") continue;
        
        std::string scenePath = entry.path().string();
        std::string normalizedScenePath = ECS::PrefabManager::NormalizePath(scenePath);
        
        // Skip active scene to avoid conflict
        if (!activeScenePath.empty() && normalizedScenePath == activeScenePath) continue;
        
        UpdatePrefabInSceneFile(scenePath, m_prefabData, targetHashes);
        fileCount++;
    }

    LOG_INFO("Global Prefab Sync complete. Patched instances in " << fileCount << " external scene files.");

    m_statusMessage = "Applied to " + std::to_string(count) + " loaded instance(s) and scanned " + std::to_string(fileCount) + " files";
    m_statusTimer = 2.0f;
}

// Applies all component data from one prefab node to one entity instance
void InspectorPanel::_applyPrefabDataToEntity(ECS::Entity entity, const nlohmann::json& prefabNode, bool preserveRootTransform) {
	// Get the Components array for this prefab node; if it doesn't exist, there is nothing to apply
    const nlohmann::json* componentsPtr = GetPrefabNodeComponents(prefabNode);

    // Prefab must have a valid Components array
    if (!componentsPtr) return;

    // For each component in the prefab assign its JSON back into the ECS entity
    for (const auto& componentEntry : *componentsPtr) {
        // Basic validation
        if (!componentEntry.contains("TypeName") || !componentEntry["TypeName"].is_string()) continue;
        if (!componentEntry.contains("Data") || !componentEntry["Data"].is_object()) continue;

        std::string typeName = componentEntry["TypeName"];
        const auto* meta = ComponentRegistryUI::Find(typeName);

        // Use metadata to load the JSON into the live ECS component
        if (meta) {
            // For LocalTransform on the root entity, only apply Scale from prefab
            if (preserveRootTransform && (typeName == "LocalTransform" || typeName == "ECS::Components::LocalTransform")) {
                // 1. Capture current instance values
                ECS::Components::LocalTransform backupPosRot;
                bool hasTransform = false;
                {
					// Get current transform from entity
                    auto* t = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(m_world, entity, "LocalTransform");
                    if (t) {
						// Backup Position and Rotation
                        backupPosRot = *t;
                        hasTransform = true;
                    }
                }

                // 2. Apply prefab data (this overwrites everything with prefab values)
                meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);

                // 3. Restore Position and Rotation from the instance, keeping the new Scale
                if (hasTransform) {
                    auto* t = Editor::ECSUtils::GetComponentPtr<ECS::Components::LocalTransform>(m_world, entity, "LocalTransform");
					// Restore Position and Rotation
                    if (t) {
                        t->Position = backupPosRot.Position;
                        t->Rotation = backupPosRot.Rotation;
                    }
                }
            }
			// For all other components, apply normally
            else {
                meta->ApplyToEntity(m_world, entity, componentEntry["Data"]);
            }
        }
    }
}

// Recursively applies prefab data down the entity hierarchy
void InspectorPanel::_applyPrefabHierarchyToEntity(ECS::Entity entity, const nlohmann::json& prefabNode, bool preserveRootTransform) {
	// Validate entity before applying data
    if (!m_world || entity.IsNull() || !m_world->IsAlive(entity)) {
        return;
    }

	// First apply data to the current entity, then we will recurse down to children
    _applyPrefabDataToEntity(entity, prefabNode, preserveRootTransform);
    if (!prefabNode.contains("Children") || !prefabNode["Children"].is_array()) {
        return;
    }

	// Gather children of this entity into a vector so we can index them in the same order as the prefab JSON array
    std::vector<ECS::Entity> entityChildren;
    m_world->ForChildren(entity, [&](ECS::Entity child) {
        if (!child.IsNull() && m_world->IsAlive(child)) {
            entityChildren.push_back(child);
        }
    });

	// If the prefab has more children than the entity, we can only apply to the overlapping ones
    const auto& prefabChildren = prefabNode["Children"];
    const size_t applyCount = std::min(prefabChildren.size(), entityChildren.size());

	// Warn if there is a mismatch in child count, but still apply to the overlapping children
    if (prefabChildren.size() != entityChildren.size()) {
        LOG_WARNING("Prefab/entity child count mismatch at entity " << entity.Index
            << " (prefab = " << prefabChildren.size() << ", entity = " << entityChildren.size()
            << "); applying overlapping children only");
    }

	// Recursively apply prefab data to each child entity using the corresponding child prefab node
    for (size_t i = 0; i < applyCount; i++) {
		// Validate prefab child node before recursing
        const auto& childPrefabNode = prefabChildren[i];
        if (!childPrefabNode.is_object()) continue;

		// Recurse down to child entity with corresponding prefab child node
        _applyPrefabHierarchyToEntity(entityChildren[i], childPrefabNode, false);
    }
}

// -------------------------------------------------------------------------
// Status Management
// -------------------------------------------------------------------------

// Renders the status message bar at the bottom of the inspector
// Shows short success or error messages that fade out over time
void InspectorPanel::_renderStatusBar() {
    if (m_statusTimer > 0.0f) {
        // Pick color based on whether the message contains "Failed"
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? EditorStyle::DangerText
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
        ImGui::Separator();
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());

        // Countdown so the message disappears naturally
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }
}
