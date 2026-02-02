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
    glm::vec3 position;          // (location 0) - XYZ position
    glm::vec2 texCoord;          // (location 1) - UV coordinates
    glm::vec4 color;             // (location 2) - RGBA color
    float     texIndex;          // (location 3) - filled by renderer when you pass a textureId
    float     strokePx;          // (location 4) - stroke width in pixels (0 = filled)

    // Emissive attributes
    float emissiveTexIndex;      // (location 5) - emissive map texture index
    float emissiveStrength;      // (location 6) - HDR emissive multiplier

    // Material2D attributes (PBR)
    float normalTexIndex;        // (location 7) - normal map texture index (-1 = none)
    float mraTexIndex;           // (location 8) - metallic-roughness-AO map texture index (-1 = none)
    glm::vec4 materialParams;    // (location 9) - x=Metallic, y=Smoothness, z=AOStrength, w=NormalStrength
	float materialFlags;         // (location 10) - bitmask of Material2D::Material2DFlags
};
