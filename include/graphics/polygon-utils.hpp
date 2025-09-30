#pragma once
#include <glm/glm.hpp>
#include <vector>

struct Triangle { glm::vec2 a, b, c; };
std::vector<Triangle> triangulateEarClipping(std::vector<glm::vec2> verts);