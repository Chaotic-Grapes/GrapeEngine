/* Start Header *****************************************************************/
/*!
\file   vertex.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   31st October 2025
\brief
Defines the Vertex struct, which represents a single vertex used in 2D and 3D
rendering. Each vertex stores its position, texture coordinates, color, texture
index, and stroke width for use by the Renderer.

Responsibilities:
- Store per-vertex data passed to the GPU through vertex buffers.
- Support textured, colored, and stroked geometry for batching systems.
- Used by the Renderer for quads, lines, circles, and other debug shapes.
*/
/* End Header *******************************************************************/

#pragma once
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoord;
    glm::vec4 color;
    float     texIndex; // filled by renderer when you pass a textureId
    float     strokePx; // stroke width in pixels (0 = filled)
};
