/* Start Header *****************************************************************/
/*!
\file    ComponentTypeRegistry.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Central registration declaration for all engine component types.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef COMPONENTTYPEREGISTRY_H
#define COMPONENTTYPEREGISTRY_H

namespace ECS {

/**
 * @brief Register all engine component types with their hash values for C# interop.
 * 
 * This function registers all C++ component types with FNV-1a hash values that
 * match the C# side implementation. MUST be called during engine initialization
 * before any C# script attempts to access components.
 * 
 * Component names must match exactly between C++ and C#.
 */
void RegisterAllComponentTypes();

}

#endif
