# BVH scenes

Derive concrete scenes from `vkz::bvh::scene` in `<vulkanizer/bvh.hpp>`.
Implement `geometry_count()`, `geometry_at()`, `instance_count()`, and
`instance_at()`. The BVH consumes these interfaces without accessing mesh
serialization, materials, or vertex input declarations.

Each geometry describes indexed float3 triangles using borrowed GPU buffers,
byte offsets, vertex stride/count, and triangle count. Indices are uint32 and
relative to the geometry's first vertex. Both buffers require acceleration
build input and device address usage. Override `validate_geometry()` to check
CPU indices or other scene-specific constraints before a full build.

Each instance supplies its geometry index, affine model transform, visibility
mask, SBT offset, and resolved Vulkan instance flags. Concrete scenes decide
opacity and facing policy. Instance order determines the Vulkan custom index;
geometry order determines the IDs accepted by `blas()`.

`scene::bvh_build_options` and `scene::bvh_instance_options` are defined in the
scene contract. Set `build_options` on the scene and `options` on each returned
instance. LOD selection belongs to the concrete scene's geometry implementation.

```cpp
acceleration.build(command_buffer, concrete_scene);
acceleration.update_instances(command_buffer, concrete_scene);
acceleration.rebuild(command_buffer, concrete_scene);
```

The existing overloads accepting `mesh::data`, geometry buffers, and options
remain available. They delegate through `mesh::scene_adapter`, which resolves
the existing interleaved stream, selected LOD, material inheritance, opacity,
and culling policy. The original namespace-level option names remain aliases
for source compatibility. `vkz::bvh` is the primary type;
`vkz::mesh::bvh` remains a compatibility alias. The adapter borrows mesh data
and instance options.

Updates preserve the TLAS handle and require unchanged geometry, instance
count, and instance-to-geometry assignments. Material policy, transforms,
masks, and SBT offsets can change. Changes to vertex/index contents require a
full build even if the layout stays the same. Concrete scenes must change each
geometry's `content_revision` when they modify GPU geometry. The mesh adapter
computes a revision from the selected LOD's CPU indices and mesh positions;
GPU-only edits still require an explicit rebuild. Incompatible instance updates
throw in both Debug and Release, allowing callers to recover with `build()`.

`rebuild()` supports GPU geometry edits with unchanged buffer layouts, triangle
counts, and instance-to-geometry assignments. It performs full BLAS/TLAS builds
into the existing storage, preserving handles and descriptors. Synchronize
previous readers before recording it, increment geometry content revisions,
and make producer writes visible to acceleration-structure build reads. Use
`build()` when buffer handles or layouts change. Adaptive clients may pad unused
triangle slots with degenerate triangles to keep the layout stable.

Each recorded update owns an instance snapshot. Uploads up to 64 KiB are
copied into command-buffer storage; larger uploads use distinct staging
allocations retained by the BVH. Multiple updates can be recorded before
submission. Submissions that update or read the same TLAS must execute in
order; callers must provide semaphore synchronization across queues.

Rebuilding retains previous generations, including their handles and uploads.
Move assignment also retains the destination's previous resources. A full
rebuild replaces the active handles, so refresh descriptors for subsequent
recordings. Old descriptors can still serve already-recorded work until cleanup.

`release_completed_resources()` waits for submitted GPU work, then frees retired
generations and staging snapshots while preserving the active BVH. Call it
periodically after large instance uploads or repeated rebuilds to bound retained
memory. `destroy()` and the destructor wait before releasing all owned resources.
Submit or discard recordings referencing those resources before cleanup or
destruction, and do not subsequently submit them again. Device-idle waits do not
complete commands that have not been submitted.

Geometry buffers, their allocator, and devices remain caller-owned and must
outlive the BVH and its retained generations. Access to the BVH and its queues
must be externally synchronized during waiting and cleanup.

## GPU-driven instances

A scene can override `gpu_instances()` and `record_instances(cmd, destination, blas)` to generate TLAS records directly from GPU placement buffers. `instance_count()` is the reserved record count; the callback must write every `VkAccelerationStructureInstanceKHR` record, with a valid affine transform, 24-bit custom/SBT indices, and references to the supplied BLAS addresses. Empty slots use a zero acceleration-structure reference and mask. `instance_at()` is unused for these scenes.

Vulkanizer allocates a storage/build-input instance buffer and synchronizes compute/transfer writes before building the TLAS. The same path is used for full builds, handle-preserving rebuilds, and instance updates. Changing the instance capacity or geometry layout requires `build()`; callers must increment geometry content revisions when vertex/index contents change. Handle-preserving rebuilds rebuild only BLASes with changed content revisions.
