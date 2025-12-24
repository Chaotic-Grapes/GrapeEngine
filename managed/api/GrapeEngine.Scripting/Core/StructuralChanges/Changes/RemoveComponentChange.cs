/* Start Header *****************************************************************/
/*!
\file   RemoveComponentChange.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Deferred component removal change.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Changes;

/// <summary>
/// Deferred component removal change.
/// </summary>
public class RemoveComponentChange : IChangeHandler
{
    /// <summary>
    /// Entity to remove component from
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

    public void Apply(World world)
    {
        if (ComponentType == null)
        {
            Logging.LogInternal("[DeferredChanges] RemoveComponent failed: Missing component type", LogLevel.Warning);
            return;
        }

        try
        {
            var method = typeof(World).GetMethod("RemoveComponent")
                ?.MakeGenericMethod(ComponentType);

            if (method != null)
            {
                method.Invoke(world, [TargetEntity]);
            }
            else
            {
                Logging.LogInternal(
                    $"[DeferredChanges] RemoveComponent failed: Could not find RemoveComponent method for {ComponentType.Name}",
                    LogLevel.Warning);
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[DeferredChanges] RemoveComponent failed: {ex.Message}", LogLevel.Error);
        }
    }

    public string GetDescription() => $"RemoveComponent<{ComponentTypeName ?? ComponentType?.Name ?? "Unknown"}>({TargetEntity.Id})";
}
