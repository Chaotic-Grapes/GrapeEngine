/* Start Header *****************************************************************/
/*!
\file    ComponentAccessAttribute.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Component access attributes for declaring system component dependencies.

This file provides compile-time attributes and helpers for declaring which
components a system reads or writes. These are used by the dependency system
to automatically detect conflicts and schedule systems safely.

Similar to Unity DOTS [ReadOnly] attribute, this allows compile-time validation
and automatic dependency resolution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef COMPONENT_ACCESS_ATTRIBUTE_H
#define COMPONENT_ACCESS_ATTRIBUTE_H

#include "Export.h"
#include "ecs/ComponentRegistry.h"
#include "ecs/SystemEnums.h"
#include <vector>
#include <string>
#include <algorithm>

namespace ECS {

    // Forward declarations
    class ISystem;
    class SystemMetadata;

    /**
     * @brief Access mode for component reads/writes.
     */
    enum class ComponentAccessMode {
        Read,       // Read-only access - many systems can have this
        Write,      // Exclusive write - only one system per group
        ReadWrite   // Read-write - exclusive access
    };

    /**
     * @brief Describes one component access pattern.
     * 
     * Example:
     * ComponentAccess(TypeIdOf<Transform>(), ComponentAccessMode::Write)
     */
    struct ComponentAccess {
        ComponentTypeId ComponentId;
        ComponentAccessMode Mode;

        ComponentAccess(ComponentTypeId id, ComponentAccessMode mode)
            : ComponentId(id), Mode(mode) {}

        bool operator==(const ComponentAccess& other) const {
            return ComponentId == other.ComponentId && Mode == other.Mode;
        }

        bool operator!=(const ComponentAccess& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Builder for declaring component access patterns in a system.
     * 
     * Provides a fluent API for declaring which components a system accesses.
     * Used to populate SystemMetadata automatically. Consolidates all metadata
     * into a single unified structure.
     * 
     * Example:
     * @code
     * class PhysicsSystem : public ISystem {
     *     SystemMetadata GetMetadata() const override {
     *         return ComponentAccessBuilder("Physics")
     *             .ReadComponent<Rigidbody>()
     *             .ReadComponent<Velocity>()
     *             .WriteComponent<Transform>()
     *             .SetExecutionOrder(10)
     *             .SetGroup(SystemGroup::Physics)
     *             .Build();
     *     }
     * };
     * @endcode
     */
    class GRAPEENGINE_API ComponentAccessBuilder {
    public:
        /**
         * @brief Create an access builder with system name.
         * @param systemName Name of the system being configured
         */
        explicit ComponentAccessBuilder(const std::string& systemName)
            : m_name(systemName) {}

        /**
         * @brief Declare a read-only component access.
         * @tparam T Component type
         * @return This builder for chaining
         */
        template<typename T>
        ComponentAccessBuilder& ReadComponent() {
            m_accesses.emplace_back(TypeIdOf<T>(), ComponentAccessMode::Read);
            return *this;
        }

        /**
         * @brief Declare a write-exclusive component access.
         * @tparam T Component type
         * @return This builder for chaining
         */
        template<typename T>
        ComponentAccessBuilder& WriteComponent() {
            m_accesses.emplace_back(TypeIdOf<T>(), ComponentAccessMode::Write);
            return *this;
        }

        /**
         * @brief Declare a read-write (exclusive) component access.
         * @tparam T Component type
         * @return This builder for chaining
         */
        template<typename T>
        ComponentAccessBuilder& ReadWriteComponent() {
            m_accesses.emplace_back(TypeIdOf<T>(), ComponentAccessMode::ReadWrite);
            return *this;
        }

        /**
         * @brief Declare multiple read components from a list.
         * @param components Vector of component type IDs
         * @return This builder for chaining
         */
        ComponentAccessBuilder& ReadComponents(const std::vector<ComponentTypeId>& components) {
            for (auto id : components) {
                m_accesses.emplace_back(id, ComponentAccessMode::Read);
            }
            return *this;
        }

        /**
         * @brief Declare multiple write components from a list.
         * @param components Vector of component type IDs
         * @return This builder for chaining
         */
        ComponentAccessBuilder& WriteComponents(const std::vector<ComponentTypeId>& components) {
            for (auto id : components) {
                m_accesses.emplace_back(id, ComponentAccessMode::Write);
            }
            return *this;
        }

        /**
         * @brief Set the execution order for this system.
         * @param order Order value (lower = earlier)
         * @return This builder for chaining
         */
        ComponentAccessBuilder& SetExecutionOrder(int order) {
            m_executionOrder = order;
            return *this;
        }

        /**
         * @brief Set the execution group for this system.
         * @param group System group (PreUpdate, Update, Physics, Render, etc.)
         * @return This builder for chaining
         */
        ComponentAccessBuilder& SetGroup(SystemGroup group) {
            m_group = group;
            return *this;
        }

        /**
         * @brief Set the run mode for this system.
         * @param mode When system runs (edit/play/both)
         * @return This builder for chaining
         */
        ComponentAccessBuilder& SetRunMode(SystemRunMode mode) {
            m_runMode = mode;
            return *this;
        }

        /**
         * @brief Set whether this system is enabled by default.
         * @param enabled True if system starts enabled
         * @return This builder for chaining
         */
        ComponentAccessBuilder& SetEnabled(bool enabled) {
            m_enabled = enabled;
            return *this;
        }

        /**
         * @brief Set the system pointer for dependency tracking.
         * @param system Pointer to the system instance
         * @return This builder for chaining
         */
        ComponentAccessBuilder& SetSystemPtr(ISystem* system) {
            m_systemPtr = system;
            return *this;
        }

        /**
         * @brief Build the SystemMetadata.
         * @return Configured SystemMetadata (immutable after creation)
         */
        SystemMetadata Build();

        /**
         * @brief Get all declared accesses.
         * @return Vector of component accesses with their modes
         */
        const std::vector<ComponentAccess>& GetAccesses() const {
            return m_accesses;
        }

        /**
         * @brief Check if system writes to a component.
         * @tparam T Component type
         * @return True if this system has write access
         */
        template<typename T>
        bool WritesComponent() const {
            ComponentTypeId id = TypeIdOf<T>();
            for (const auto& access : m_accesses) {
                if (access.ComponentId == id && 
                    (access.Mode == ComponentAccessMode::Write || 
                     access.Mode == ComponentAccessMode::ReadWrite)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Check if system reads from a component.
         * @tparam T Component type
         * @return True if this system has read access
         */
        template<typename T>
        bool ReadsComponent() const {
            ComponentTypeId id = TypeIdOf<T>();
            for (const auto& access : m_accesses) {
                if (access.ComponentId == id && 
                    (access.Mode == ComponentAccessMode::Read || 
                     access.Mode == ComponentAccessMode::ReadWrite)) {
                    return true;
                }
            }
            return false;
        }

    private:
        std::string m_name;
        std::vector<ComponentAccess> m_accesses;
        int m_executionOrder = 0;
        SystemGroup m_group = SystemGroup::Update;
        SystemRunMode m_runMode = SystemRunMode::PlayOnly;
        bool m_enabled = true;
        ISystem* m_systemPtr = nullptr;

        friend class ComponentAccessValidator;
    };

    /**
     * @brief Validates component access declarations.
     * 
     * Ensures that declared component accesses are valid and consistent.
     * Automatically called by ComponentAccessBuilder::Build() to validate
     * all declared component accesses before creating SystemMetadata.
     * 
     * Can validate:
     * - No duplicate declarations for same component
     * - Proper read/write categorization
     * - Consistency with component access modes
     * 
     * Example usage:
     * @code
     * ComponentAccessBuilder builder("MySystem");
     * builder.Reads<Position>();
     * builder.Writes<Velocity>();
     * 
     * // Validation is automatic in Build()
     * auto metadata = builder.Build();  // Validates internally
     * 
     * // Or validate manually:
     * std::vector<std::string> errors;
     * if (!ComponentAccessValidator::ValidateWithErrors(builder, errors)) {
     *     for (const auto& error : errors) {
     *         LOG_ERROR("Validation error: " << error);
     *     }
     * }
     * @endcode
     */
    class GRAPEENGINE_API ComponentAccessValidator {
    public:
        /**
         * @brief Validate component access declarations.
         * @param builder The builder containing access declarations
         * @return True if valid, false if errors found
         */
        static bool Validate(const ComponentAccessBuilder& builder);

        /**
         * @brief Validate and report errors.
         * @param builder The builder to validate
         * @param outErrors Vector to receive error messages
         * @return True if valid, false otherwise
         */
        static bool ValidateWithErrors(const ComponentAccessBuilder& builder,
                                       std::vector<std::string>& outErrors);

        /**
         * @brief Check for duplicate component declarations.
         * @param accesses Vector of component accesses
         * @return True if duplicates found
         */
        static bool HasDuplicates(const std::vector<ComponentAccess>& accesses);

        /**
         * @brief Get a human-readable error message for access mode conflict.
         * @param component Component type ID
         * @param currentMode Current declared mode
         * @param conflictMode Conflicting mode
         * @return Error message string
         */
        static std::string GetConflictMessage(ComponentTypeId component,
                                             ComponentAccessMode currentMode,
                                             ComponentAccessMode conflictMode);
    };

    /**
     * @brief Helper functions for component access queries and analysis.
     * 
     * Provides utility functions for:
     * - Checking if access modes conflict
     * - Converting access modes to human-readable strings
     * - Checking read/write capability of access modes
     * - Determining precedence between conflicting accesses
     * 
     * These helpers work with ComponentAccessMode and are used by
     * both ComponentAccessValidator and SystemDependencyGraph for
     * dependency analysis.
     * 
     * Example usage:
     * @code
     * // Check if two systems can access same component safely
     * ComponentAccessMode modeA = ComponentAccessMode::Read;
     * ComponentAccessMode modeB = ComponentAccessMode::Read;
     * 
     * if (!ComponentAccessHelper::AccessesConflict(modeA, modeB)) {
     *     // Both systems can read the component safely
     * }
     * 
     * // Convert to string for logging
     * std::string modeStr = ComponentAccessHelper::ModeToString(modeA);
     * LOG_DEBUG("System accesses component in " << modeStr << " mode");
     * 
     * // Determine execution order for conflicting accesses
     * int precedence = ComponentAccessHelper::DeterminePrecedence(
     *     modeA, modeB, executionOrderA, executionOrderB);
     * if (precedence < 0) {
     *     LOG_DEBUG("System A must execute before System B");
     * }
     * @endcode
     */
    class GRAPEENGINE_API ComponentAccessHelper {
    public:
        /**
         * @brief Check if two access modes conflict.
         * @param modeA First access mode
         * @param modeB Second access mode
         * @return True if accessing same component with these modes causes conflict
         */
        static bool AccessesConflict(ComponentAccessMode modeA, ComponentAccessMode modeB);

        /**
         * @brief Get access mode string representation.
         * @param mode Access mode
         * @return String like "Read", "Write", "ReadWrite"
         */
        static std::string ModeToString(ComponentAccessMode mode);

        /**
         * @brief Check if a component is accessed for writing.
         * @param mode The access mode
         * @return True if mode includes write access
         */
        static bool IsWriteAccess(ComponentAccessMode mode);

        /**
         * @brief Check if a component is accessed for reading.
         * @param mode The access mode
         * @return True if mode includes read access
         */
        static bool IsReadAccess(ComponentAccessMode mode);

        /**
         * @brief Check if two systems have compatible access patterns for a component.
         * @param systemAMode System A's access mode for the component
         * @param systemBMode System B's access mode for the component
         * @return True if they can both access the component safely
         */
        static bool CanAccessTogether(ComponentAccessMode systemAMode,
                                     ComponentAccessMode systemBMode);

        /**
         * @brief Find which system must run first based on access modes.
         * @param modeA System A's access mode
         * @param modeB System B's access mode
         * @param orderA System A's execution order
         * @param orderB System B's execution order
         * @return Negative if A first, positive if B first, zero if undetermined
         */
        static int DeterminePrecedence(ComponentAccessMode modeA,
                                      ComponentAccessMode modeB,
                                      int orderA, int orderB);
    };

}

#endif
