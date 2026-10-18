# TODO

## BVH

- [x] Give instance uploads distinct storage until GPU completion, including multiple updates recorded before submission.
- [x] Retain old generations across rebuild and move assignment; wait before cleanup/destruction and document descriptor refresh.
- [x] Track geometry-content revisions, hash CPU mesh geometry, and document GPU-only edits that require rebuilding.
- [x] Separate GPU resource allocation, upload snapshots, and command recording into internal helpers.
- [x] Cover small and large upload snapshots, separate submissions, rebuild/move before submission, cleanup, and destruction with submitted GPU work.
