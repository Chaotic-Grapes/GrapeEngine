/* Start Header *****************************************************************/
/*!
\file   Command.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Represents a deferred structural change command.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.StructuralChanges.Commands;

/// <summary>
/// Represents a deferred structural change command.
/// </summary>
public struct Command
{
    /// <summary>
    /// Type of command to execute
    /// </summary>
    public CommandType Type { get; set; }

    /// <summary>
    /// Target entity (for add/remove/set operations)
    /// </summary>
    public Entity TargetEntity { get; set; }

    /// <summary>
    /// Component type hash (for add/remove/set operations)
    /// </summary>
    public ulong ComponentTypeHash { get; set; }

    /// <summary>
    /// Component runtime type (for generic method invocation)
    /// </summary>
    public Type ComponentType { get; set; }

    /// <summary>
    /// Component data (for set operations)
    /// </summary>
    public object ComponentData { get; set; }

    /// <summary>
    /// User-defined metadata for the command
    /// </summary>
    public object Metadata { get; set; }

    /// <summary>
    /// Frame this command was recorded
    /// </summary>
    public uint Frame { get; set; }

    /// <summary>
    /// Playback order index
    /// </summary>
    public int OrderIndex { get; set; }
}
