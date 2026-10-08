#pragma once

#include "HandTracking.hpp"
#include "Util.hpp"
#include <vector>

struct HandVertex {
    glm::vec3 position;
    D3DCOLOR color;
};
static_assert(sizeof(HandVertex) == 4 * sizeof(float)); // D3DFVF_XYZ | D3DFVF_DIFFUSE

// Appends opaque, shaded triangles in the same right-handed, metre-based
// reference space as the OpenXR views. No game camera or wheel anchoring.
void append_hand_mesh(const HandTracking::Hand& hand, std::vector<HandVertex>& vertices);
