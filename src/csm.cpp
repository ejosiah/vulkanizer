#include "vulkanizer/csm.hpp"

#include <array>
#include <unordered_map>

#include "vulkanizer/render.hpp"
#include "vulkanizer/transforms.hpp"
#include "vulkanizer/descriptor_set_builder.hpp"
#include "vulkanizer/graphics_pipeline_builder.hpp"

namespace vkz::csm {

    static std::string cms_vertex_shader = R"(
#version 460

#extension GL_EXT_multiview : enable
#extension GL_EXT_nonuniform_qualifier : enable

${vertex_shader_include}

layout(push_constant) uniform Constants {
    mat4 worldTransform;
    int cascadeIndex;
};

void main() {
    mat4 model = worldTransform * get_model_matrix();
    gl_Position = cascadeViewProjMat[gl_ViewIndex] * model * position;
}
)";

    static std::string cms_fragment_shader = R"(
#version 460

void main() {
    // TODO discard shadow fragment based on mesh alpha
}
)";

    static std::string quad_vertex_shader = R"(
#version 460 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 vUv;

void main(){
    vUv = uv;
    gl_Position = vec4(pos, 0, 1);
}
)";

    static std::string shadow_map_debug_frag = R"(
#version 460 core

layout(set = 0, binding = 1) uniform sampler2DArray shadowMap;

layout(push_constant) uniform Constants {
    uint numCascades;
};

layout(location = 0) in vec2 fuv;

layout(location = 0) out vec4 fragColor;

void main(){

    vec2 uv = fuv;
    int id = 0;
    if(numCascades > 1){
        float w = ceil(sqrt(numCascades));
        float h = numCascades/w;
        vec2 grid = vec2(w, h);
        vec2 gid = floor(fuv * grid);
        id = int(gid.y * w + gid.x);
        uv = fract(fuv * grid);

        if (id >= numCascades) discard;
    }

    fragColor = texture(shadowMap, vec3(uv, id)).rrrr;
}
    )";

    class impl {
    public:
        
    impl() = default;

    explicit impl(const params& params)
        : device_(params.device)
        , descriptor_pool_(params.descriptor_pool)
        , depth_format_(params.depth_format)
        , num_cascades_(params.num_cascades)
        , size_(params.size)
        , cascade_splits_(params.num_cascades)
        , shadow_map_(params.in_flight_frames)
        , memory_allocator_(params.memory_allocator)
        , vertex_include_descriptorset_layout_(params.vertex_include_descriptorset_layout)
        , vertex_shader_include_(params.vertex_shader_include)
        , vertex_shader_position_offset_(params.vertex_shader_position_offset)
        , vertex_shader_position_stride_(params.vertex_shader_position_stride){
        assert(params.size != 0 && "shadow map size should not be zero");
        assert(params.num_cascades > 0 && "numCascades should be at least 2");
        assert(params.in_flight_frames != 0 && "inflightFrames should be at least 1");
    }

    void init() {
        createShadowMapTexture();
        createRenderInfo();
        createUniforms();
        createDescriptorSetLayouts();
        createDescriptorSet();
        updateDescriptorSets();
        createPipeline();
    }

    void destroy() {
        if (device_.logical) {
            if (debug_.pipeline) {
                vkDestroyPipeline(device_.logical, debug_.pipeline, nullptr);
                debug_.pipeline = VK_NULL_HANDLE;
            }

            if (debug_.layout) {
                vkDestroyPipelineLayout(device_.logical, debug_.layout, nullptr);
                debug_.layout = VK_NULL_HANDLE;
            }

            if (pipeline_) {
                vkDestroyPipeline(device_.logical, pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (layout_) {
                vkDestroyPipelineLayout(device_.logical, layout_, nullptr);
                layout_ = VK_NULL_HANDLE;
            }

            for (auto& shadow_map : shadow_map_) {
                if (shadow_map.sampler.handle) {
                    vkDestroySampler(device_.logical, shadow_map.sampler.handle, nullptr);
                    shadow_map.sampler = {};
                }

                if (shadow_map.image_view.handle) {
                    vkDestroyImageView(device_.logical, shadow_map.image_view.handle, nullptr);
                    shadow_map.image_view = {};
                }
            }

            if (descriptor_set_layout_) {
                vkDestroyDescriptorSetLayout(device_.logical, descriptor_set_layout_, nullptr);
                descriptor_set_layout_ = VK_NULL_HANDLE;
            }

            if (owns_vertex_include_descriptorset_layout_ && vertex_include_descriptorset_layout_) {
                vkDestroyDescriptorSetLayout(device_.logical, vertex_include_descriptorset_layout_, nullptr);
                vertex_include_descriptorset_layout_ = VK_NULL_HANDLE;
            }
        }

        if (memory_allocator_.allocator) {
            if (debug_buffer_) {
                memory_allocator_.deallocate(debug_buffer_);
                debug_buffer_ = {};
            }

            if (_uniforms.gpu) {
                memory_allocator_.deallocate(_uniforms.gpu);
                _uniforms = {};
            }

            for (auto& shadow_map : shadow_map_) {
                if (shadow_map.image.handle) {
                    memory_allocator_.deallocate(shadow_map.image);
                    shadow_map.image = {};
                }
            }
        }

        render_info_.clear();
        shadow_map_.clear();
        cascade_splits_.clear();
        view_indexes_ = {};
        descriptor_set_ = VK_NULL_HANDLE;
        owns_vertex_include_descriptorset_layout_ = false;
    }

    void update(const camera& camera, const glm::vec3& lightDirection, std::span<float> splitDepth, std::span<glm::vec3> extents = {}) {
        if(!extents.empty()) {
            assert(extents.size() == num_cascades_ * 2);
        }
        const auto nearClip = camera.near_plane;
        const auto farClip = camera.far_plane;
        const auto clipRange = farClip - nearClip;

        const auto minZ = nearClip;
        const auto maxZ = nearClip + clipRange;

        const auto range = maxZ - minZ;
        const auto ratio = maxZ / minZ;


        // Calculate split depths based on view camera frustum
        // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
        for (uint i = 0; i < num_cascades_; i++) {
            float p = (i + 1) / static_cast<float>(num_cascades_);
            float log = minZ * std::pow(ratio, p);
            float uniform = minZ + range * p;
            float d = split_lambda_ * (log - uniform) + uniform;
            cascade_splits_[i] = (d - nearClip) / clipRange;
        }

        float lastSplitDist = 0.0;
        for (uint i = 0; i < num_cascades_; i++) {
            float splitDist = cascade_splits_[i];

            glm::vec3 frustumCorners[8] = {
                glm::vec3(-1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f,  1.0f,  1.0f),
                glm::vec3( 1.0f, -1.0f,  1.0f),
                glm::vec3(-1.0f, -1.0f,  1.0f),
            };


            // Project frustum corners into world space
            glm::mat4 invCam = glm::inverse(camera.view_projection);
            for (auto & frustumCorner : frustumCorners) {
                glm::vec4 invCorner = invCam * glm::vec4(frustumCorner, 1.0f);
                frustumCorner = invCorner / invCorner.w;
            }

            for (uint j = 0; j < 4; j++) {
                glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
                frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
                frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
            }

            // Get frustum center
            auto frustumCenter = glm::vec3(0.0f);
            for (auto frustumCorner : frustumCorners) {
                frustumCenter += frustumCorner;
            }
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (auto frustumCorner : frustumCorners) {
                float distance = glm::length(frustumCorner - frustumCenter);
                radius = glm::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            auto maxExtents = glm::vec3(radius);
            auto minExtents = -maxExtents;

            if(!extents.empty()){
                extents[i * 2] =  minExtents;
                extents[i * 2 + 1] =  maxExtents;
            }

            auto lightDir = normalize(lightDirection);
            auto lightViewMatrix = glm::lookAt(frustumCenter + lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
            auto lightOrthoMatrix = vkz::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

            // Store split distance and matrix in cascade
            splitDepth[i] = (camera.near_plane + splitDist * clipRange) * -1.0f;
            _uniforms.cpu[i]= lightOrthoMatrix * lightViewMatrix;

            lastSplitDist = cascade_splits_[i];
        }
    }

    void capture(const scene& scene, VkCommandBuffer commandBuffer, int currentFrame) const {
        vkz::render(commandBuffer, render_info_[currentFrame], [&] {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &descriptor_set_, 0, VK_NULL_HANDLE);
            vkCmdSetCullMode(commandBuffer, VK_CULL_MODE_FRONT_BIT);
            vkCmdSetPrimitiveTopology(commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            vkCmdPushConstants(commandBuffer, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants_), &constants_);

            scene(layout_);
        });
    }

    const texture& shadowMap(int index) const {
        return shadow_map_[index];
    }

    uint cascadeCount() const {
        return num_cascades_;
    }

    void setRenderPass(VkRenderPass renderPass, glm::uvec2 resolution) {
        render_pass_ = renderPass;
        screen_resolution_ = resolution;
    }

    void render(VkCommandBuffer commandBuffer) const {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debug_.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debug_.layout, 0, 1, &descriptor_set_, 0, nullptr);
        vkCmdPushConstants(commandBuffer, debug_.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint), &num_cascades_);
        // TODO render clip space quad
    }

    void splitLambda(float value) {
        split_lambda_ = value;
    }

    buffer cascadeViewProjection() const {
        return _uniforms.gpu;
    }

    VkDescriptorSetLayout  descriptorSetLayout() const {
        return descriptor_set_layout_;
    }

    VkDescriptorSet descriptorSet() const {
        return descriptor_set_;
    }


private:
    void createShadowMapTexture() {
        for (auto& shadow_map : shadow_map_) {
            shadow_map.image =
                image::builder(memory_allocator_)
                    .type(VK_IMAGE_TYPE_2D)
                    .format(depth_format_)
                    .extent({ size_, size_, 1})
                    .mip_levels(1)
                    .array_layers(1)
                    .samples(VK_SAMPLE_COUNT_1_BIT)
                    .tiling(VK_IMAGE_TILING_OPTIMAL)
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                    .sharing_mode(VK_SHARING_MODE_EXCLUSIVE)
                    .initial_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                .build();

            shadow_map.image_view =
                image_view::builder(device_.logical)
                    .image(shadow_map.image)
                    .view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .format(depth_format_)
                    .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .base_mip_level(0)
                    .level_count(1)
                    .base_array_layer(0)
                    .layer_count(num_cascades_)
                .build();

            shadow_map.sampler =
                sampler::builder(device_.logical)
                    .mag_filter(VK_FILTER_LINEAR)
                    .min_filter(VK_FILTER_LINEAR)
                    .mipmap_mode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                    .address_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                    .mip_lod_bias(0.0)
                    .anisotropy(true, 1.0)
                    .min_lod(0.0)
                    .max_lod(1.0f)
                    .border_color(VK_BORDER_COLOR_INT_OPAQUE_WHITE)
                .build();

            // TODO transition layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

        }
    }

    void createRenderInfo() {
        for(auto& shadowMap : shadow_map_) {
            render_info_.push_back({
                  .depth_attachment = { { shadowMap.image_view, depth_format_ } },
                  .render_area = { size_, size_ },
                  .num_layers = num_cascades_,
                  .view_mask =  viewMask()
              });
        }
    }

    void createUniforms() {
        _uniforms.gpu =
            buffer::builder(memory_allocator_)
                .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .size(sizeof(glm::mat4))
            .build();

        debug_buffer_ =
            buffer::builder(memory_allocator_)
                .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                .memory_usage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .size(sizeof(num_cascades_))
            .build();

        auto mapping = _uniforms.gpu.map();
        *mapping.as<glm::mat4>() = glm::mat4{1};
        mapping.unmap();

        mapping = debug_buffer_.map();
        *mapping.as<uint>() = num_cascades_;
        mapping.unmap();
    }

    void createDescriptorSetLayouts() {
        descriptor_set_layout_ =
            descriptor_set_layout_builder{device_}
                .name("cascade_shadow_map_light_descriptor_set_layout")
                .binding(0)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    .descriptor_count(1)
                    .shader_stages(VK_SHADER_STAGE_VERTEX_BIT)
                .binding(1)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .descriptor_count(1)
                    .shader_stages(VK_SHADER_STAGE_FRAGMENT_BIT)
                .binding(2)
                    .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    .descriptor_count(1)
                    .shader_stages(VK_SHADER_STAGE_VERTEX_BIT)
                .create_layout();

        if (!vertex_include_descriptorset_layout_) {
            vertex_include_descriptorset_layout_ =
                descriptor_set_layout_builder{device_}
            .name("cascade_shadow_map_vertex_shader_include_layout")
            .binding(0)
                .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                .descriptor_count(1)
                .shader_stages(VK_SHADER_STAGE_VERTEX_BIT)
            .create_layout();
            owns_vertex_include_descriptorset_layout_ = true;
        }
    }

    void createDescriptorSet() {
        VkDescriptorSetAllocateInfo allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate_info.descriptorPool = descriptor_pool_;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &descriptor_set_layout_;

        VKZ_CHECK_VULKAN(vkAllocateDescriptorSets(device_.logical, &allocate_info, &descriptor_set_));
    }

    void updateDescriptorSets() const {
        std::array<VkWriteDescriptorSet, 3> writes{};

        writes[0].dstSet = descriptor_set_;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].descriptorCount = 1;
        const VkDescriptorBufferInfo info{ _uniforms.gpu, 0, VK_WHOLE_SIZE };
        writes[0].pBufferInfo = &info;

        writes[1].dstSet = descriptor_set_;
        writes[1].dstBinding = 2;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        VkDescriptorBufferInfo debuginfo{ debug_buffer_, 0, VK_WHOLE_SIZE };
        writes[1].pBufferInfo = &debuginfo;

        writes[2].dstSet = descriptor_set_;
        writes[2].dstBinding = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        VkDescriptorImageInfo imageInfo{
            shadow_map_[0].sampler.handle
            , shadow_map_[0].image_view
            , VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        writes[2].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device_.logical, VKZ_COUNT(writes), writes.data(), 0, nullptr);
    }

    void createPipeline() {
        std::string vertex_shader = cms_vertex_shader;
        vertex_shader.replace(vertex_shader.find("${vertex_shader_include}"), std::string{"${vertex_shader_include}"}.size(), vertex_shader_include_);
        pipeline_ =
            graphics_pipeline_builder{device_}
                .allow_derivatives()
                .shader_stage()
                    .vertex_shader(vertex_shader)
                    .fragment_shader(cms_fragment_shader)
                .vertex_input_state()
                    .add_vertex_binding_description(0, vertex_shader_position_stride_, VK_VERTEX_INPUT_RATE_VERTEX)
                    .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, vertex_shader_position_offset_)
                .input_assembly_state()
                    .triangles()
                .viewport_state()
                    .viewport()
                        .origin(0, 0)
                        .dimension(size_, size_)
                        .min_depth(0)
                        .max_depth(1)
                    .scissor()
                        .offset(0, 0)
                        .extent(size_, size_)
                    .add()
                .rasterization_state()
                    .enable_depth_bias()
                    .depth_bias_constant_factor(depth_bias_constant_)
                    .depth_bias_slope_factor(depth_bias_slope_)
                    .cull_front_face()
                    .front_face_counter_clockwise()
                    .polygon_mode_fill()
                .depth_stencil_state()
                    .enable_depth_write()
                    .enable_depth_test()
                    .compare_op_less_or_equal()
                    .min_depth_bounds(0)
                    .max_depth_bounds(1)
                .color_blend_state()
                    .attachment()
                    .add()
                .dynamic_state()
                    .cull_mode()
                    .primitive_topology()
                .layout()
                    .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants_))
                    .add_descriptor_set_layout(descriptor_set_layout_)
                    .add_descriptor_set_layout(vertex_include_descriptorset_layout_)
                .dynamic_render_pass()
                    .depth_attachment(depth_format_)
                    .view_mask(viewMask())
                .subpass(0)
                .name("cascade_shadow_map")
            .build(layout_);

            if(render_pass_) {
                debug_.pipeline =
                    graphics_pipeline_builder{device_}
                        .shader_stage()
                            .vertex_shader(quad_vertex_shader)
                            .fragment_shader(shadow_map_debug_frag)
                        .vertex_input_state()
                            .add_vertex_binding_description(0, sizeof(glm::vec4), VK_VERTEX_INPUT_RATE_VERTEX)
                            .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT,  0)
                            .add_vertex_attribute_description(1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec2))
                        .input_assembly_state()
                            .triangle_strip()
                        .viewport_state()
                            .viewport()
                                .origin(0, 0)
                            .dimension(screen_resolution_.x, screen_resolution_.y)
                                .min_depth(0)
                                .max_depth(1)
                            .scissor()
                                .offset(0, 0)
                                .extent(screen_resolution_.x, screen_resolution_.y)
                            .add()
                        .rasterization_state()
                            .cull_back_face()
                            .front_face_counter_clockwise()
                            .polygon_mode_fill()
                        .depth_stencil_state()
                            .enable_depth_write()
                            .enable_depth_test()
                            .compare_op_always()
                            .min_depth_bounds(0)
                            .max_depth_bounds(1)
                        .color_blend_state()
                            .attachment()
                            .add()
                        .layout()
                            .add_push_constant_range(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint))
                            .add_descriptor_set_layout(descriptor_set_layout_)
                        .render_pass(render_pass_)
                        .subpass(0)
                        .name("debug_cascade_shadow_map")
                    .build(debug_.layout);
            }
    }

    uint viewMask() const {
        return (1u << num_cascades_) - 1;
    }

    VkDevice& device() {
        return device_.logical;
    }

    vkz::device device_{};
    VkDescriptorPool descriptor_pool_{};
    VkFormat depth_format_{VK_FORMAT_UNDEFINED};
    uint num_cascades_{};
    uint size_{};
    float split_lambda_{DEFAULT_CASCADE_SLIT_LAMBDA};
    std::vector<float> cascade_splits_;
    std::vector<render_info> render_info_;
    std::vector<texture> shadow_map_;
    vma_memory_allocator memory_allocator_;
    VkDescriptorSetLayout descriptor_set_layout_{};
    VkDescriptorSetLayout vertex_include_descriptorset_layout_{};
    bool owns_vertex_include_descriptorset_layout_{};
    std::string vertex_shader_include_;
    VkDeviceSize vertex_shader_position_offset_{};
    VkDeviceSize vertex_shader_position_stride_{};
    VkDescriptorSet descriptor_set_{};
    glm::uvec2 screen_resolution_{};
    float depth_bias_constant_{0.005f};
    float depth_bias_slope_{0.05f};

    struct {
        glm::mat4 worldTransform{1};
        int cascadeIndex{0};
    } constants_;

    struct {
        buffer gpu;
        std::span<glm::mat4> cpu;
    } _uniforms;
    buffer debug_buffer_;
    std::span<uint> view_indexes_;
    VkPipeline pipeline_{};
    VkPipelineLayout layout_{};
    VkRenderPass render_pass_{};

    struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    } debug_;
};

    namespace {
        std::unordered_map<id, impl> csm_instances;
        id next_id{1};

        impl& get(id id) {
            auto itr = csm_instances.find(id);
            if (itr == csm_instances.end()) {
                VKZ_THROW("Invalid cascade shadow map id")
            }
            return itr->second;
        }
    }

    id create(const params& params) {
        auto result = next_id++;
        auto [itr, inserted] = csm_instances.emplace(result, impl{params});
        itr->second.init();
        return result;
    }

    void destroy(id id) {
        auto itr = csm_instances.find(id);
        if (itr == csm_instances.end()) {
            return;
        }

        itr->second.destroy();
        csm_instances.erase(itr);
    }

    void update(id id, const camera& camera, const glm::vec3& light_direction, std::span<float> split_depth) {
        get(id).update(camera, light_direction, split_depth);
    }

    void capture(id id, const scene& scene, VkCommandBuffer command_buffer, int current_frame) {
        get(id).capture(scene, command_buffer, current_frame);
    }

    const texture shadow_map(id id, int index) {
        return get(id).shadowMap(index);
    }

    uint cascade_count(id id) {
        return get(id).cascadeCount();
    }

    buffer cascade_view_projection(id id) {
        return get(id).cascadeViewProjection();
    }

    VkDescriptorSetLayout descriptor_set_layout(id id) {
        return get(id).descriptorSetLayout();
    }

    VkDescriptorSet descriptor_set(id id) {
        return get(id).descriptorSet();
    }

    void set(id id, VkRenderPass render_pass, glm::uvec2 resolution) {
        get(id).setRenderPass(render_pass, resolution);
    }

    void render(id id, VkCommandBuffer command_buffer) {
        get(id).render(command_buffer);
    }

    void split_lambda(id id, float value) {
        get(id).splitLambda(value);
    }
}
