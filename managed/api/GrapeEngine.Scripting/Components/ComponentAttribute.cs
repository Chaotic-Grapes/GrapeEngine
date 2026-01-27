/* Start Header *****************************************************************/
/*!
\file   ComponentAttribute.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Attribute to explicitly mark a struct as an ECS component for auto-discovery.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Components;

/// <summary>
/// Mark a struct as an ECS component for automatic discovery and registration.
/// 
/// Usage:
/// <code>
/// [Component]
/// public struct Health
/// {
///     public int CurrentHealth;
///     public int MaxHealth;
/// }
/// </code>
/// 
/// Requirements:
/// - Must be applied to a struct (value type)
/// - The struct must be unmanaged (contain only blittable types)
/// - The struct will be automatically discovered and registered with the ECS
/// 
/// Optional Parameters:
/// - Name: Custom display name for the component (defaults to class name)
/// 
/// Example with custom name:
/// <code>
/// [Component(Name = "Player Health")]
/// public struct Health
/// {
///     public int CurrentHealth;
///     public int MaxHealth;
/// }
/// </code>
/// </summary>
[AttributeUsage(AttributeTargets.Struct)]
public sealed class ComponentAttribute : Attribute
{
    /// <summary>
    /// Optional custom display name for the component.
    /// If not specified, the class name will be used.
    /// </summary>
    public string Name { get; set; } = string.Empty;
}

