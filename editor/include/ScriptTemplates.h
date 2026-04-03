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

    enum class ScriptTemplateType {
        BasicSystem,      // Simple ISystem with OnCreate/OnUpdate/OnDestroy
        EditModeSystem    // System marked with [ExecuteInEditMode]
    };

    class ScriptTemplates {
    public:
        /**
         * @brief Generate a C# script from the given template type.
         * @param templateType  Template to use for code generation.
         * @param className     Name of the generated class.
         * @param namespaceName Namespace to wrap the generated class in.
         * @return Generated C# source code as a string.
         */
        static std::string GenerateScript(
            ScriptTemplateType templateType,
            const std::string& className,
            const std::string& namespaceName
        );

        /**
         * @brief Get the human-readable description of a template for UI display.
         * @param templateType Template to describe.
         * @return Description string.
         */
        static std::string GetTemplateDescription(ScriptTemplateType templateType);

        /**
         * @brief Get all template display names as a null-terminated array.
         * @param outCount Set to the number of entries in the returned array.
         * @return Pointer to an array of null-terminated name strings.
         */
        static const char* const* GetTemplateNames(int& outCount);

        /**
         * @brief Parse a template name string back to its enum value.
         * @param name Display name to look up.
         * @return Corresponding ScriptTemplateType enum value.
         */
        static ScriptTemplateType GetTemplateTypeFromName(const std::string& name);

    private:
        /**
         * @brief Generate a standard C# ISystem template with OnCreate/OnUpdate/OnDestroy stubs.
         * @param className     Name of the generated class.
         * @param namespaceName Namespace to wrap the generated class in.
         * @return Generated C# source code as a string.
         */
        static std::string GenerateBasicSystemTemplate(
            const std::string& className,
            const std::string& namespaceName
        );

        /**
         * @brief Generate a C# system template decorated with [ExecuteInEditMode].
         * @param className     Name of the generated class.
         * @param namespaceName Namespace to wrap the generated class in.
         * @return Generated C# source code as a string.
         */
        static std::string GenerateEditModeSystemTemplate(
            const std::string& className,
            const std::string& namespaceName
        );
    };

}
