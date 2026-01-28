/* Start Header *****************************************************************/
/*!
\file   EventsAPI.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
P/Invoke declarations for the Debug API used in scripting. Internal use only.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Internal.Unsafe;

/// <summary>
/// Internal P/Invoke declarations for Event Components operations.
/// Bridges C# event system to C++ EventDispatcher.
/// </summary>
internal partial class EventsAPI
{
    // ============================================================================
    // Collision Events
    // ============================================================================

    /// <summary>
    /// Fire a collision event between two entities.
    /// Adds CollisionEvent components to both entities.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_FireCollisionEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static unsafe partial void FireCollisionEvent(
        void* worldPtr,
        uint entity1Id,
        uint entity2Id,
        float contactPointX,
        float contactPointY,
        float contactPointZ,
        float contactNormalX,
        float contactNormalY,
        float contactNormalZ,
        float relativeVelocityX,
        float relativeVelocityY,
        float relativeVelocityZ,
        float impulseMagnitude
    );

    /// <summary>
    /// Fire a trigger enter event.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_FireTriggerEnterEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static unsafe partial void FireTriggerEnterEvent(
        void* worldPtr,
        uint triggerId,
        uint otherEntityId
    );

    /// <summary>
    /// Fire a trigger stay event (entity still overlapping).
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_FireTriggerStayEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static unsafe partial void FireTriggerStayEvent(
        void* worldPtr,
        uint triggerId,
        uint otherEntityId
    );

    /// <summary>
    /// Fire a collision exit event.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_FireCollisionExitEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static unsafe partial void FireCollisionExitEvent(
        void* worldPtr,
        uint entity1Id,
        uint entity2Id,
        float lastContactPointX,
        float lastContactPointY,
        float lastContactPointZ
    );

    /// <summary>
    /// Fire a trigger exit event.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_FireTriggerExitEvent")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static unsafe partial void FireTriggerExitEvent(
        void* worldPtr,
        uint triggerId,
        uint otherEntityId
    );

    /// <summary>
    /// Clear all frame event components.
    /// Called at the end of each frame.
    /// </summary>
    [LibraryImport("GrapeEngineNative", EntryPoint = "WorldInterop_ClearFrameEvents")]
    [UnmanagedCallConv(CallConvs = [typeof(CallConvCdecl)])]
    internal static unsafe partial void ClearFrameEvents(void* worldPtr);
}


