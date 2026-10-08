#include "Config.hpp"
#include "D3DSceneScope.hpp"
#include "HandMesh.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << "Failed: " #condition " at line " << __LINE__ << '\n'; \
            std::exit(1);                                                       \
        }                                                                       \
    } while (false)

namespace {
    const auto instance = static_cast<XrInstance>(1);
    const auto session = static_cast<XrSession>(2);
    const auto space = static_cast<XrSpace>(3);
    struct Mock {
        bool supports = true;
        bool missing_function = false;
        bool property_failure = false;
        int create_failure = 0;
        int queries = 0;
        int creates = 0;
        int destroys = 0;
        int locates = 0;
        std::array<bool, 2> active { true, true };
        std::array<bool, 2> locate_failure {};
        int invalid_joint = -1;
        bool nonfinite_position = false;
        bool invalid_orientation = false;
        bool invalid_radius = false;
        bool controller_source = false;
        bool source_active = true;
        bool source_requested = false;
        XrTime time = 0;
        XrSpace space = XR_NULL_HANDLE;
    } mock;

    void fill_joints(XrHandJointLocationEXT* joints, int hand)
    {
        const float x = hand == 0 ? -0.16f : 0.16f;
        for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
            joints[i] = {
                XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,
                { { 0, 0, 0, 1 }, { x, -0.15f, -0.4f } }, 0.008f
            };
        }
        joints[XR_HAND_JOINT_PALM_EXT].radius = 0.03f;
        joints[XR_HAND_JOINT_WRIST_EXT].pose.position.z = -0.36f;
        for (int finger = 0; finger < 5; ++finger) {
            const int start = finger == 0 ? 2 : 6 + (finger - 1) * 5;
            const int count = finger == 0 ? 4 : 5;
            for (int i = 0; i < count; ++i) {
                joints[start + i].pose.position = { x + (finger - 2) * 0.018f, -0.15f, -0.39f - 0.025f * i };
            }
        }
    }

    XrResult XRAPI_CALL create_tracker(XrSession actual_session, const XrHandTrackerCreateInfoEXT* info, XrHandTrackerEXT* tracker)
    {
        CHECK(actual_session == session);
        CHECK(info->type == XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT);
        CHECK(info->handJointSet == XR_HAND_JOINT_SET_DEFAULT_EXT);
        ++mock.creates;
        if (mock.creates == mock.create_failure)
            return XR_ERROR_FEATURE_UNSUPPORTED;
        if (info->next) {
            const auto& source = *static_cast<const XrHandTrackingDataSourceInfoEXT*>(info->next);
            CHECK(source.type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT);
            CHECK(source.requestedDataSourceCount == 1);
            CHECK(source.requestedDataSources[0] == XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT);
            mock.source_requested = true;
        }
        *tracker = static_cast<XrHandTrackerEXT>(info->hand == XR_HAND_LEFT_EXT ? 10 : 11);
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL destroy_tracker(XrHandTrackerEXT tracker)
    {
        CHECK(tracker == static_cast<XrHandTrackerEXT>(10) || tracker == static_cast<XrHandTrackerEXT>(11));
        ++mock.destroys;
        return XR_SUCCESS;
    }

    XrResult XRAPI_CALL locate_joints(XrHandTrackerEXT tracker, const XrHandJointsLocateInfoEXT* info, XrHandJointLocationsEXT* locations)
    {
        const int hand = tracker == static_cast<XrHandTrackerEXT>(10) ? 0 : 1;
        CHECK(info->type == XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT);
        CHECK(locations->type == XR_TYPE_HAND_JOINT_LOCATIONS_EXT);
        CHECK(locations->jointCount == XR_HAND_JOINT_COUNT_EXT);
        ++mock.locates;
        mock.time = info->time;
        mock.space = info->baseSpace;
        if (mock.locate_failure[hand])
            return XR_ERROR_RUNTIME_FAILURE;
        fill_joints(locations->jointLocations, hand);
        locations->isActive = mock.active[hand];
        if (mock.invalid_joint >= 0)
            locations->jointLocations[mock.invalid_joint].locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if (mock.nonfinite_position)
            locations->jointLocations[0].pose.position.x = std::numeric_limits<float>::quiet_NaN();
        if (mock.invalid_orientation)
            locations->jointLocations[0].pose.orientation = {};
        if (mock.invalid_radius)
            locations->jointLocations[0].radius = std::numeric_limits<float>::infinity();
        if (locations->next) {
            auto& source = *static_cast<XrHandTrackingDataSourceStateEXT*>(locations->next);
            CHECK(source.type == XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT);
            source.isActive = mock.source_active;
            source.dataSource = mock.controller_source ? XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT : XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
        }
        return XR_SUCCESS;
    }

    void update(HandTracking& hands, bool enabled = true)
    {
        hands.update(session, space, 123456, enabled);
    }

    void test_capabilities()
    {
        HandTracking hands;
        mock = {};
        hands.initialize(instance, 1, false, false);
        update(hands);
        CHECK(!hands.supported() && !hands.enabled() && mock.queries == 0 && mock.creates == 0);
        mock.supports = false;
        hands.initialize(instance, 1, true, false);
        update(hands);
        CHECK(!hands.supported() && mock.creates == 0);
        mock = {};
        mock.property_failure = true;
        hands.initialize(instance, 1, true, false);
        CHECK(!hands.supported());
        mock = {};
        mock.missing_function = true;
        hands.initialize(instance, 1, true, false);
        update(hands);
        CHECK(!hands.supported() && mock.creates == 0);
    }

    void test_lifecycle()
    {
        mock = {};
        HandTracking hands;
        hands.initialize(instance, 1, true, false);
        CHECK(hands.supported());
        hands.update(XR_NULL_HANDLE, space, 123456, true);
        hands.update(session, XR_NULL_HANDLE, 123456, true);
        hands.update(session, space, 0, true);
        CHECK(mock.creates == 0);
        mock.create_failure = 2;
        update(hands);
        CHECK(!hands.enabled() && mock.creates == 2 && mock.destroys == 1);
        update(hands);
        CHECK(mock.creates == 2); // Optional failures must not retry/log each frame.
        update(hands, false);
        mock.create_failure = 0;
        update(hands);
        CHECK(hands.enabled() && mock.creates == 4);
        update(hands, false);
        CHECK(!hands.enabled() && mock.destroys == 3 && !hands.hands()[0].active);
        update(hands);
        CHECK(hands.enabled() && mock.creates == 6);
        hands.shutdown();
        CHECK(mock.destroys == 5 && !hands.supported() && !hands.enabled());
        hands.shutdown();
        CHECK(mock.destroys == 5);
    }

    void test_visibility()
    {
        mock = {};
        HandTracking hands;
        hands.initialize(instance, 1, true, true);
        update(hands);
        CHECK(mock.source_requested && mock.time == 123456 && mock.space == space);
        CHECK(hands.hands()[0].active && hands.hands()[1].active);
        CHECK(hands.status() == "L: tracked, R: tracked");
        mock.active[0] = false;
        update(hands);
        CHECK(!hands.hands()[0].active && hands.hands()[1].active);
        CHECK(hands.status() == "L: inactive, R: tracked");
        mock.active[0] = true;
        update(hands);
        CHECK(hands.hands()[0].active && mock.creates == 2);
        mock.locate_failure[1] = true;
        update(hands);
        CHECK(hands.hands()[0].active && !hands.hands()[1].active);
        mock.locate_failure[1] = false;
        update(hands);
        CHECK(hands.hands()[1].active);
        mock.invalid_joint = XR_HAND_JOINT_LITTLE_TIP_EXT;
        update(hands);
        CHECK(!hands.hands()[0].active && !hands.hands()[1].active);
        CHECK(hands.hands()[0].invalid_joint == XR_HAND_JOINT_LITTLE_TIP_EXT);
        mock.invalid_joint = -1;
        mock.nonfinite_position = true;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.nonfinite_position = false;
        mock.invalid_orientation = true;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.invalid_orientation = false;
        mock.invalid_radius = true;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.invalid_radius = false;
        mock.controller_source = true;
        update(hands);
        CHECK(!hands.hands()[0].active && !hands.hands()[1].active);
        mock.controller_source = false;
        mock.source_active = false;
        update(hands);
        CHECK(!hands.hands()[0].active);
        mock.source_active = true;
        const auto recentered_space = static_cast<XrSpace>(4);
        hands.update(session, recentered_space, 234567, true);
        CHECK(hands.hands()[0].active && mock.space == recentered_space && mock.time == 234567);
        hands.hide();
        CHECK(!hands.hands()[0].active && hands.hands()[0].joints[0].locationFlags == 0);
        update(hands);
        CHECK(hands.hands()[0].active);
        hands.shutdown();

        // The core extension alone is sufficient; no data-source chain required.
        hands.initialize(instance, 1, true, false);
        update(hands);
        CHECK(hands.hands()[0].active && hands.hands()[1].active);
        hands.shutdown();
    }

    void test_mesh()
    {
        HandTracking::Hand hand;
        std::vector<HandVertex> vertices;
        append_hand_mesh(hand, vertices);
        CHECK(vertices.empty());
        fill_joints(hand.joints.data(), 0);
        hand.active = true;
        append_hand_mesh(hand, vertices);
        CHECK(vertices.size() > 1000 && vertices.size() < 6000 && vertices.size() % 3 == 0);
        for (const auto& vertex : vertices) {
            CHECK(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z));
            CHECK(vertex.position.x > -0.23f && vertex.position.x < -0.09f);
            CHECK(vertex.position.y > -0.18f && vertex.position.y < -0.12f);
            CHECK(vertex.position.z > -0.51f && vertex.position.z < -0.34f);
            CHECK((vertex.color & 0xff000000) == 0xff000000);
        }
        const size_t one_hand = vertices.size();
        fill_joints(hand.joints.data(), 1);
        append_hand_mesh(hand, vertices);
        CHECK(vertices.size() == one_hand * 2);
        for (size_t i = 0; i < one_hand; ++i) {
            CHECK(std::abs(vertices[i + one_hand].position.x - vertices[i].position.x - 0.32f) < 0.00001f);
        }
        // Collapsed bones and zero radii must not produce NaNs.
        vertices.clear();
        for (auto& joint : hand.joints) {
            joint.pose.position = { 0, 0, -0.5f };
            joint.radius = 0;
        }
        append_hand_mesh(hand, vertices);
        for (const auto& vertex : vertices) {
            CHECK(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z));
        }
        vertices.clear();
        hand.joints[XR_HAND_JOINT_INDEX_PROXIMAL_EXT].pose.position.y = 0.1f;
        append_hand_mesh(hand, vertices);
        for (const auto& vertex : vertices) {
            CHECK(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z));
        }
    }

    void test_config()
    {
        Config config {};
        CHECK(!config.openxr_hand_tracking);
        Config changed = config;
        changed.openxr_hand_tracking = true;
        CHECK(!(changed == config));
        config = changed;
        CHECK(config.openxr_hand_tracking && config == changed);
        const auto path = std::filesystem::temp_directory_path() / std::format("openRBRVR-hand-test-{}.toml", GetCurrentProcessId());
        config.runtime = OPENXR;
        CHECK(config.write(path));
        auto loaded = Config::from_toml(path);
        CHECK(loaded.openxr_hand_tracking && loaded.runtime == OPENXR);
        config.openxr_hand_tracking = false;
        CHECK(config.write(path));
        CHECK(!Config::from_toml(path).openxr_hand_tracking);
        {
            std::ofstream legacy(path);
            legacy << "runtime = 'openxr'\n[OpenXR]\nworldScale = 1000\n";
        }
        CHECK(!Config::from_toml(path).openxr_hand_tracking);
        std::filesystem::remove(path);
    }

    void test_scene_ownership()
    {
        struct Device {
            bool in_scene = false;
            int begins = 0;
            int ends = 0;
            HRESULT failure = D3D_OK;
            HRESULT BeginScene()
            {
                ++begins;
                if (FAILED(failure))
                    return failure;
                if (in_scene)
                    return D3DERR_INVALIDCALL;
                in_scene = true;
                return D3D_OK;
            }
            HRESULT EndScene()
            {
                CHECK(in_scene);
                ++ends;
                in_scene = false;
                return D3D_OK;
            }
        } device;

        {
            D3DSceneScope scene(&device);
            CHECK(scene.valid() && scene.owned() && device.in_scene);
            CHECK(scene.finish() == D3D_OK && !device.in_scene);
        }
        CHECK(device.begins == 1 && device.ends == 1);
        device.in_scene = true;
        {
            D3DSceneScope scene(&device);
            CHECK(scene.valid() && !scene.owned());
            CHECK(scene.finish() == D3D_OK && device.in_scene);
        }
        CHECK(device.in_scene && device.ends == 1); // Never close RBR's scene.
        device.in_scene = false;
        device.failure = D3DERR_DEVICELOST;
        {
            D3DSceneScope scene(&device);
            CHECK(!scene.valid() && !scene.owned());
        }
        CHECK(device.ends == 1);
        device.failure = D3D_OK;
        {
            D3DSceneScope scene(&device);
            CHECK(scene.valid());
        }
        CHECK(!device.in_scene && device.ends == 2); // Early-return cleanup.
    }
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance actual_instance, XrSystemId system, XrSystemProperties* properties)
{
    CHECK(actual_instance == instance && system == 1);
    ++mock.queries;
    if (mock.property_failure)
        return XR_ERROR_RUNTIME_FAILURE;
    auto& hand = *static_cast<XrSystemHandTrackingPropertiesEXT*>(properties->next);
    CHECK(hand.type == XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT);
    hand.supportsHandTracking = mock.supports;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance actual_instance, const char* name, PFN_xrVoidFunction* function)
{
    CHECK(actual_instance == instance);
    *function = nullptr;
    if (mock.missing_function)
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    if (std::string_view(name) == "xrCreateHandTrackerEXT")
        *function = reinterpret_cast<PFN_xrVoidFunction>(create_tracker);
    if (std::string_view(name) == "xrDestroyHandTrackerEXT")
        *function = reinterpret_cast<PFN_xrVoidFunction>(destroy_tracker);
    if (std::string_view(name) == "xrLocateHandJointsEXT")
        *function = reinterpret_cast<PFN_xrVoidFunction>(locate_joints);
    CHECK(*function);
    return XR_SUCCESS;
}

int main()
{
    test_capabilities();
    test_lifecycle();
    test_visibility();
    test_mesh();
    test_config();
    test_scene_ownership();
    std::cout << "Hand tests passed: capabilities, lifecycle, visibility, mesh, configuration, D3D scene ownership\n";
}
