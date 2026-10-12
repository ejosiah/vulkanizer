#pragma once

#include "rtx_neural_cache.hpp"
#include <vulkanizer/reflection.hpp>
#include <vulkanizer/ray_tracing_pipeline.hpp>

namespace rtx {
    class nrc_gi {
    public:
        nrc_gi(vkz::context &context, vkz::vma_memory_allocator &allocator,
            const vkz::reflection &reflection, uint32_t texture_count, glm::uvec2 resolution,
            glm::vec4 bounds, VkQueue queue, uint32_t queue_family);
        ~nrc_gi();
        void exec(VkCommandBuffer cmd);
        void end_frame(VkQueue queue);
        void reset(glm::vec4 bounds);
        const vkz::texture &indirect_light_texture() const { return indirect_light_; }

    private:
        neural_radiance_cache cache_;
        vkz::reflection::tracing_input tracing_;
        vkz::ray_tracing_pipeline pipeline_;
        vkz::texture indirect_light_;
        vkz::descriptor_set_layout output_layout_;
        vkz::descriptor_pool output_pool_;
        vkz::descriptor_set output_set_;
        vkz::pipeline resolve_pipeline_;
        glm::uvec2 resolution_;
    };
}
