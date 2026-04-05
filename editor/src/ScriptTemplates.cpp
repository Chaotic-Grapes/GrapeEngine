/* Start Header *****************************************************************/
/*!
\file   ScriptTemplates.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu

\brief
Implementation of script templates for ECS-aligned C# system generation.
*/
/* End Header *******************************************************************/

#include "ScriptTemplates.h"

namespace Editor::Templates {

    /**
     * @brief Dispatch to the correct template generator and return the generated C# source.
     * @param templateType Template to generate code for.
     * @param className Name of the generated class.
     * @param namespaceName Namespace to wrap the class in.
     * @return Generated C# source code as a string.
     */
    std::string ScriptTemplates::GenerateScript(
        ScriptTemplateType templateType,
        const std::string& className,
        const std::string& namespaceName)
    {
        switch (templateType)
        {
            case ScriptTemplateType::BasicSystem:
                return GenerateBasicSystemTemplate(className, namespaceName);
            case ScriptTemplateType::EditModeSystem:
                return GenerateEditModeSystemTemplate(className, namespaceName);
            default:
                return GenerateBasicSystemTemplate(className, namespaceName);
        }
    }

    /**
     * @brief Return a short human-readable description of the given template type for display in the UI.
     * @param templateType Template to describe.
     * @return Description string.
     */
    std::string ScriptTemplates::GetTemplateDescription(ScriptTemplateType templateType)
    {
        switch (templateType)
        {
            case ScriptTemplateType::BasicSystem:
                return "Basic system with query-based component processing";
            case ScriptTemplateType::EditModeSystem:
                return "System that runs only in the editor (edit mode)";
            default:
                return "Unknown template";
        }
    }

    /**
     * @brief Return a null-terminated array of template display name strings.
     * @param outCount Set to the number of entries in the returned array.
     * @return Pointer to the array of name strings.
     */
    const char* const* ScriptTemplates::GetTemplateNames(int& outCount)
    {
        static const char* names[] = {
            "BasicSystem",
            "EditModeSystem"
        };
        outCount = 2;
        return names;
    }

    /**
     * @brief Convert a template display name string back to its ScriptTemplateType enum value.
     * @param name Display name to look up.
     * @return Corresponding ScriptTemplateType, defaulting to BasicSystem if not recognized.
     */
    ScriptTemplateType ScriptTemplates::GetTemplateTypeFromName(const std::string& name)
    {
        if (name == "BasicSystem") return ScriptTemplateType::BasicSystem;
        if (name == "EditModeSystem") return ScriptTemplateType::EditModeSystem;
        return ScriptTemplateType::BasicSystem;
    }

    /**
     * @brief Generate a standard ISystem C# template with OnCreate/OnUpdate/OnDestroy stubs.
     * @param className Name of the generated class.
     * @param namespaceName Namespace to wrap the class in.
     * @return Generated C# source code as a string.
     */
    std::string ScriptTemplates::GenerateBasicSystemTemplate(
        const std::string& className,
        const std::string& namespaceName)
    {
        return
            "using GrapeEngine.Scripting.Systems;\n"
            "using GrapeEngine.Scripting.Systems.Attributes;\n\n"
            "namespace " + namespaceName + ";\n\n"
            "/// <summary>\n"
            "/// System that processes entities with specific components.\n"
            "/// This is a pure ECS system: it queries entities and updates their components.\n"
            "/// </summary>\n"
            "[System(SystemGroup.Update, SystemRunMode.PlayOnly)]\n"
            "public class " + className + " : SystemBase\n"
            "{\n"
            "    protected override void OnCreate()\n"
            "    {\n"
            "        Log(\"System " + className + " initialized\");\n"
            "    }\n\n"
            "    protected override void OnUpdate()\n"
            "    {\n"
            "        // TODO: Query entities and update components\n"
            "        // Example:\n"
            "        // var query = Query<Transform>();\n"
            "        // foreach (var (entity, transform) in query)\n"
            "        // {\n"
            "        //     // Process component\n"
            "        // }\n"
            "    }\n\n"
            "    protected override void OnDestroy()\n"
            "    {\n"
            "        Log(\"System " + className + " destroyed\");\n"
            "    }\n"
            "}\n";
    }

    /**
     * @brief Generate a C# system template decorated with [ExecuteInEditMode] for editor-only execution.
     * @param className Name of the generated class.
     * @param namespaceName Namespace to wrap the class in.
     * @return Generated C# source code as a string.
     */
    std::string ScriptTemplates::GenerateEditModeSystemTemplate(
        const std::string& className,
        const std::string& namespaceName)
    {
        return
            "using GrapeEngine.Scripting.Systems;\n"
            "using GrapeEngine.Scripting.Systems.Attributes;\n"
            "using GrapeEngine.Scripting.Internal.Hosting;\n\n"
            "namespace " + namespaceName + ";\n\n"
            "/// <summary>\n"
            "/// A system that runs in the editor (edit mode).\n"
            "/// Only executes when the game is not playing.\n"
            "/// </summary>\n"
            "[SystemGroup(SystemGroup.Update), SystemRunMode.EditOnly]\n"
            "public class " + className + " : SystemBase\n"
            "{\n"
            "    protected override void OnCreate()\n"
            "    {\n"
            "        Log(\"Edit-mode system " + className + " initialized\");\n"
            "    }\n\n"
            "    protected override void OnUpdate()\n"
            "    {\n"
            "        // This runs in edit mode only\n"
            "        // TODO: Add editor-only logic\n"
            "    }\n\n"
            "    protected override void OnDestroy()\n"
            "    {\n"
            "        Log(\"Edit-mode system " + className + " destroyed\");\n"
            "    }\n"
            "}\n";
    }
} 
