/* Start Header *****************************************************************/
/*!
\file    ComponentConflictDetector.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of unified component conflict detection extracted from both
SystemDependencyGraph implementations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/ComponentConflictDetector.h"
#include <sstream>
#include <iomanip>

namespace ECS {

    bool ComponentConflictDetector::HasConflict(
        const SystemMetadata& metaA,
        const SystemMetadata& metaB)
    {
        // Use the new unified ComponentAccesses API
        const auto& accessesA = metaA.GetComponentAccesses();
        const auto& accessesB = metaB.GetComponentAccesses();
        
        if (!accessesA.empty() && !accessesB.empty()) {
            // Both have accesses in new format - use mode-aware conflict detection
            return _hasAccessConflict(accessesA, accessesB);
        }
        
        // Fall back to simple Read/Write conflict detection for backward compatibility
        return HasConflict(
            metaA.GetWriteComponents(),
            metaB.GetWriteComponents(),
            metaA.GetReadComponents(),
            metaB.GetReadComponents()
        );
    }

    bool ComponentConflictDetector::HasConflict(
        const std::vector<ComponentTypeId>& writeComps1,
        const std::vector<ComponentTypeId>& writeComps2,
        const std::vector<ComponentTypeId>& readComps1,
        const std::vector<ComponentTypeId>& readComps2)
    {
        // Check write-write conflicts
        if (_hasWriteWriteConflict(writeComps1, writeComps2)) {
            return true;
        }

        // Check write-read conflicts (bidirectional)
        if (_hasWriteReadConflict(writeComps1, readComps2, writeComps2, readComps1)) {
            return true;
        }

        // Read-read is safe
        return false;
    }

    std::vector<std::string> ComponentConflictDetector::GetConflictDetails(
        const SystemMetadata& metaA,
        const SystemMetadata& metaB)
    {
        std::vector<std::string> conflicts;

        // Get component lists from metadata
        auto writeA = metaA.GetWriteComponents();
        auto writeB = metaB.GetWriteComponents();
        auto readA = metaA.GetReadComponents();
        auto readB = metaB.GetReadComponents();

        // Check write-write conflicts
        for (const auto& compA : writeA) {
            for (const auto& compB : writeB) {
                if (compA == compB) {
                    std::stringstream ss;
                    ss << "Both " << metaA.GetName() << " and " << metaB.GetName()
                       << " write to component 0x" << std::hex << std::setfill('0')
                       << std::setw(8) << compA;
                    conflicts.push_back(ss.str());
                }
            }
        }

        // Check write-read conflicts
        for (const auto& compA : writeA) {
            for (const auto& compB : readB) {
                if (compA == compB) {
                    std::stringstream ss;
                    ss << metaA.GetName() << " writes to component (0x" << std::hex
                       << std::setfill('0') << std::setw(8) << compA
                       << ") that " << metaB.GetName() << " reads";
                    conflicts.push_back(ss.str());
                }
            }
        }

        // Check read-write conflicts
        for (const auto& compA : readA) {
            for (const auto& compB : writeB) {
                if (compA == compB) {
                    std::stringstream ss;
                    ss << metaA.GetName() << " reads component (0x" << std::hex
                       << std::setfill('0') << std::setw(8) << compA
                       << ") that " << metaB.GetName() << " writes";
                    conflicts.push_back(ss.str());
                }
            }
        }

        return conflicts;
    }

    bool ComponentConflictDetector::_hasWriteWriteConflict(
        const std::vector<ComponentTypeId>& writeComps1,
        const std::vector<ComponentTypeId>& writeComps2)
    {
        for (const auto& compA : writeComps1) {
            for (const auto& compB : writeComps2) {
                if (compA == compB) {
                    return true;  // Both write to same component
                }
            }
        }
        return false;
    }

    bool ComponentConflictDetector::_hasWriteReadConflict(
        const std::vector<ComponentTypeId>& writeComps1,
        const std::vector<ComponentTypeId>& readComps2,
        const std::vector<ComponentTypeId>& writeComps2,
        const std::vector<ComponentTypeId>& readComps1)
    {
        // Check write1-read2 conflicts
        for (const auto& compA : writeComps1) {
            for (const auto& compB : readComps2) {
                if (compA == compB) {
                    return true;
                }
            }
        }

        // Check write2-read1 conflicts
        for (const auto& compA : writeComps2) {
            for (const auto& compB : readComps1) {
                if (compA == compB) {
                    return true;
                }
            }
        }

        return false;
    }

    bool ComponentConflictDetector::_hasAccessConflict(
        const std::vector<ComponentAccess>& accessesA,
        const std::vector<ComponentAccess>& accessesB)
    {
        // Check all pairs of accesses for conflicts
        for (const auto& accessA : accessesA) {
            for (const auto& accessB : accessesB) {
                // Same component?
                if (accessA.ComponentId != accessB.ComponentId) {
                    continue;
                }

                // Check if modes conflict
                ComponentAccessMode modeA = accessA.Mode;
                ComponentAccessMode modeB = accessB.Mode;

                // Both write (Write or ReadWrite) -> conflict
                bool aWrites = (modeA == ComponentAccessMode::Write || modeA == ComponentAccessMode::ReadWrite);
                bool bWrites = (modeB == ComponentAccessMode::Write || modeB == ComponentAccessMode::ReadWrite);
                
                if (aWrites && bWrites) {
                    return true;  // Both write = conflict
                }

                // One writes, other reads -> conflict
                bool aReads = (modeA == ComponentAccessMode::Read || modeA == ComponentAccessMode::ReadWrite);
                bool bReads = (modeB == ComponentAccessMode::Read || modeB == ComponentAccessMode::ReadWrite);
                
                if ((aWrites && bReads) || (aReads && bWrites)) {
                    return true;  // One writes, other reads = conflict
                }

                // Both read -> no conflict
            }
        }

        return false;
    }

}
