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
        metadata.Name = m_name;
        metadata.ReadComponents = m_readComponents;         // Deprecated, for backward compat
        metadata.WriteComponents = m_writeComponents;       // Deprecated, for backward compat
        metadata.ComponentAccesses = m_accesses;            // New unified field
        metadata.ExecutionOrder = m_executionOrder;
        metadata.Enabled = m_enabled;
        
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

        // Check for components declared in both read and write with conflicting modes
        for (const auto& readComp : builder.m_readComponents) {
            auto writeIt = std::find(builder.m_writeComponents.begin(),
                                     builder.m_writeComponents.end(),
                                     readComp);
            
            // Having same component in both read and write is OK (it's read-write mode)
            // But we could add validation for intentional read-write vs separate read/write declarations
        }

        return isValid;
    }

    bool ComponentAccessValidator::HasDuplicates(const std::vector<ComponentAccess>& accesses) {
        for (size_t i = 0; i < accesses.size(); ++i) {
            for (size_t j = i + 1; j < accesses.size(); ++j) {
                if (accesses[i].ComponentId == accesses[j].ComponentId &&
                    accesses[i].Mode != accesses[j].Mode) {
                    // Same component with conflicting modes is a duplicate
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
