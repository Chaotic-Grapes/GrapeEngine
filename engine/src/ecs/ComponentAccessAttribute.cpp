/* Start Header *****************************************************************/
/*!
\file    ComponentAccessAttribute.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of component access attributes and validation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/ComponentAccessAttribute.h"
#include "ecs/ISystem.h"
#include "core/Logger.h"
#include <algorithm>
#include <sstream>

namespace ECS {

    SystemMetadata ComponentAccessBuilder::Build() {
        SystemMetadata metadata;
        
        // Use member initialization - direct access via friend class
        metadata.m_name = m_name;
        metadata.m_componentAccesses = m_accesses;
        metadata.m_executionOrder = m_executionOrder;
        metadata.m_group = m_group;
        metadata.m_runMode = m_runMode;
        metadata.m_enabled = m_enabled;
        metadata.m_systemPtr = m_systemPtr;
        
        // Validate component accesses
        std::vector<std::string> validationErrors;
        if (!ComponentAccessValidator::ValidateWithErrors(*this, validationErrors)) {
            // Log validation errors
            std::stringstream errorLog;
            errorLog << "ComponentAccess validation failed for system '" << m_name << "':";
            for (const auto& error : validationErrors) {
                errorLog << "\n  - " << error;
            }
            
            LOG_WARNING(errorLog.str());
        }
        
        return metadata;
    }

    // Compute read components from the unified ComponentAccesses list
    std::vector<ComponentTypeId> SystemMetadata::GetReadComponents() const {
        std::vector<ComponentTypeId> result;
        for (const auto& access : m_componentAccesses) {
            if (access.Mode == ComponentAccessMode::Read || 
                access.Mode == ComponentAccessMode::ReadWrite) {
                result.push_back(access.ComponentId);
            }
        }
        return result;
    }

    // Compute write components from the unified ComponentAccesses list
    std::vector<ComponentTypeId> SystemMetadata::GetWriteComponents() const {
        std::vector<ComponentTypeId> result;
        for (const auto& access : m_componentAccesses) {
            if (access.Mode == ComponentAccessMode::Write || 
                access.Mode == ComponentAccessMode::ReadWrite) {
                result.push_back(access.ComponentId);
            }
        }
        return result;
    }

    bool ComponentAccessValidator::Validate(const ComponentAccessBuilder& builder) {
        std::vector<std::string> errors;
        return ValidateWithErrors(builder, errors);
    }

    bool ComponentAccessValidator::ValidateWithErrors(const ComponentAccessBuilder& builder,
                                                      std::vector<std::string>& outErrors) {
        outErrors.clear();
        bool isValid = true;

        // Check for duplicate component declarations
        if (HasDuplicates(builder.GetAccesses())) {
            outErrors.push_back("Duplicate component declarations found");
            isValid = false;
        }

        return isValid;
    }

    bool ComponentAccessValidator::HasDuplicates(const std::vector<ComponentAccess>& accesses) {
        // Treat identical duplicate declarations (same component, same mode)
        // as a real duplicate. Combinations of Read/Write/ReadWrite are
        // mergeable into a single effective access (e.g. Read + Write -> ReadWrite)
        // and should not be considered an error here.
        for (size_t i = 0; i < accesses.size(); ++i) {
            for (size_t j = i + 1; j < accesses.size(); ++j) {
                if (accesses[i].ComponentId == accesses[j].ComponentId &&
                    accesses[i].Mode == accesses[j].Mode) {
                    // Exact duplicate declaration (same component and same mode)
                    return true;
                }
            }
        }

        return false;
    }

    std::string ComponentAccessValidator::GetConflictMessage(ComponentTypeId component,
                                                            ComponentAccessMode currentMode,
                                                            ComponentAccessMode conflictMode) {
        std::stringstream ss;
        ss << "Component conflict: ";
        ss << "already declared as " << ComponentAccessHelper::ModeToString(currentMode);
        ss << " but attempting to declare as " << ComponentAccessHelper::ModeToString(conflictMode);
        return ss.str();
    }

    bool ComponentAccessHelper::AccessesConflict(ComponentAccessMode modeA, ComponentAccessMode modeB) {
        // Read-only accesses don't conflict
        if (modeA == ComponentAccessMode::Read && modeB == ComponentAccessMode::Read) {
            return false;
        }

        // Any combination with Write or ReadWrite conflicts
        return true;
    }

    std::string ComponentAccessHelper::ModeToString(ComponentAccessMode mode) {
        switch (mode) {
            case ComponentAccessMode::Read:
                return "Read";
            case ComponentAccessMode::Write:
                return "Write";
            case ComponentAccessMode::ReadWrite:
                return "ReadWrite";
            default:
                return "Unknown";
        }
    }

    bool ComponentAccessHelper::IsWriteAccess(ComponentAccessMode mode) {
        return mode == ComponentAccessMode::Write || mode == ComponentAccessMode::ReadWrite;
    }

    bool ComponentAccessHelper::IsReadAccess(ComponentAccessMode mode) {
        return mode == ComponentAccessMode::Read || mode == ComponentAccessMode::ReadWrite;
    }

    bool ComponentAccessHelper::CanAccessTogether(ComponentAccessMode systemAMode,
                                                 ComponentAccessMode systemBMode) {
        // Both read - OK
        if (systemAMode == ComponentAccessMode::Read && 
            systemBMode == ComponentAccessMode::Read) {
            return true;
        }

        // One or both write - conflict
        return false;
    }

    int ComponentAccessHelper::DeterminePrecedence(ComponentAccessMode modeA,
                                                  ComponentAccessMode modeB,
                                                  int orderA, int orderB) {
        // If they can access together (both read), no precedence required
        if (CanAccessTogether(modeA, modeB)) {
            return 0;
        }

        // There's a conflict - use execution order to determine precedence
        if (orderA < orderB) {
            return -1;  // A goes first
        }
        else if (orderA > orderB) {
            return 1;   // B goes first
        }
        
        // Same order with conflicting access - error condition
        return 0;  // Undetermined
    }

}
