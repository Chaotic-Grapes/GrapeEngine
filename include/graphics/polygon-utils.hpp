/* Start Header *****************************************************************/
/*!
\file   polygon-utils.hpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Defines a simple Triangle struct and exposes a triangulation function based on
the ear clipping algorithm. Given a polygon represented by a list of vertices,
the function decomposes it into a set of non-overlapping triangles.

*/
/* End Header *******************************************************************/

#pragma once
#include <glm/glm.hpp>
#include <vector>

struct Triangle { glm::vec2 a, b, c; };
std::vector<Triangle> triangulateEarClipping(std::vector<glm::vec2> verts);