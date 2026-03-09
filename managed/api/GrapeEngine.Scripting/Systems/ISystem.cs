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

namespace GrapeEngine.Scripting.Systems;

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
    void OnUpdate(World world);

    /// <summary>
    /// Determines whether this system should run this frame.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    /// <returns>True to run OnUpdate, false to skip.</returns>
    bool ShouldRun(World world) => true;

    /// <summary>
    /// Called when the system is being destroyed.
    /// Use this for cleanup logic.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    void OnDestroy(World world) { }

    /// <summary>
    /// Called when a scene starts playing (editor: transitioning to Play mode).
    /// </summary>
    void OnSceneStart() { }

    /// <summary>
    /// Called when a scene stops playing (editor: transitioning away from Play mode).
    /// </summary>
    void OnSceneStop() { }

    /// <summary>
    /// Called when the system transitions from not running to running.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    void OnStartRunning(World world) { }

    /// <summary>
    /// Called when the system transitions from running to not running.
    /// </summary>
    /// <param name="world">The ECS World instance</param>
    void OnStopRunning(World world) { }
}

