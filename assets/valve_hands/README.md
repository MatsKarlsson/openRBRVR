# Valve OpenXR glove assets

These are offline-converted versions of Valve's left/right OpenXR glove meshes
and the supplied red glove texture. Models and textures are copyright Valve
Corporation and distributed under the [included license](LICENSE). They are
separate from the application's MPL-2.0 code. Preserve this license when
redistributing the source or plugin binary, including modified assets.

Source: [ValveSoftware/openxr_engine_plugins, assets/valve_hand_models](https://github.com/ValveSoftware/openxr_engine_plugins/tree/560de2fcab62952d8d6e00760ec6d95eae0f5fd3/assets/valve_hand_models).

Pinned commit: `560de2fcab62952d8d6e00760ec6d95eae0f5fd3`.

| Source file | SHA256 |
| --- | --- |
| `openxr_glove_left_model_slim.glb` | `135189e6a2caab7d5fa0182feda4c2c7b1a9485bc5a7b27b0fe7b5b5ed7e33eb` |
| `openxr_glove_right_model_slim.glb` | `d7d207229f1878c83742db31a111de44f7cf3136d03a3e7bf1f9c786bc30d550` |
| `textures/vr_glove_color_red.jpg` | `a424f719450ca9bce29ba28e90712dab4e40b56e67642150379b5359475eb868` |

Changes: extract the mesh, normals, UVs, skin weights and inverse bind matrices
from each GLB; remap the 26 named bones to `XrHandJointEXT` order; normalize skin
weights; resize the red texture from 4096 to 512 pixels, generate all ten mip
levels, and encode opaque BC1 blocks. The original geometry and UVs are retained.
Normal maps and PBR materials are not used by the D3D9 fixed-function renderer.
The `.inc` files are compiled into the DLL. No model importer, Python, Pillow or
external hand asset files are required to build or run the plugin.

To regenerate, download the three files above and `LICENSE` from the pinned
source into an ignored directory such as `zig-out/valve-source`. Place the
texture alongside the GLBs using its basename `vr_glove_color_red.jpg`. With
Python 3 and Pillow installed, run from the repository root:

```text
python tools/import_valve_hands.py zig-out/valve-source assets/valve_hands
```

The converter prints source hashes, vertex/triangle counts and texture size.
Each hand has 3028 vertices and 4899 triangles. The shared mipmapped texture
occupies 174776 bytes on the GPU. Skinning happens once per frame on the CPU;
all VR views reuse the same indexed mesh.

`zig build hand-preview --release=fast` runs a real 32-bit D3D9 smoke test without
a headset, using a hidden window. It writes `zig-out/valve-gloves.bmp`: left hand
above right, with back, palm and curled views. An optional output path follows
`--`. This checks the assets, skinning, texture upload and indexed draw, but
does not establish fit to a particular player's hands or in-game stereo/depth.
