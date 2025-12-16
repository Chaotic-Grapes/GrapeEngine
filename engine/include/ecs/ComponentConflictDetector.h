/* Start Header *****************************************************************/
/*!
\file    ComponentConflictDetector.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Unified component conflict detection for dependency graphs. Extracted from
both ecs/SystemDependencyGraph and jobs/SystemDependencyGraph implementations
to eliminate code duplication.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_COMPONENT_CONFLICT_DETECTOR_H
#define ECS_COMPONENT_CONFLICT_DETECTOR_H

#include "ecs/ISystem.h"
#include <vector>
#include <string>

namespace ECS {

    /**
     * @class ComponentConflictDetector
     * @brief Provides unified conflict detection for component access patterns.
     * 
     * This class handles the common logic for detecting conflicts between
     * component access patterns used by both SystemDependencyGraph and
     * jobs/SystemDependencyGraph implementations.
     * 
     * Supports both:
     * - Legacy mode: Simple Read/Write lists (backward compatibility)
     * - New mode: ComponentAccess structures with modes (Read/Write/ReadWrite)
     */
    class ComponentConflictDetector {
    public:
        /**
         * @brief Detects if two systems have component access conflicts.
         * @param metaA Metadata for system A
         * @param metaB Metadata for system B
         * @return true if there are conflicts, false if can run in parallel
         * 
         * Conflicts occur when:
         * - Both write to same component (write-write)
         * - One writes, other reads same component (write-read)
         * - One reads, other writes same component (read-write)
         * 
         * Read-read access is safe and does NOT constitute a conflict.
         * 
         * This method automatically handles both legacy Read/Write lists
         * and new ComponentAccess vectors.
         */
        static bool HasConflict(
            const SystemMetadata& metaA,
            const SystemMetadata& metaB
        );

        /**
         * @brief Detects if component access patterns conflict.
         * @param writeComps1 Write components for entity A
         * @param writeComps2 Write components for entity B
         * @param readComps1 Read components for entity A
         * @param readComps2 Read components for entity B
         * @return true if there are conflicts, false if can run in parallel
         * 
         * Generic version working with component ID vectors (legacy mode).
         */
        static bool HasConflict(
            const std::vector<ComponentTypeId>& writeComps1,
            const std::vector<ComponentTypeId>& writeComps2,
            const std::vector<ComponentTypeId>& readComps1,
            const std::vector<ComponentTypeId>& readComps2
        );

        /**
         * @brief Provides detailed description of conflicts between two systems.
         * @param metaA Metadata for system A
         * @param metaB Metadata for system B
         * @return Vector of conflict descriptions (empty if no conflicts)
         */
        static std::vector<std::string> GetConflictDetails(
            const SystemMetadata& metaA,
            const SystemMetadata& metaB
        );

    private:
        /**
         * @brief Internal helper to check write-write conflicts.
         */
        static bool _hasWriteWriteConflict(
            const std::vector<ComponentTypeId>& writeComps1,
            const std::vector<ComponentTypeId>& writeComps2
        );

        /**
         * @brief Internal helper to check write-read conflicts.
         */
        static bool _hasWriteReadConflict(
            const std::vector<ComponentTypeId>& writeComps1,
            const std::vector<ComponentTypeId>& readComps2,
            const std::vector<ComponentTypeId>& writeComps2,
            const std::vector<ComponentTypeId>& readComps1
        );

        /**
         * @brief Check conflicts using ComponentAccess structures with modes.
         * @param accessesA Access patterns for system A
         * @param accessesB Access patterns for system B
         * @return true if there are conflicts, false otherwise
         * 
         * More sophisticated conflict detection using access modes.
         */
        static bool _hasAccessConflict(
            const std::vector<ComponentAccess>& accessesA,
            const std::vector<ComponentAccess>& accessesB
        );
    };

}

#endif
