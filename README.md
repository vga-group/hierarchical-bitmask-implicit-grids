Hierarchical Bitmask Implicit Grids for Efficient Point-in-Volume Queries on the GPU
====================================================================================

Implicit grids are a practical data structure for world-space, scene-wide
spatial lookups. They can be built in sub-millisecond times for hundreds of
thousands of objects and queried very efficiently on the GPU, making them
suitable for light culling and decal rendering, especially when typical
solutions like deferred & clustered shading are not available.

The pre-print of our paper is available [here](https://webpages.tuni.fi/vga/publications/HBIG.html).
This program was used for the benchmarks in the paper.

## Code overview

Despite being by the same group that makes
[Tauray](https://github.com/vga-group/tauray), it is not based on it due to
differences in goals. This is a gutted version of an unrelated codebase that may
be open-sourced in full at some point later on.

Files related to implicit grid implementation:

| Name                            | Purpose                                     |
| ------------------------------- | ------------------------------------------- |
| `gfx/clustering.glsl`           | Lookups from the data structure             |
| `gfx/clustering.comp`           | Builds implicit grid                        |
| `gfx/clustering_hierarchy.comp` | Builds hierarchy layer                      |
| `gfx/clustering_stage.{cc,hh}`  | CPU-side render pass code                   |
| `gfx/decal_ranges.comp`         | Evaluates range of decal volume across axes |
| `gfx/light_ranges.comp`         | Evaluates range of light volume across axes |
| `gfx/decal_order.comp`          | Calculates sorting key for decals           |
| `gfx/light_morton.comp`         | Calculates sorting key for lights           |

The `clustering` term is used only for historical reasons; `binning` would be
better.

## Building

The program has been tested only on Ubuntu 22.04, don't expect a Windows build
to be easy.

```
cmake -S . -B release -DCMAKE_BUILD_TYPE=Release
cmake --build release
````

## Getting the expected scene

We don't include the Sponza scene in the repo, you'll need to download that
separately. You can get the .gltf version from [here](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Sponza),
import it into [Blender](https://www.blender.org/) and export it as a .glb file,
which is expected by this program. Place it into `data/sponza.glb`.

## Running

```
release/ig_benchmark
```

The program will start up as a black window.

Initially, it's set up to benchmark ray traced light culling. By pressing 'f',
you can swap between light culling and decal rendering tests. By pressing 'r',
you can swap between ray tracing and multi-view tests.

You can start the currently selected benchmark with the 't' key. Timing data is
printed in stdout.
