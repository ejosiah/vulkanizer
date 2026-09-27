#pragma once

#include "context.hpp"
#include "memory.hpp"
#include "texture.hpp"
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

/**
 * @brief Sparse virtual shadow maps for a directional light.
 *
 * Record each frame with begin(), capture(), then end() on one graphics and compute
 * queue, in submission order. Externally synchronize host access to this API.
 * Returned Vulkan resources are borrowed and remain owned by the instance.
 */
namespace vkz::svsm {
    /** @brief Shadow-map instance handle. */
    using id = uint32_t;
    /** @brief Maximum supported number of clipmap levels. */
    inline constexpr uint32_t max_clipmaps = 16;

    /**
     * @brief World-space axis-aligned bounds of one independently drawable caster.
     *
     * Bounds must enclose skinning, displacement, and all geometry actually drawn.
     * Visibility and indirect draws use the input record index, not an application ID.
     */
    struct alignas(16) caster_bounds {
        /** @brief World-space center in xyz; w is unused. */
        glm::vec4 center{};
        /** @brief Nonnegative half extents in xyz; w is unused. */
        glm::vec4 half_extent{};
        /** @brief Application metadata, including an optional opaque ID in x. */
        glm::uvec4 metadata{};
    };
    /**
     * @brief Region whose light-space XY projection invalidates pages at every level.
     *
     * Supply both old and new bounds for movement, old bounds for deletion, and new
     * bounds for insertion. Invalidation is independent of receiver depth. Report
     * material or deformation changes even when the bounds themselves are unchanged.
     */
    using invalidation = caster_bounds;
    static_assert(sizeof(caster_bounds) == 48);

    /** @brief Instance capacities and device configuration. */
    struct params {
        /** @brief Device with dynamic rendering and synchronization2 enabled. */
        vkz::device device;
        /** @brief Allocator used for instance resources; must outlive the instance. */
        vma_memory_allocator memory_allocator;
        /** @brief Power-of-two resolution of each virtual clipmap in texels. */
        uint32_t virtual_resolution{8192};
        /** @brief Power-of-two page width in texels, from 8 through 256. */
        uint32_t page_size{128};
        /** @brief Number of levels, from 1 through max_clipmaps. */
        uint32_t clipmap_count{6};
        /** @brief Maximum number of resident physical pages. */
        uint32_t physical_pages{1024};
        /** @brief Maximum number of caster records per frame. */
        uint32_t max_casters{65536};
        /** @brief Number of independently reusable frame slots. */
        uint32_t in_flight_frames{2};
        /** @brief Page rendering budget per frame; coarsest requested pages take priority. */
        uint32_t max_pages_per_frame{1024};
        /** @brief First clipmap's full width in world units; doubles each level. */
        float first_clipmap_extent{32};
        /** @brief Whether valid pages can be reused across frames. */
        bool cache{true};
        /**
         * @brief Assertion that fragmentStoresAndAtomics was enabled on the logical device.
         * @note Must be true. Vulkan cannot query enabled device features retrospectively.
         */
        bool fragment_stores_and_atomics_enabled{false};
    };

    /** @brief Borrowed receiver and caster inputs that must survive GPU execution. */
    struct frame_inputs {
        /** @brief Single-sample sampled 2D receiver depth, already in a shader-readable layout. */
        texture depth;
        /** @brief Inverse of the exact receiver view-projection matrix, including jitter. */
        glm::mat4 inverse_view_projection{1};
        /** @brief World-space camera position used to center clipmaps. */
        glm::vec3 camera_position{};
        /** @brief Nonzero direction in which light travels; normalized internally. */
        glm::vec3 light_direction{0, -1, 0};
        /** @brief Minimum projection onto the normalized light direction; include offscreen casters. */
        float light_depth_min{-1000};
        /** @brief Maximum light projection; changing either depth bound invalidates the cache. */
        float light_depth_max{1000};
        /** @brief Receiver background value; use zero for reverse-Z depth. */
        float background_depth{1};
        /** @brief Sampled receiver channel containing depth, from 0 through 3. */
        uint32_t depth_channel{};
        /** @brief PCF radius from 0 through 4, also used to request neighbouring pages. */
        uint32_t filter_radius{1};
        /** @brief Frame slot; wait for its previous GPU use before reusing it or its inputs. */
        uint32_t frame_slot{};
        /** @brief STORAGE_BUFFER of tightly packed caster_bounds for all potential casters. */
        buffer casters;
        /** @brief Number of input caster records. */
        uint32_t caster_count{};
        /** @brief STORAGE_BUFFER of tightly packed invalidation records. */
        buffer invalidations;
        /** @brief Number of invalidation records. */
        uint32_t invalidation_count{};
        /** @brief Optional STORAGE_BUFFER containing one VkDrawIndexedIndirectCommand per caster. */
        buffer indexed_draws;
        /** @brief Request every virtual page, including when opaque receivers are absent. */
        bool request_all_pages{false};
        /** @brief Invalidate the cache for camera cuts, world rebasing, or unreported scene changes. */
        bool reset_cache{false};
    };

    /** @brief Default opaque pipeline push constants; custom pipelines may use their own layout. */
    struct draw_constants {
        /** @brief Object-to-world transform. */
        glm::mat4 model{1};
        /** @brief Clipmap level supplied by render_context. */
        uint32_t clipmap{};
        /** @brief Caster's index in the input bounds buffer. */
        uint32_t caster_index{};
        /** @brief Nonzero to reject casters using GPU visibility results. */
        uint32_t use_visibility{1};
        /** @brief Normalized depth bias applied before uint conversion; there is no depth attachment. */
        float depth_bias{0.0001f};
    };
    static_assert(sizeof(draw_constants) == 80);

    /** @brief Borrowed per-clipmap state passed to the capture callback during recording. */
    struct render_context {
        /** @brief Command buffer with attachmentless dynamic rendering active. */
        VkCommandBuffer command_buffer{};
        /** @brief Clipmap level being rendered. */
        uint32_t clipmap{};
        /** @brief World-to-clip transform for this level. */
        glm::mat4 view_projection{1};
        /** @brief SVSM descriptor set for the current frame slot. */
        VkDescriptorSet descriptors{};
        /** @brief Default opaque pipeline layout, accepting draw_constants. */
        VkPipelineLayout default_layout{};
        /** @brief GPU uint[clipmap_count][caster_count] visibility, indexed by input record index. */
        buffer visibility;
        /** @brief GPU indirect commands; contents are valid only when templates were supplied. */
        buffer indexed_draws;
        /** @brief Byte offset of this clipmap's VkDrawIndexedIndirectCommand array. */
        VkDeviceSize indexed_draw_offset{};
        /** @brief Number of caster records and indirect commands per clipmap. */
        uint32_t caster_count{};
    };

    /** @brief GPU counter readback layout, containing eight consecutive uint32_t values. */
    struct statistics {
        /** @brief Requested virtual pages. */
        uint32_t requested{};
        /** @brief Pages scheduled for rendering, including pages containing no casters. */
        uint32_t rendered{};
        /** @brief Requested pages reused from the valid cache. */
        uint32_t cached{};
        /** @brief Unserved page requests; coarser sampling may still provide coverage. */
        uint32_t missing{};
        /** @brief Occupied physical pages. */
        uint32_t resident{};
        /** @brief Physical pages reclaimed by eviction. */
        uint32_t evicted{};
        /** @brief Accepted caster and clipmap pairs, not unique objects. */
        uint32_t visible_casters{};
        /** @brief Reserved for future use. */
        uint32_t reserved{};
    };

    /**
     * @brief Allocate a directional-light shadow-map instance.
     * @param params Device configuration and resource capacities.
     * @return Handle owning the allocated shadow-map resources.
     */
    id create(const params &params);
    /**
     * @brief Release an instance and its resources.
     * @param id Instance to destroy.
     * @pre All GPU work using the instance or its borrowed resources has completed.
     */
    void destroy(id id);
    /**
     * @brief Record page requests, cache updates, allocation, and caster culling.
     *
     * Call once per frame outside rendering, in submission order, on one graphics and
     * compute queue. Serializes cache updates against previous sampling on that queue.
     * The caller handles cross-queue ownership transfers and semaphore synchronization.
     * @param id Instance to update.
     * @param command_buffer Recording command buffer, outside any rendering scope.
     * @param inputs Frame inputs whose referenced resources must survive GPU execution.
     * @pre Any previous frame's end() has been recorded and the selected slot is available.
     */
    void begin(id id, VkCommandBuffer command_buffer, const frame_inputs &inputs);
    /**
     * @brief Record caster rendering into scheduled pages for each clipmap.
     *
     * Opens attachmentless dynamic rendering, binds the default opaque pipeline and
     * descriptor set, then invokes the callback for each level. The callback binds
     * geometry and issues direct or indirect draws for all contributors to scheduled
     * pages, including unchanged casters. Default vertices are position vec3 at binding
     * zero with stride sizeof(glm::vec3). Custom pipelines use one sample, no attachments,
     * and shaders/svsm.glsl with descriptor set zero by default.
     * @param id Instance whose pages are being rendered.
     * @param command_buffer Same command buffer passed to begin().
     * @param draw Callback recording application-specific caster draws.
     * @pre begin() has been recorded for this frame.
     */
    void capture(id id, VkCommandBuffer command_buffer, const std::function<void(const render_context &)> &draw);
    /**
     * @brief Publish completed pages and synchronize subsequent shader sampling.
     * @param id Instance whose frame is being completed.
     * @param command_buffer Same command buffer passed to begin() and capture().
     * @pre capture() has been recorded for this frame.
     */
    void end(id id, VkCommandBuffer command_buffer);
    /**
     * @brief Invalidate the cache on the next begin().
     * @param id Instance to invalidate.
     * @note Normally called between frames. Also abandons an unfinished recording;
     * discard that command buffer before calling reset() in that case.
     */
    void reset(id id);

    /**
     * @brief Obtain the layout for custom caster and lighting pipelines.
     * @param id Owning instance.
     * @return Borrowed descriptor set layout.
     */
    VkDescriptorSetLayout descriptor_set_layout(id id);
    /**
     * @brief Obtain a frame slot's SVSM descriptor set.
     * @param id Owning instance.
     * @param frame_slot Slot index below params::in_flight_frames.
     * @return Borrowed set populated by begin(); use for lighting after end().
     */
    VkDescriptorSet descriptor_set(id id, uint32_t frame_slot);
    /**
     * @brief Obtain GPU caster visibility generated by begin().
     * @param id Owning instance.
     * @param frame_slot Slot index below params::in_flight_frames.
     * @return Borrowed uint[clipmap_count][caster_count] buffer indexed by input record index.
     */
    buffer visibility(id id, uint32_t frame_slot);
    /**
     * @brief Obtain GPU indirect commands generated from frame_inputs::indexed_draws.
     * @param id Owning instance.
     * @param frame_slot Slot index below params::in_flight_frames.
     * @return Borrowed per-clipmap arrays of VkDrawIndexedIndirectCommand. Rejected
     * casters have instanceCount zero; other template fields are preserved.
     * @pre Indexed draw templates were supplied to begin() for this slot.
     */
    buffer indexed_draws(id id, uint32_t frame_slot);
    /**
     * @brief Obtain the GPU statistics buffer.
     * @param id Owning instance.
     * @param frame_slot Slot index below params::in_flight_frames.
     * @return Borrowed buffer with the statistics layout.
     * @note Copy to readback after end(); wait for GPU completion and invalidate
     * noncoherent readback memory before CPU access.
     */
    buffer counters(id id, uint32_t frame_slot);
    /**
     * @brief Obtain the physical depth atlas.
     * @param id Owning instance.
     * @return Borrowed R32_UINT texture containing float depth bit patterns, in GENERAL
     * layout after begin(). Depth writes use unsigned integer atomics.
     */
    texture physical_pool(id id);
} // namespace vkz::svsm
