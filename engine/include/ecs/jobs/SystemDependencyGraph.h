/* Start Header *****************************************************************/
/*!
\file    SystemDependencyGraph.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
DEPRECATED: This file is maintained for backward compatibility only.

The SystemDependencyGraph class has been consolidated into
ecs/SystemDependencyGraph.h with support for both direct system pointers
and metadata-based scheduling.

New code should use: ecs/SystemDependencyGraph.h directly.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_JOBS_SYSTEM_DEPENDENCY_GRAPH_H
#define ECS_JOBS_SYSTEM_DEPENDENCY_GRAPH_H

// Forward to the consolidated main version
#include "ecs/SystemDependencyGraph.h"

// For backward compatibility, maintain the SystemDependencyMetadata typedef
// This struct is now defined in ecs/ISystem.h

#endif  // ECS_JOBS_SYSTEM_DEPENDENCY_GRAPH_H

