# azdo
AZDO OpenGL techniques including visibility culling and LOD selection inside the GPU

For more information about AZDO, checkout this [video](https://www.youtube.com/watch?v=K70QbvzB6II)

# Description

We implement three techniques in the GPU:

* Hardware instancing: Fast rendering of repeated geometries
  * Single draw call for all instances
    * Avoids API overhead
  * Access per-instance data
    * Use gl_InstanceID inside vertex shader
    * 3x4 transformation matrix
  * Combined with culling
    * gl_InstanceID -> visible buffer -> instance data

* Visibility culling Discard geometries outside frustum and occluded
  * Compute shader
    * Test AABB vs frustum in clip space
    * Compute screen-space 2D AABB and its min-z
    * Fetch max depth from hierarchical-z 2D texture
    * If AABB min-z < max depth, store 1 else 0
  * How to build hi-z map
    * Draw occluders and use depth with mipmapping
    * logN rendering passes gather max of 4 texels=

* Level of detail: Choose discrete levels during rendering
  * Compute shader
    * Gather instances with visible == 1
    * Use distance to camera to choose LOD
    * Write to different output buffer per LOD
    * Keep track of output index with atomic counters
  * Render all LOD at once
    * glMultiDrawElementsIndirect
    * drawCmds[i].baseInstance = i
    * LOD level vertex attrib w/ divisor = # instance + 1

The general rendering algorithm consists of the following steps:

1. Render previously visible
1. Update hierarchical-z mipmaps
1. Test AABBs for visibility and store in current
1. Gather visible in current and not last frames
1. Render newly visible
1. Gather visible in current for next frame
1. Swap current with last

# Results

Here are some images of a test scene consisting solely of cylinders and corresponding performance results:

![scene](https://github.com/potato3d/azdo/blob/main/imgs/fig1.png "scene")
![lod](https://github.com/potato3d/azdo/blob/main/imgs/fig2.png "lod")
![perf](https://github.com/potato3d/azdo/blob/main/imgs/fig3.png "perf")