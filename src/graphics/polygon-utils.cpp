/* Start Header *****************************************************************/
/*!
\file   polygon-utils.cpp
\author Choi Meng Yew (100%)
\par    choi.m@digipen.edu
\date   3rd October 2025
\brief
Implements a polygon triangulation utility based on the ear clipping algorithm.
The function `triangulateEarClipping` takes a simple polygon (list of vertices)
and decomposes it into non-overlapping triangles suitable for rendering.

Details:
- Ensures polygon winding is counter-clockwise (reverses if necessary).
- Iteratively clips convex "ears" while checking that no other vertex lies inside.
- Returns a list of Triangle structs containing vertex positions.
- Handles both convex and concave polygons; degeneracy stops triangulation early.
*/
/* End Header *******************************************************************/

#include "../include/graphics/polygon-utils.hpp"
#include <algorithm>  // for std::reverse
#include <numeric>    // for std::iota
static float cross(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (b.x - a.x) * (c.y - a.y) -
        (b.y - a.y) * (c.x - a.x);
}

static bool isPointInTriangle(const glm::vec2& p,
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec2& c) {
    float c1 = cross(p, a, b);
    float c2 = cross(p, b, c);
    float c3 = cross(p, c, a);
    bool hasNeg = (c1 < 0) || (c2 < 0) || (c3 < 0);
    bool hasPos = (c1 > 0) || (c2 > 0) || (c3 > 0);
    return !(hasNeg && hasPos);
}

std::vector<Triangle> triangulateEarClipping(std::vector<glm::vec2> verts) {
    std::vector<Triangle> result;

    if (verts.size() < 3) return result;

    // Ensure polygon is CCW
    float area = 0;
    for (size_t i = 0; i < verts.size(); i++) {
        const glm::vec2& v1 = verts[i];
        const glm::vec2& v2 = verts[(i + 1) % verts.size()];
        area += (v1.x * v2.y - v2.x * v1.y);
    }
    if (area < 0) std::reverse(verts.begin(), verts.end());

    // Work on a copy
    std::vector<int> indices(verts.size());
    std::iota(indices.begin(), indices.end(), 0);

    while (indices.size() > 3) {
        bool earFound = false;

        for (size_t i = 0; i < indices.size(); i++) {
            int prev = indices[(i + indices.size() - 1) % indices.size()];
            int curr = indices[i];
            int next = indices[(i + 1) % indices.size()];

            const glm::vec2& a = verts[prev];
            const glm::vec2& b = verts[curr];
            const glm::vec2& c = verts[next];

            // Check convexity
            if (cross(a, b, c) <= 0) continue;

            // Check no other vertex inside
            bool hasInside = false;
            for (int idx : indices) {
                if (idx == prev || idx == curr || idx == next) continue;
                if (isPointInTriangle(verts[idx], a, b, c)) {
                    hasInside = true;
                    break;
                }
            }
            if (hasInside) continue;

            // Found an ear
            result.push_back({ a, b, c });
            indices.erase(indices.begin() + i);
            earFound = true;
            break;
        }

        if (!earFound) {
            // Degenerate polygon
            break;
        }
    }

    // Final triangle
    if (indices.size() == 3) {
        result.push_back({ verts[indices[0]], verts[indices[1]], verts[indices[2]] });
    }

    return result;
}