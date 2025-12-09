/* Start Header *****************************************************************/
/*!
\file   ISystemMetadata.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Interface for systems to provide custom metadata.
Systems can implement this interface instead of using attributes for more
control over their execution group and other metadata.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Hosting;

namespace GrapeEngine.Scripting.Systems;

/// <summary>
/// Optional interface for systems to provide custom metadata.
/// If implemented, metadata from this interface takes precedence over attributes.
/// </summary>
public interface ISystemMetadata
{
    /// <summary>
    /// The execution group (phase) this system runs in.
    /// </summary>
    SystemGroup Group { get; }
}
