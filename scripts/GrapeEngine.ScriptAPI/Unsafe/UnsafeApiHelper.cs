/* Start Header *****************************************************************/
/*!
\file   UnsafeApiHelper.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   29th October 2025
\brief
Helper class for unsafe API interop. Internal use only. To be expanded later on.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.ScriptAPI.Unsafe;

internal class UnsafeApiHelper
{

    // Native library name constant
    // Used for DllImport attributes (we're actually using LibraryImport)
    public const string NativeLib = "GrapeEngine.dll";
}
