/* Start Header *****************************************************************/
/*!
\file   AddComponentChange.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Deferred component addition change.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Changes;

/// <summary>
/// Deferred component addition change.
/// </summary>
public class AddComponentChange : IChangeHandler
{
    /// <summary>
    /// Entity to add component to
    /// </summary>
    public required Entity TargetEntity { get; set; }

    /// <summary>
    /// Component type
    /// </summary>
    public required Type ComponentType { get; set; }

    /// <summary>
    /// Component type name (for description only)
    /// </summary>
    public string ComponentTypeName { get; set; } = string.Empty;

    /// <summary>
    /// Component data
    /// </summary>
    public required object ComponentData { get; set; }

    public void Apply(World world)
    {
        if (ComponentType == null || ComponentData == null)
        {
            Console.WriteLine("[DeferredChanges] AddComponent failed: Missing component type or data");
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("AddComponent")
                ?.MakeGenericMethod(ComponentType);

            if (method != null)
            {
                method.Invoke(world, [TargetEntity, ComponentData]);
            }
            else
            {
                Console.WriteLine($"[DeferredChanges] AddComponent failed: Could not find method for {ComponentType.Name}");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[DeferredChanges] AddComponent error: {ex.Message}");
        }
    }

    public string GetDescription() => $"AddComponent<{ComponentTypeName ?? ComponentType?.Name ?? "Unknown"}>({TargetEntity.Id})";
}
