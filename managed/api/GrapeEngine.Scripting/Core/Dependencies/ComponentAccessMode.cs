/* Start Header *****************************************************************/
/*!
\file   ComponentAccessMode.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Component access mode for dependency resolution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Core.Dependencies;

/// <summary>
/// Component access mode for dependency resolution.
/// Matches the C++ ComponentAccessMode enum.
/// </summary>
public enum ComponentAccessMode
{
    /// <summary>
    /// Read-only access - shareable with other readers
    /// </summary>
    Read = 0,

    /// <summary>
    /// Exclusive write access - only one writer per group
    /// </summary>
    Write = 1,

    /// <summary>
    /// Read-write access - exclusive access for both reading and writing
    /// </summary>
    ReadWrite = 2
}
