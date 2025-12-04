/* Start Header *****************************************************************/
/*!
\file   ISystem.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Interface for C# scripted systems that operate on the ECS World.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine;

/// <summary>
/// Interface for custom C# systems that execute gameplay logic.
/// Systems are registered globally and update all relevant entities each frame.
/// </summary>
public interface ISystem
{
    /// <summary>
    /// Called once when the system is first created.
    /// Use this for initialization logic.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    void OnCreate(World world) { }

    /// <summary>
    /// Called every frame. Implement gameplay logic here.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    /// <param name="deltaTime">Time elapsed since last frame in seconds</param>
    void OnUpdate(World world, float deltaTime);

    /// <summary>
    /// Called when the system is being destroyed.
    /// Use this for cleanup logic.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    void OnDestroy(World world) { }
}
