#include "HandMesh.hpp"
#include <algorithm>
#include <cmath>
#include <gtc/constants.hpp>

namespace {
    constexpr int sides = 8;

    HandVertex vertex(glm::vec3 position, glm::vec3 normal)
    {
        const float light = 0.65f + 0.35f * std::max(0.0f, glm::dot(normal, glm::normalize(glm::vec3(-0.3f, 0.8f, 0.5f))));
        return { position, D3DCOLOR_XRGB(static_cast<int>(215 * light), static_cast<int>(165 * light), static_cast<int>(130 * light)) };
    }

    void triangle(std::vector<HandVertex>& vertices, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 normal)
    {
        vertices.push_back(vertex(a, normal));
        vertices.push_back(vertex(b, normal));
        vertices.push_back(vertex(c, normal));
    }

    glm::vec3 position(const XrHandJointLocationEXT& joint)
    {
        return { joint.pose.position.x, joint.pose.position.y, joint.pose.position.z };
    }

    float radius(const XrHandJointLocationEXT& joint)
    {
        return std::clamp(joint.radius, 0.003f, 0.018f);
    }

    void bone(std::vector<HandVertex>& vertices, const XrHandJointLocationEXT& a, const XrHandJointLocationEXT& b)
    {
        const auto start = position(a);
        const auto end = position(b);
        const auto delta = end - start;
        if (glm::dot(delta, delta) < 0.000001f || glm::dot(delta, delta) > 0.04f) {
            return;
        }
        const auto axis = glm::normalize(delta);
        const auto u = glm::normalize(glm::cross(axis, std::abs(axis.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
        const auto v = glm::cross(axis, u);
        for (int i = 0; i < sides; ++i) {
            const float angle0 = glm::two_pi<float>() * i / sides;
            const float angle1 = glm::two_pi<float>() * (i + 1) / sides;
            const auto n0 = u * std::cos(angle0) + v * std::sin(angle0);
            const auto n1 = u * std::cos(angle1) + v * std::sin(angle1);
            const auto a0 = vertex(start + radius(a) * n0, n0);
            const auto a1 = vertex(start + radius(a) * n1, n1);
            const auto b0 = vertex(end + radius(b) * n0, n0);
            const auto b1 = vertex(end + radius(b) * n1, n1);
            vertices.insert(vertices.end(), { a0, b0, a1, a1, b0, b1 });
        }
    }

    void joint_sphere(std::vector<HandVertex>& vertices, const XrHandJointLocationEXT& joint)
    {
        constexpr int rings = 4;
        const auto at = [&](int ring, int side) {
            const float latitude = glm::pi<float>() * ring / rings;
            const float longitude = glm::two_pi<float>() * side / sides;
            const glm::vec3 normal(std::sin(latitude) * std::cos(longitude), std::cos(latitude), std::sin(latitude) * std::sin(longitude));
            return vertex(position(joint) + radius(joint) * normal, normal);
        };
        for (int ring = 0; ring < rings; ++ring) {
            for (int side = 0; side < sides; ++side) {
                const auto a = at(ring, side);
                const auto b = at(ring, side + 1);
                const auto c = at(ring + 1, side);
                const auto d = at(ring + 1, side + 1);
                if (ring != 0)
                    vertices.insert(vertices.end(), { a, c, b });
                if (ring != rings - 1)
                    vertices.insert(vertices.end(), { b, c, d });
            }
        }
    }
}

void append_hand_mesh(const HandTracking::Hand& hand, std::vector<HandVertex>& vertices)
{
    if (!hand.active) {
        return;
    }
    const auto& joints = hand.joints;
    const auto p = [&](int index) { return position(joints[index]); };
    const auto& q = joints[XR_HAND_JOINT_PALM_EXT].pose.orientation;
    const auto rotation = glm::normalize(glm::quat(q.w, q.x, q.y, q.z));
    const auto normal = rotation * glm::vec3(0, 1, 0);
    const auto across = p(XR_HAND_JOINT_INDEX_PROXIMAL_EXT) - p(XR_HAND_JOINT_LITTLE_PROXIMAL_EXT);
    const auto wrist = p(XR_HAND_JOINT_WRIST_EXT);
    const std::array outline {
        wrist + across * 0.3f,
        p(XR_HAND_JOINT_INDEX_METACARPAL_EXT),
        p(XR_HAND_JOINT_INDEX_PROXIMAL_EXT),
        p(XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT),
        p(XR_HAND_JOINT_RING_PROXIMAL_EXT),
        p(XR_HAND_JOINT_LITTLE_PROXIMAL_EXT),
        p(XR_HAND_JOINT_LITTLE_METACARPAL_EXT),
        wrist - across * 0.3f,
    };
    const auto center = p(XR_HAND_JOINT_PALM_EXT);
    const auto thickness = normal * std::clamp(joints[XR_HAND_JOINT_PALM_EXT].radius * 0.5f, 0.008f, 0.015f);
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto a = outline[i];
        const auto b = outline[(i + 1) % outline.size()];
        triangle(vertices, center + thickness, a + thickness, b + thickness, normal);
        triangle(vertices, center - thickness, b - thickness, a - thickness, -normal);
        const auto edge = b - a;
        const auto side = glm::cross(edge, normal);
        if (glm::dot(side, side) > 0.000001f) {
            const auto side_normal = glm::normalize(side);
            triangle(vertices, a + thickness, a - thickness, b + thickness, side_normal);
            triangle(vertices, b + thickness, a - thickness, b - thickness, side_normal);
        }
    }

    constexpr std::array starts { XR_HAND_JOINT_THUMB_METACARPAL_EXT, XR_HAND_JOINT_INDEX_PROXIMAL_EXT,
        XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT, XR_HAND_JOINT_RING_PROXIMAL_EXT, XR_HAND_JOINT_LITTLE_PROXIMAL_EXT };
    for (const auto start : starts) {
        for (int i = 0; i < 4; ++i) {
            joint_sphere(vertices, joints[start + i]);
            if (i < 3)
                bone(vertices, joints[start + i], joints[start + i + 1]);
        }
    }
}
