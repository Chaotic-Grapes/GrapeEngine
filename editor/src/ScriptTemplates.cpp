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
            case ScriptTemplateType::HotReloadSystem:
                return GenerateHotReloadSystemTemplate(className, namespaceName);
            case ScriptTemplateType::MetadataSystem:
                return GenerateMetadataSystemTemplate(className, namespaceName);
            default:
                return GenerateBasicSystemTemplate(className, namespaceName);
        }
    }

    std::string ScriptTemplates::GetTemplateDescription(ScriptTemplateType templateType)
    {
        switch (templateType)
        {
            case ScriptTemplateType::BasicSystem:
                return "Basic system with query-based component processing";
            case ScriptTemplateType::EditModeSystem:
                return "System that runs only in the editor (marked with [ExecuteInEditMode])";
            case ScriptTemplateType::HotReloadSystem:
                return "System with [Preserve] fields that persist state across hot reloads";
            case ScriptTemplateType::MetadataSystem:
                return "System implementing ISystemMetadata for custom execution group assignment";
            default:
                return "Unknown template";
        }
    }

    const char* const* ScriptTemplates::GetTemplateNames(int& outCount)
    {
        static const char* names[] = {
            "BasicSystem",
            "EditModeSystem",
            "HotReloadSystem",
            "MetadataSystem"
        };
        outCount = 4;
        return names;
    }

    ScriptTemplateType ScriptTemplates::GetTemplateTypeFromName(const std::string& name)
    {
        if (name == "BasicSystem") return ScriptTemplateType::BasicSystem;
        if (name == "EditModeSystem") return ScriptTemplateType::EditModeSystem;
        if (name == "HotReloadSystem") return ScriptTemplateType::HotReloadSystem;
        if (name == "MetadataSystem") return ScriptTemplateType::MetadataSystem;
        return ScriptTemplateType::BasicSystem;
    }

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
            "        Logging.Log(\"System " + className + " initialized\");\n"
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
            "        Logging.Log(\"System " + className + " destroyed\");\n"
            "    }\n"
            "}\n";
    }

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
            "[ExecuteInEditMode]\n"
            "[SystemGroup(SystemGroup.Update)]\n"
            "public class " + className + " : SystemBase\n"
            "{\n"
            "    protected override void OnCreate()\n"
            "    {\n"
            "        Logging.Log(\"Edit-mode system " + className + " initialized\");\n"
            "    }\n\n"
            "    protected override void OnUpdate()\n"
            "    {\n"
            "        // This runs in edit mode only\n"
            "        // TODO: Add editor-only logic\n"
            "    }\n\n"
            "    protected override void OnDestroy()\n"
            "    {\n"
            "        Logging.Log(\"Edit-mode system " + className + " destroyed\");\n"
            "    }\n"
            "}\n";
    }

    std::string ScriptTemplates::GenerateHotReloadSystemTemplate(
        const std::string& className,
        const std::string& namespaceName)
    {
        return
            "using GrapeEngine.Scripting.Systems;\n"
            "using GrapeEngine.Scripting.Systems.Attributes;\n"
            "using GrapeEngine.Scripting.Internal.Hosting;\n"
            "using GrapeEngine.Scripting.Services;\n\n"
            "namespace " + namespaceName + ";\n\n"
            "/// <summary>\n"
            "/// A system with state preserved across hot reloads.\n"
            "/// Fields marked with [Preserve] will be automatically serialized and restored.\n"
            "/// </summary>\n"
            "[SystemGroup(SystemGroup.Update)]\n"
            "public class " + className + " : SystemBase\n"
            "{\n"
            "    [Preserve]\n"
            "    private int _counter = 0;\n\n"
            "    [Preserve]\n"
            "    private float _accumulatedTime = 0f;\n\n"
            "    protected override void OnCreate()\n"
            "    {\n"
            "        Logging.Log(\"System " + className + " initialized (counter=\" + _counter + \")\");\n"
            "    }\n\n"
            "    protected override void OnUpdate()\n"
            "    {\n"
            "        var deltaTime = Time.DeltaTime;\n"
            "        _accumulatedTime += deltaTime;\n"
            "        _counter++;\n\n"
            "        // TODO: Your game logic here\n"
            "    }\n\n"
            "    protected override void OnDestroy()\n"
            "    {\n"
            "        Logging.Log(\"System " + className + " destroyed\");\n"
            "    }\n"
            "}\n";
    }

    std::string ScriptTemplates::GenerateMetadataSystemTemplate(
        const std::string& className,
        const std::string& namespaceName)
    {
        return
            "using GrapeEngine.Scripting.Systems;\n"
            "using GrapeEngine.Scripting.Systems.Attributes;\n\n"
            "namespace " + namespaceName + ";\n\n"
            "/// <summary>\n"
            "/// A system that provides custom metadata through the ISystemMetadata interface.\n"
            "/// Allows dynamic control of execution group and other system properties.\n"
            "/// </summary>\n"
            "[SystemGroup(SystemGroup.Update)]\n"
            "public class " + className + " : SystemBase, ISystemMetadata\n"
            "{\n"
            "    /// <summary>\n"
            "    /// Define which execution phase this system runs in.\n"
            "    /// </summary>\n"
            "    public SystemGroup Group => SystemGroup.Update;\n\n"
            "    protected override void OnCreate()\n"
            "    {\n"
            "        Logging.Log(\"System " + className + " initialized\");\n"
            "    }\n\n"
            "    protected override void OnUpdate()\n"
            "    {\n"
            "        // TODO: Your game logic here\n"
            "    }\n\n"
            "    protected override void OnDestroy()\n"
            "    {\n"
            "        Logging.Log(\"System " + className + " destroyed\");\n"
            "    }\n"
            "}\n";
    }

} 
