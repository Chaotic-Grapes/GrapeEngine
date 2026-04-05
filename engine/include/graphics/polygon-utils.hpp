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

/**
 * @brief Decompose a polygon into triangles using the ear-clipping algorithm.
 * @param verts Ordered polygon vertices (counter-clockwise preferred).
 * @return List of non-overlapping triangles covering the polygon area.
 */
std::vector<Triangle> triangulateEarClipping(std::vector<glm::vec2> verts);