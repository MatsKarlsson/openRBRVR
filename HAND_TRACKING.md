# Experimental OpenXR hand visuals

This implements the visual-only first version: procedural palms and fingers,
no gestures or gameplay input, default off, cockpit driving only. OpenVR is
unaffected. Enable `[OpenXR] handTracking = true` with `runtime = 'openxr'`, or
use and save the OpenXR menu toggle.

## Runtime support

Both extension enumeration and `XrSystemHandTrackingPropertiesEXT` must report
support. Tracker creation and joint-location errors are optional failures:
hands hide while the rest of VR continues. A creation failure is retried only
after toggling the feature off/on or restarting the OpenXR session. Location
failures recover on the next successful frame; repeated errors do not log every
frame.

| Runtime/device path | 32-bit extension check | Headset tracking/rendering |
| --- | --- | --- |
| Installed VDXR / Virtual Desktop path on this development PC | A separate 32-bit loader probe on 2026-10-08 exposed `XR_EXT_hand_tracking` and `XR_EXT_hand_tracking_data_source`; instance creation succeeded | A later RBR stage run detected Quest 3, reported system hand support and created both trackers. Joint calls succeeded but both hands stayed inactive, with zero mesh vertices. Active joints and rendering remain unverified. |
| Meta/Oculus PC runtime with Quest 3 / Link | Not verified on this PC | Not verified; initial target, no compatibility claim yet |
| Runtime without `XR_EXT_hand_tracking` or system hand support | Covered by mocked tests | Feature remains unavailable; no trackers or visuals |

The active 32-bit runtime registry entry during the probe was
`C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr-32.json`.
No runtime configuration was changed.

For Virtual Desktop, enable **Settings -> Streaming -> Advanced Options ->
Forward tracking data to PC** in the Quest app. The [VDXR developer describes
this opt-in requirement](https://community.khronos.org/t/handjointlocations-all-wrong/111702/7).
Working Quest menu gestures do not establish that PC forwarding is enabled.
The 2026-10-08 21:03 stage log confirmed that the feature was on, head views were
valid and cockpit rendering was eligible, but no active bare-hand joints were
available to generate geometry. Checking forwarding is the next hardware step.

The application uses the core [Khronos hand-tracking API](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrLocateHandJointsEXT.html).
When available, [hand-tracking data-source selection](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrHandTrackingDataSourceInfoEXT.html)
requests unobstructed hands and rejects controller-derived skeletons. With only
the core extension, the runtime determines the source; that API alone cannot
distinguish controller emulation from bare-hand joints. The plugin does not
generate fallback poses.

## Lifecycle and rendering

- Optional instance extensions: `XR_EXT_hand_tracking`, plus
  `XR_EXT_hand_tracking_data_source` when exposed. They are enabled at instance
  creation to allow live toggling. If instance creation fails, it is retried
  without these optional extensions.
- Required entry points: `xrCreateHandTrackerEXT`, `xrDestroyHandTrackerEXT`,
  `xrLocateHandJointsEXT`, loaded through `xrGetInstanceProcAddr`.
- Capability query: `XrSystemProperties` chained to
  `XrSystemHandTrackingPropertiesEXT`.
- Per hand: `XrHandTrackerEXT`, `XrHandTrackerCreateInfoEXT` with
  `XR_HAND_JOINT_SET_DEFAULT_EXT`, and 26 `XrHandJointLocationEXT` records.
- Per frame: `XrHandJointsLocateInfoEXT` uses the current, possibly recentered
  LOCAL space and the same prediction-dampened display time as the views.
  `XrHandJointLocationsEXT::isActive`, valid joint flags and finite data gate
  visibility. Optional data-source state also gates bare-hand visibility.
- Tracking data is cleared before each frame; invalid head views also suppress
  hands. Trackers are destroyed on disable and before session destruction,
  including automatic session restarts. Partial creation is cleaned up.
- Mesh vertices remain in OpenXR reference-space metres. Eye pose and projection
  are applied once, with no extra game-camera or horizon-lock transform.
  The same geometry feeds stereo, quad views and DXVK fixed-function multiview.
- The draw occurs after the game's eye scene, before its render target is
  finished. It keeps cockpit depth, uses the actual reversed-Z comparison,
  and restores D3D9 state with an all-state block. Draw failure is nonfatal.
- There are no mesh assets, persistent hand GPU buffers, action bindings,
  controller input changes, wheel anchors or IK.

The implementation is split into `HandTracking` (capability/lifecycle/joints),
`HandMesh` (procedural triangles) and the OpenXR frame/draw integration.

## Validation

### In-game diagnostics

The OpenXR menu's `Hand input` line shows the PC runtime's live left/right
status, including in menus where hand drawing is intentionally hidden. The
plugin writes `Plugins/openRBRVR-hands.log` anew on each launch. While hand
visuals are enabled, it logs joint status and cockpit/frame eligibility every
five seconds, and successful draw status or errors at the same limited rate.
Tracker creation/capability errors are also recorded. This distinguishes no
PC hand data (`inactive`) from invalid joints and graphics failures. No joint
coordinates are written.

The hand renderer supports an existing D3D9 scene: if DXVK reports that RBR has
already begun one, it borrows that scene and leaves it open. A scene opened by
the hand renderer is closed by the renderer. This ownership behavior is covered
by the mock tests.

### Automated and headset checks

Run `zig build test` for the 32-bit mock-runtime tests. These cover absent
extensions, unsupported systems, property-query/function-loading failures,
partial tracker creation and cleanup, retry on toggle, shutdown, independent
hand loss/reacquisition, locate failures, invalid/nonfinite joints, controller
source rejection, recentered space/time propagation, finite mesh geometry and
configuration save/load/copy/default behavior. No headset is needed.

Run `zig build --release=fast` for the RBR plugin. Output is
`zig-out/bin/openRBRVR.dll`; runtime dependencies are unchanged.

The following still require RBR and a connected headset:

1. Enable hands and verify both palms and all fingers in a cockpit driving view.
2. Occlude each hand, then reacquire it. Verify there are no frozen/fallback hands.
3. Recenter, move the seat, change world scale, and enable horizon lock. Check
   that tracked hands follow the physical hands without doubled head motion.
4. Pause, enter menus, switch to external cameras and play a replay. Hands should
   hide in all of these contexts; returning to cockpit driving should recover.
5. Compare depth against the wheel/dashboard; check both eyes, MSAA, stereo,
   quad views, multiview and BTB stages. Verify later game/overlay draws retain
   their normal graphics state.
6. Toggle hands, restart the OpenXR session and exit/relaunch. Check stability,
   tracker cleanup and frame time with the feature on versus off.
7. Run on an actual runtime without hand tracking, with the setting both on and
   off, to confirm startup and ordinary rendering continue normally.

These automated checks and the extension probe do not establish visual quality,
in-game performance or working Quest hand tracking. Those remain hardware tests.
