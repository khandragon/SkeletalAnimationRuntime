# Data-Driven Skeletal Animation Runtime

A C++ / OpenGL skeletal animation runtime focused on engine-level animation systems: glTF loading, skeleton evaluation, animation sampling, GPU skinning, cross-fading, data-driven animation graphs, 1D blend trees, root motion, multithreaded crowd animation, profiling, and debug visualization.

The goal of this project was not just to play an animation clip. The goal was to build the core runtime pieces that a small game engine would need in order to load a skinned character, evaluate animation data, blend poses, drive animation from external JSON data, and profile performance across many animated characters.

### Prerequisites

Install the following before configuring the project:

- **Git**
- **CMake 3.28 or newer**
- **Visual Studio** with the **Desktop development with C++** workload
- A Windows SDK installed through Visual Studio
- **vcpkg**
- A GPU and graphics driver with **OpenGL 4.5** support

You can verify CMake with:

```powershell
cmake --version
```

### Clone the Repository

```powershell
git clone <repository-url>
cd SkeletalAnimationRuntime
```

Replace `<repository-url>` with the URL of this repository.

### Set Up vcpkg

If vcpkg is not already installed:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

For the current PowerShell session:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
```

If vcpkg is installed somewhere else, set `VCPKG_ROOT` to that location instead.

### Install Dependencies

If the repository contains a `vcpkg.json` manifest, install the manifest dependencies with:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
```

If the repository does **not** contain a `vcpkg.json` manifest, install the dependencies directly:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install `
    glfw3 `
    glad `
    glm `
    "imgui[glfw-binding,opengl3-binding]" `
    nlohmann-json `
    tinygltf `
    --triplet x64-windows
```

### Configure

Run the configure step from the repository root:

```powershell
cmake -S . -B build/windows-debug `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
```

This generates the native build files and allows the `find_package(...)` calls in `CMakeLists.txt` to resolve the vcpkg dependencies.

> If `cmake --build` reports that `ALL_BUILD.vcxproj` or another generated project file is missing, run the configure command above first.

### Build

Build the Debug configuration:

```powershell
cmake --build build/windows-debug --config Debug
```

The executable will be generated at:

```text
build/windows-debug/Debug/SkeletalAnimationRuntime.exe
```

For a Release build:

```powershell
cmake --build build/windows-debug --config Release
```

The Release executable will be generated at:

```text
build/windows-debug/Release/SkeletalAnimationRuntime.exe
```

### Run

From the repository root, run the Debug build with:

```powershell
.\build\windows-debug\Debug\SkeletalAnimationRuntime.exe
```

Or run the Release build with:

```powershell
.\build\windows-debug\Release\SkeletalAnimationRuntime.exe
```

The CMake post-build step automatically copies the repository's `assets` directory beside the executable, so the animation graph and glTF assets are available to the runtime without a separate copy step.

### Rebuild After Source Changes

For normal C++ source changes, you usually only need to rebuild:

```powershell
cmake --build build/windows-debug --config Debug
```

Re-run the configure step when you change dependency configuration, CMake settings, source-file lists in `CMakeLists.txt`, the generator/toolchain, or the build directory.

## Controls

| Input       | Action                 |
| ----------- | ---------------------- |
| `1`         | Set speed to 0.0       |
| `2`         | Set speed to 1.5       |
| `3`         | Set speed to 4.0       |
| Right mouse | Look around            |
| `W/A/S/D`   | Move camera            |
| `Q/E`       | Move camera down/up    |
| Shift       | Faster camera movement |
| Escape      | Close application      |

## Summary

This project implements a data-driven skeletal animation runtime rather than a one-off animation demo. It covers the full path from glTF mesh and skeleton loading, to animation sampling, pose blending, GPU skinning, graph-driven playback, root motion, blend trees, debug visualization, profiling, multithreaded crowd updates, and performance scaling.

The final result supports data-driven 1D locomotion blend trees, allowing a single `speed` parameter to smoothly blend between survey/idle, walk, and run animations.

## Features

- Loads skinned glTF/glb character meshes.
- Parses skeleton hierarchy, joint names, parent indices, bind transforms, and inverse bind matrices.
- Samples translation, rotation, and scale animation channels.
- Evaluates local joint poses into global joint matrices.
- Performs GPU skinning in the vertex shader.
- Supports idle/walk/run clip playback.
- Supports cross-fading between animation clips.
- Supports a data-driven JSON animation graph.
- Supports parameter-driven transitions using simple conditions such as `speed > 0.1`.
- Supports data-driven 1D locomotion blend trees.
- Supports root motion extraction from a selected root/hip joint.
- Supports root motion debug path rendering.
- Supports debug skeleton rendering over the skinned mesh.
- Supports joint markers, selected joint inspection, bind pose/current pose toggles, and forward vector visualization.
- Supports a movable debug camera.
- Supports profiling panels for animation update, local-to-global pose evaluation, joint matrix upload, rendering, and total frame time.
- Supports crowd tests with 1, 10, 100, and 500 animated characters.
- Supports multithreaded animation update using a CPU job system.
- Includes optimization work such as persistent pose storage, keyframe index caching, SIMD pose blending, and avoiding repeated allocations.

## Architecture

The runtime is split into several small systems:

```txt
src/
├── animation/
│   ├── AnimationClip.*
│   ├── AnimationGraph.*
│   ├── Animator.*
│   ├── Pose.*
│   ├── Skeleton.*
│   └── AnimatedCharacter.*
├── assets/
│   └── GltfLoader.*
├── core/
│   ├── JobSystem.h
│   └── Timer.h
├── math/
│   ├── Transform.h
│   └── SimdTransform.h
├── render/
│   ├── DebugDraw.*
│   ├── Mesh.*
│   └── Shader.*
└── main.cpp
```

The high-level update flow is:

```txt
Input
Animation graph parameter updates
Animation graph state/blend-tree evaluation
Per-character animation update
Root motion extraction
Joint matrix generation
GPU joint matrix upload
Rendering
Debug UI
```

The animation update is designed so that each `AnimatedCharacter` owns its own `Animator`, pose data, joint matrices, root motion state, and world transform. This makes characters independent and safe to update in parallel.

## glTF Loading

The runtime loads skinned glTF/glb assets and extracts the minimum data needed for skeletal animation:

Mesh attributes:

```txt
POSITION
NORMAL
TEXCOORD_0
JOINTS_0
WEIGHTS_0
indices
```

Skeleton data:

```cpp
struct Joint
{
    std::string name;
    int parent = -1;
    glm::mat4 inverseBindMatrix;
    Transform bindLocalTransform;
};
```

The loader reads:

- glTF skin joint list.
- Inverse bind matrices.
- Node hierarchy.
- Node local TRS transforms.
- Animation clips and animation channels.

The transform convention follows glTF TRS composition:

```cpp
localMatrix = translate(T) * rotation(R) * scale(S);
```

This is important because the local transform must match how the asset was authored. The global pose is then computed by walking the skeleton hierarchy:

```cpp
if (parent < 0)
    global[joint] = localMatrix;
else
    global[joint] = global[parent] * localMatrix;
```

## Animation Sampling

Animation clips are represented as channels:

```cpp
enum class ChannelPath
{
    Translation,
    Rotation,
    Scale
};

struct AnimationChannel
{
    int jointIndex = -1;
    ChannelPath path;
    std::vector<float> times;
    std::vector<glm::vec4> values;
};
```

Each frame starts from the skeleton bind pose, then animated channels overwrite the relevant local joint transforms. This keeps non-animated joints stable and prevents missing channels from producing invalid poses.

Sampling behavior:

- Translation uses linear interpolation.
- Scale uses linear interpolation.
- Rotation uses quaternion interpolation.
- Time wraps for looping clips.
- Time clamps for non-looping clips.
- Each channel caches its last keyframe index to avoid scanning from the beginning every frame.

The keyframe cache is stored per `Animator`, not inside the shared `AnimationClip`, so multiple characters can safely evaluate the same clip on different threads.

## Pose Blending

The runtime supports clip-to-clip cross-fading and blend-tree pose blending.

A pose contains local joint data and global joint matrices. The optimized pose layout stores local data in separate arrays:

```cpp
struct Pose
{
    std::vector<glm::vec3> translations;
    std::vector<glm::quat> rotations;
    std::vector<glm::vec3> scales;
    std::vector<glm::mat4> global;
};
```

This layout is more cache-friendly than storing one large `Transform` object per joint and makes SIMD blending easier.

Pose blending is performed per joint:

```cpp
translation = lerp(a.translation, b.translation, weight);
rotation    = normalize(nlerp_or_slerp(a.rotation, b.rotation, weight));
scale       = lerp(a.scale, b.scale, weight);
```

After local pose blending, the runtime recomputes the global pose so skinning receives the final blended joint transforms.

## Skinning

The runtime uses GPU skinning. Each frame, the CPU computes one skinning matrix per joint:

```cpp
jointMatrix[j] = pose.global[j] * skeleton.joints[j].inverseBindMatrix;
```

The vertex shader blends the joint matrices using the vertex joint indices and weights:

```glsl
mat4 skin =
    weights.x * jointMatrices[joints.x] +
    weights.y * jointMatrices[joints.y] +
    weights.z * jointMatrices[joints.z] +
    weights.w * jointMatrices[joints.w];

vec4 skinnedPosition = skin * vec4(inPosition, 1.0);
```

This moves vertex deformation to the GPU while keeping animation evaluation and pose generation on the CPU.

## Data-Driven Animation Graph

The animation graph is loaded from external JSON. This allows animation behavior to change without recompiling the program.

The graph supports:

- Clip states.
- Parameter values.
- Conditional transitions.
- Blend times.
- 1D blend tree states.

Example transition-style graph:

```json
{
  "parameters": {
    "speed": 0.0
  },
  "states": {
    "idle": { "clip": "Survey", "loop": true },
    "walk": { "clip": "Walk", "loop": true },
    "run": { "clip": "Run", "loop": true }
  },
  "transitions": [
    {
      "from": "idle",
      "to": "walk",
      "condition": "speed > 0.1",
      "blend_time": 0.25
    }
  ]
}
```

The final locomotion setup uses a 1D blend tree:

```json
{
  "character": "fox",
  "model": "assets/characters/Fox.glb",
  "initial_state": "locomotion",

  "parameters": {
    "speed": 0.0
  },

  "states": {
    "locomotion": {
      "type": "blend_tree_1d",
      "parameter": "speed",
      "motions": [
        { "clip": "Survey", "position": 0.0 },
        { "clip": "Walk", "position": 1.5 },
        { "clip": "Run", "position": 4.0 }
      ]
    }
  },

  "transitions": []
}
```

For a speed between two motion positions, the runtime computes:

```cpp
weight = inverseLerp(lowerPosition, upperPosition, speed);
pose = blend(lowerPose, upperPose, weight);
```

This allows the graph to smoothly blend between idle/survey, walk, and run using a single `speed` parameter.

## Root Motion

Root motion is extracted after animation sampling and before joint matrix generation.

The runtime selects a root motion joint, usually the hip/root joint:

```txt
b_Hip_01
Hips
Root
Armature
```

Each frame:

```txt
previous root position
current root position
delta = current - previous
horizontal delta is applied to character.worldPosition
optional horizontal root motion is removed from the pose
global pose is recomputed if the pose was modified
```

This allows the animation to drive the character’s world movement instead of only playing in-place.

The debug UI displays:

- Root joint index/name.
- Root delta this frame.
- Accumulated root motion.
- Root path point count.
- Root path rendering in world space.

## Multithreading

The runtime updates characters independently, which makes animation update work parallelizable.

Main thread:

```txt
Input
Animation graph decisions
Submit animation jobs
Wait for jobs
Upload joint matrices
Render
```

Worker threads:

```txt
Sample clips
Blend poses
Compute global poses
Apply root motion
Generate joint matrices
```

OpenGL calls remain on the main/render thread. Worker threads only touch CPU-side character data.

The update loop follows this structure:

```cpp
for each character:
    submit animation update job

wait for all jobs

for each character:
    submit root motion job

wait for all jobs

for each character:
    submit joint matrix generation job

wait for all jobs

render on main thread
```

This makes it possible to test performance scaling across many animated characters.

## Debug Views

The runtime includes an ImGui debug interface with panels for:

### Animation

- Current state.
- Current clip.
- Current animation time.
- Normalized time.
- Playback speed.
- Loop toggle.
- Speed parameter.
- Blend tree state/motions.

### Blending

- Source clip.
- Target clip.
- Blend weight.
- Blend time.

### Skeleton

- Joint count.
- Selected joint.
- Parent joint.
- Local transform.
- Global matrix.

### Debug Rendering

- Toggle mesh.
- Toggle skeleton.
- Toggle joint markers.
- Toggle root motion path.
- Toggle forward vector.
- Toggle bind pose skeleton.
- Clear root path.

### Root Motion

- Enable/disable root motion.
- Remove horizontal root motion from pose.
- Root joint name/index.
- Delta this frame.
- Accumulated root motion.
- Root path point count.

### Camera

- Right mouse look.
- WASD movement.
- Q/E vertical movement.
- Shift fast movement.
- Adjustable movement speed and mouse sensitivity.

### Performance

- Animation sampling time.
- Local-to-global pose time.
- Animation update time.
- Root motion time.
- Joint matrix generation time.
- Joint matrix upload time.
- Render time.
- Full frame time.

## Profiling Results

The runtime includes CPU timing through scoped timers. Timed regions include:

```txt
Animation graph loading
Skinned mesh loading
Skeleton loading
Animation clip loading
Animation sampling
Local-to-global pose
Root motion
Joint matrix generation
Joint matrix upload
Rendering
Full frame
```

Example profiling panel:

## Performance Scaling

The runtime includes a crowd test to compare 1, 10, 100, and 500 animated characters. Each character owns its own animator, pose data, joint matrices, root motion state, and world transform.

| Characters | Animation Update | Render | Total Frame |

Testing notes:

- Debug skeletons should be disabled for high character counts when measuring animation cost.
- VSync can cap the reported frame time, so profiling with VSync disabled may give clearer CPU timing.
- Uniform joint matrix uploads are simple but not ideal for large crowds.
- A future SSBO/UBO path would reduce per-character uniform upload overhead.

## Important Implementation Details

### Bind pose fallback

Every sampled pose starts from the skeleton bind pose. This matters because glTF clips may only animate some joints or channels.

### Parent-before-child hierarchy

Global pose evaluation assumes parents are evaluated before children. The skeleton loader maintains this ordering.

### Per-animator keyframe caches

Keyframe caches are stored per animator so that many characters can sample the same shared animation clips safely.

### CPU/GPU responsibility split

The CPU evaluates animation poses and produces joint matrices. The GPU performs per-vertex skinning.

### Worker thread safety

Worker threads never call OpenGL. They only modify CPU-side data owned by a single character.

### Root motion order

Root motion is applied after pose evaluation and before joint matrix generation. If horizontal root motion is removed from the pose, the global pose is recomputed before skinning matrices are generated.
