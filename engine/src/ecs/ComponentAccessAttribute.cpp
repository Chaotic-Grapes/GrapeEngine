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

    /**
     * @brief Finalize the builder and produce a validated SystemMetadata instance.
     *        Logs a warning if component access validation fails.
     * @return Fully populated SystemMetadata for the system.
     */
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

    /**
     * @brief Return all component IDs accessed with Read or ReadWrite mode.
     * @return Vector of component type IDs that are read by this system.
     */
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

    /**
     * @brief Return all component IDs accessed with Write or ReadWrite mode.
     * @return Vector of component type IDs that are written by this system.
     */
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

    /**
     * @brief Validate the component accesses in the given builder.
     * @param builder The builder to validate.
     * @return True if valid, false otherwise.
     */
    bool ComponentAccessValidator::Validate(const ComponentAccessBuilder& builder) {
        std::vector<std::string> errors;
        return ValidateWithErrors(builder, errors);
    }

    /**
     * @brief Validate the component accesses and collect error messages.
     * @param builder The builder to validate.
     * @param outErrors Output vector populated with error descriptions.
     * @return True if valid, false if any errors were found.
     */
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

    /**
     * @brief Check whether any component access appears more than once with the same mode.
     * @param accesses The list of component accesses to inspect.
     * @return True if an exact duplicate (same component ID and mode) is found.
     */
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

    /**
     * @brief Build a human-readable description of an access mode conflict.
     * @param component The component type ID involved (unused, reserved for future use).
     * @param currentMode The access mode already declared.
     * @param conflictMode The conflicting access mode being declared.
     * @return A string describing the conflict.
     */
    std::string ComponentAccessValidator::GetConflictMessage(ComponentTypeId /*component*/,
                                                            ComponentAccessMode currentMode,
                                                            ComponentAccessMode conflictMode) {
        std::stringstream ss;
        ss << "Component conflict: ";
        ss << "already declared as " << ComponentAccessHelper::ModeToString(currentMode);
        ss << " but attempting to declare as " << ComponentAccessHelper::ModeToString(conflictMode);
        return ss.str();
    }

    /**
     * @brief Determine whether two access modes conflict with each other.
     * @param modeA First access mode.
     * @param modeB Second access mode.
     * @return True if the modes conflict (i.e., at least one is Write or ReadWrite).
     */
    bool ComponentAccessHelper::AccessesConflict(ComponentAccessMode modeA, ComponentAccessMode modeB) {
        // Read-only accesses don't conflict
        if (modeA == ComponentAccessMode::Read && modeB == ComponentAccessMode::Read) {
            return false;
        }

        // Any combination with Write or ReadWrite conflicts
        return true;
    }

    /**
     * @brief Convert a ComponentAccessMode enum value to its string representation.
     * @param mode The access mode to convert.
     * @return String name of the mode ("Read", "Write", "ReadWrite", or "Unknown").
     */
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

    /**
     * @brief Check whether the given mode includes write access.
     * @param mode The access mode to test.
     * @return True if the mode is Write or ReadWrite.
     */
    bool ComponentAccessHelper::IsWriteAccess(ComponentAccessMode mode) {
        return mode == ComponentAccessMode::Write || mode == ComponentAccessMode::ReadWrite;
    }

    /**
     * @brief Check whether the given mode includes read access.
     * @param mode The access mode to test.
     * @return True if the mode is Read or ReadWrite.
     */
    bool ComponentAccessHelper::IsReadAccess(ComponentAccessMode mode) {
        return mode == ComponentAccessMode::Read || mode == ComponentAccessMode::ReadWrite;
    }

    /**
     * @brief Determine whether two systems can safely access a component concurrently.
     * @param systemAMode Access mode of system A.
     * @param systemBMode Access mode of system B.
     * @return True only if both modes are Read-only.
     */
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

    /**
     * @brief Determine execution precedence between two conflicting systems based on their execution orders.
     * @param modeA Access mode of system A.
     * @param modeB Access mode of system B.
     * @param orderA Execution order of system A.
     * @param orderB Execution order of system B.
     * @return -1 if A goes first, 1 if B goes first, 0 if undetermined or no conflict.
     */
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
