/* Start Header *****************************************************************/
/*!
\file   ScriptTemplates.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Script templates for ECS-aligned C# system generation.
Provides reusable templates for different types of systems.

Provides:
- BasicSystem: Simple ISystem with query-based processing
- EditModeSystem: System that runs only in editor
- HotReloadSystem: System with preserved state across hot reloads
- MetadataSystem: System implementing ISystemMetadata for custom group assignment
*/
/* End Header *******************************************************************/

#pragma once

#include <string>
#include <unordered_map>

namespace Editor::Templates {

    /// <summary>
    /// Template types for script generation.
    /// </summary>
    enum class ScriptTemplateType {
        BasicSystem,      // Simple ISystem with OnCreate/OnUpdate/OnDestroy
        EditModeSystem,   // System marked with [ExecuteInEditMode]
        HotReloadSystem,  // System with [Preserve] fields for state persistence
        MetadataSystem    // System implementing ISystemMetadata
    };

    /// <summary>
    /// Script template generator for ECS-aligned C# systems.
    /// </summary>
    class ScriptTemplates {
    public:
        /// <summary>
        /// Generate a script from a template with the given class name and namespace.
        /// </summary>
        static std::string GenerateScript(
            ScriptTemplateType templateType,
            const std::string& className,
            const std::string& namespaceName
        );

        /// <summary>
        /// Get the description of a template for UI display.
        /// </summary>
        static std::string GetTemplateDescription(ScriptTemplateType templateType);

        /// <summary>
        /// Get all template names as an array of strings.
        /// </summary>
        static const char* const* GetTemplateNames(int& outCount);

        /// <summary>
        /// Parse a template name string to enum.
        /// </summary>
        static ScriptTemplateType GetTemplateTypeFromName(const std::string& name);

    private:
        static std::string GenerateBasicSystemTemplate(
            const std::string& className,
            const std::string& namespaceName
        );

        static std::string GenerateEditModeSystemTemplate(
            const std::string& className,
            const std::string& namespaceName
        );

        static std::string GenerateHotReloadSystemTemplate(
            const std::string& className,
            const std::string& namespaceName
        );

        static std::string GenerateMetadataSystemTemplate(
            const std::string& className,
            const std::string& namespaceName
        );
    };

}
