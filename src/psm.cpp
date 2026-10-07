#include "vulkanizer/psm.hpp"
#include "vulkanizer/primitives.hpp"
#include "vulkanizer/detail/functions.hpp"
#include "vulkanizer/barrier.hpp"
#include "vulkanizer/commands.hpp"
#include "vulkanizer/builders.hpp"
#include "vulkanizer/render.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <format>
#include <ranges>

namespace vkz::psm {

    namespace {
        constexpr float near_clip = 0.1f;
        std::string psm_vertex_shader = R"(
#version 460

#extension GL_EXT_multiview : enable
#extension GL_EXT_nonuniform_qualifier : enable

${vertex_shader_include}

layout(std430, set = 0, binding = 0) readonly buffer Light {
    mat4 projection;
    mat4 view[6];
};

layout(push_constant) uniform Constants {
    mat4 worldTransform;
    vec3 lightPos;
};

layout(location = 0) out struct {
    vec3 worldPos;
    vec3 lightPos;
} vs_out;

void main() {
    mat4 model = worldTransform * get_model_matrix();

    vec4 worldPos = model * vec4(position, 1);

    gl_Position = projection * view[gl_ViewIndex] * worldPos;
    vs_out.worldPos = worldPos.xyz;
    vs_out.lightPos = lightPos;

}

)";

        std::string psm_fragment_shader = R"(
#version 460
layout(location = 0) in struct {
    vec3 worldPos;
    vec3 lightPos;
} fs_in;
layout(location = 0) out float pointToLightDistance;
void main() {
    pointToLightDistance = distance(fs_in.worldPos, fs_in.lightPos);
}
)";

        constexpr int PROJECTION_MATRIX = 0;
        constexpr int VIEW_MATRIX_START = 1;
        struct face {
            glm::vec3 direction;
            glm::vec3 up;
        };

        std::array<face, 6> faces = {{
            {{1.0, 0.0, 0.0}, {0.0,-1.0, 0.0}},
            {{-1.0, 0.0, 0.0}, {0.0,-1.0, 0.0}},
            {{0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},
            {{0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}},
            {{0.0, 0.0, 1.0}, {0.0,-1.0, 0.0}},
            {{0.0, 0.0, -1.0}, {0.0,-1.0, 0.0}}
        }};

        struct shadow_map {
            texture color;
            image_view color_attachment;
            texture depth;
            image_view depth_attachment;
        };

        struct push_constants {
            glm::mat4 world_transform{1};
            glm::vec3 light_position{};
        };
    }

    class impl {
    public:
        impl() = default;

        explicit impl(const params& params)
            : device_(params.device)
            , allocator_(params.memory_allocator)
            , descriptor_pool_(params.descriptor_pool)
            , queue_family_(params.queue_family_index)
            , shadow_maps_(params.in_flight_frames)
            , uniforms_(params.in_flight_frames)
            , descriptor_sets(params.in_flight_frames)
            , depth_format_(params.depth_format)
            , vertex_shader_include_(params.vertex_shader_include)
            , vertex_shader_position_offset_(params.vertex_shader_position_offset)
            , vertex_shader_position_stride_(params.vertex_shader_position_stride)
            , vertex_include_descriptor_set_layout_(params.vertex_include_descriptor_set_layout)
            , resolution_(params.resolution)
            , range_(params.range)
        {}

        ~impl() {
            destroy();
        }

        void init() {
            if (!(range_ > near_clip) || !std::isfinite(range_)) {
                VKZ_THROW("Point shadow map range must be finite and greater than 0.1")
            }
            staging_buffer staging_buffer{allocator_, 1024 * sizeof(uint32)};
            {
                scope_command_buffer command_buffer{ device_, queue_family_};
                create_shadow_maps(command_buffer);
                create_uniforms(command_buffer, staging_buffer);
                create_cube(command_buffer, staging_buffer);
            }
            create_render_info();
            create_descriptor_set_layout();
            update_descriptor_sets();
            create_pipeline();

            staging_buffer.reset();
            staging_buffer.destroy();
        }

        void destroy() {
            if (pipeline_.handle) { pipeline_.destroy(); pipeline_ = {}; }
            for (auto& uniform : uniforms_) {
                if (uniform._) { uniform.destroy(); uniform = {}; }
            }
            for (auto& map : shadow_maps_) {
                if (map.color.image_view.handle) map.color.image_view.destroy();
                if (map.color_attachment.handle) map.color_attachment.destroy();
                if (map.color.image.handle) map.color.image.destroy();
                if (map.depth.image_view.handle) map.depth.image_view.destroy();
                if (map.depth_attachment.handle) map.depth_attachment.destroy();
                if (map.depth.image.handle) map.depth.image.destroy();
                map = {};
            }
            if (cube_.mesh.vertex_buffer._) cube_.mesh.vertex_buffer.destroy();
            if (cube_.mesh.index_buffer) cube_.mesh.index_buffer->destroy();
            cube_.mesh = {};
            if (descriptor_set_layout_.handle) { descriptor_set_layout_.destroy(); descriptor_set_layout_ = {}; }
        }

        void update(const glm::vec3& light_position) {
            constants_.light_position = light_position;
            for(auto i = 0; i < 6; ++i) {
                light_matrix[VIEW_MATRIX_START + i] = glm::lookAt(light_position, light_position + faces[i].direction, faces[i].up);
            }
        }

        void set_range(float range) {
            light_matrix[PROJECTION_MATRIX] = glm::perspectiveRH_ZO(glm::half_pi<float>(), 1.0f, near_clip, range);
        }

        void capture(const scene& scene, VkCommandBuffer command_buffer, uint32 frame_index) {
            if (static_cast<size_t>(frame_index) >= shadow_maps_.size()) {
                VKZ_THROW("Cascade shadow map frame index is out of range")
            }

            const auto i = frame_index;
            vkz::update(command_buffer, uniforms_[i], sizeof(glm::mat4) * 7, light_matrix.data());
            VkMemoryBarrier2 uniform_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            uniform_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            uniform_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            uniform_barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            uniform_barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.memoryBarrierCount = 1;
            dependency.pMemoryBarriers = &uniform_barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency);
            auto& map = shadow_maps_[i];
            vkz::barrier::push_and_flush(command_buffer, map.color.image, map.color.image_view.create_info.subresourceRange,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vkz::barrier::push_and_flush(command_buffer, map.depth.image, map.depth.image_view.create_info.subresourceRange,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
            render(command_buffer, render_info_[i], [&]{
                vkz::bind_pipeline(command_buffer, pipeline_);
                vkz::bind_descriptor_sets(command_buffer, pipeline_, {descriptor_sets[i]});
                vkz::set_cull_mode(command_buffer, VK_CULL_MODE_FRONT_BIT);
                vkz::set_primitive_topology(command_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
                vkz::push_constants(command_buffer, pipeline_, constants_);
                scene(pipeline_);
            });
            vkz::barrier::push_and_flush(command_buffer, map.color.image, map.color.image_view.create_info.subresourceRange,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        [[nodiscard]]
        texture get_shadow_map(uint32 frame_index) const {
            return shadow_maps_.at(frame_index).color;
        }

    private:
        void create_shadow_maps(VkCommandBuffer command_buffer) {
            const auto usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            auto img_builder =
                image::builder(allocator_)
                    .type(VK_IMAGE_TYPE_2D)
                    .flags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
                    .format(depth_format_)
                    .extent(resolution_.x, resolution_.y, 1)
                    .array_layers(6);

            auto view_builder =
                image_view::builder(device_)
                    .view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                    .format(depth_format_)
                    .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                    .layer_count(6);

            for(auto& shadow_map : shadow_maps_) {
                shadow_map.color.image = img_builder.format(VK_FORMAT_R32_SFLOAT)
                    .usage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT).build();
                shadow_map.color.image_view = image_view::builder(device_)
                    .image(shadow_map.color.image).view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                    .format(VK_FORMAT_R32_SFLOAT).aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT).layer_count(6).build();
                shadow_map.color_attachment = image_view::builder(device_)
                    .image(shadow_map.color.image).view_type(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
                    .format(VK_FORMAT_R32_SFLOAT).aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT).layer_count(6).build();
                img_builder.format(depth_format_);
                shadow_map.depth.image = img_builder.usage(usage).build();
                shadow_map.depth.image_view = view_builder.image(shadow_map.depth.image).build();
                shadow_map.depth_attachment = image_view::builder(device_)
                    .image(shadow_map.depth.image).view_type(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
                    .format(depth_format_).aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT).layer_count(6).build();

                vkz::barrier::push(shadow_map.color.image, shadow_map.color.image_view.create_info.subresourceRange,
                                   VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
                                   VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            vkz::barrier::flush(command_buffer);
        }

        void create_render_info() {
            for(auto& shadow_map : shadow_maps_) {
                render_info_.push_back({
                    .color_attachments = {{shadow_map.color_attachment, VK_FORMAT_R32_SFLOAT, glm::vec4(std::numeric_limits<float>::max())}},
                    .depth_attachment = { { shadow_map.depth_attachment, depth_format_ } },
                    .render_area = resolution_,
                    .num_layers = 6,
                    .view_mask =  0b00111111
                });
            }
        }

        void create_uniforms(VkCommandBuffer command_buffer, staging_buffer& staging) {
            const auto uniforms_byte_size = sizeof(glm::mat4) * 7;

            auto memory = staging.borrow(uniforms_byte_size);
            auto light_matrices = memory.as<glm::mat4>();
            set_range(range_);
            light_matrices[PROJECTION_MATRIX] = light_matrix[PROJECTION_MATRIX];
            for(auto i = VIEW_MATRIX_START; i < 7; ++i) {
                light_matrices[i] = glm::mat4{1};
            }

            for(auto i = 0; i < uniforms_.size(); ++i) {
                uniforms_[i] =
                    buffer::builder(allocator_)
                        .name(prefix(std::format("light_matrix_{}", i)))
                        .usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                        .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                        .size(uniforms_byte_size)
                    .build();

                memory.copy_to(command_buffer, uniforms_[i]);
            }
        }

        void create_cube(VkCommandBuffer command_buffer, staging_buffer& staging) {
            const auto cube = prim::cube();
            const auto vertices = map_range(cube.vertices, [](const auto& v){ return v.position; });
            const auto indices = cube.indices;
            auto vertex_byte_size = sizeof(vertices[0]) * vertices.size();
            cube_.mesh.vertex_buffer =
                buffer::builder(allocator_)
                    .name(prefix("ube_mesh_vertices"))
                    .usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                    .size(vertex_byte_size)
                .build();

            auto index_byte_size = sizeof(indices[0]) * indices.size();
            cube_.mesh.index_buffer =
                buffer::builder(allocator_)
                    .name(prefix("ube_mesh_indexes"))
                    .usage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                    .memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
                    .size(sizeof(indices[0]) * indices.size())
                .build();

            auto vertex_mem = staging.borrow(vertex_byte_size);
            vertex_mem.copy_from(vertices.data(), vertex_byte_size);

            auto index_mem = staging.borrow(index_byte_size);
            index_mem.copy_from(indices.data(), index_byte_size);

            vertex_mem.copy_to(command_buffer, cube_.mesh.vertex_buffer);
            index_mem.copy_to(command_buffer, cube_.mesh.index_buffer.value());
        }

        void create_descriptor_set_layout() {
            descriptor_set_layout_ =
                make_descriptor_set_layout_builder(device_)
                    .name("psm_descriptor_set_layout")
                    .binding(0)
                        .descriptor_type(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                        .descriptor_count(1)
                        .shader_stages(VK_SHADER_STAGE_ALL_GRAPHICS)
                .create_layout();
        }

        void update_descriptor_sets() {
            descriptor_sets = descriptor_pool_.allocate_n(descriptor_set_layout_, descriptor_sets.size());

            for (size_t i = 0; i < descriptor_sets.size(); ++i) {
                update_descriptors(device_, {{
                    .descriptor_set = descriptor_sets[i],
                    .bindings = {descriptor<buffer_descriptor>(uniforms_[i], 0)}
                }});
            }
        }

        void create_pipeline() {
            std::string vertex_shader = psm_vertex_shader;
            vertex_shader
                .replace(
                    vertex_shader.find("${vertex_shader_include}"),
                    std::string{"${vertex_shader_include}"}.size(),
                    vertex_shader_include_);

            const auto w = resolution_.x;
            const auto h = resolution_.y;

            pipeline_ =
                make_graphics_pipeline_builder(device_)
                    .shader_stage()
                        .vertex_shader(vertex_shader)
                        .fragment_shader(psm_fragment_shader)
                    .vertex_input_state()
                        .add_vertex_binding_description(0, vertex_shader_position_stride_, VK_VERTEX_INPUT_RATE_VERTEX)
                        .add_vertex_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, vertex_shader_position_offset_)
                    .input_assembly_state()
                        .triangles()
                    .viewport_state()
                        .viewport()
                            .origin(0, 0)
                            .dimension(w, h)
                            .min_depth(0)
                            .max_depth(1)
                        .scissor()
                            .offset(0, 0)
                            .extent(static_cast<int>(w), static_cast<int>(h))
                        .add()
                    .rasterization_state()
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
                        .attachment().add()
                    .dynamic_state()
                        .cull_mode()
                        .primitive_topology()
                    .layout()
                        .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants))
                        .add_descriptor_set_layout(descriptor_set_layout_)
                        .add_descriptor_set_layout(vertex_include_descriptor_set_layout_)
                    .dynamic_render_pass()
                        .add_color_attachment(VK_FORMAT_R32_SFLOAT)
                        .depth_attachment(depth_format_)
                        .view_mask(0b00111111)
                    .subpass(0)
                    .name(prefix("capture_pipeline"))
                .build();
        }

        std::string prefix(const std::string& label) {
            static uint32 sequence = 0;
            if(name_.empty()) {
                return std::format("psm_{}_{}", label, sequence++);
            }else {
                return std::format("{}_psm_{}", name_, label);
            }
        }

        device device_;
        vma_memory_allocator allocator_;
        descriptor_pool descriptor_pool_{};
        std::string name_{};
        std::string vertex_shader_include_;
        descriptor_set_layout vertex_include_descriptor_set_layout_;
        VkDeviceSize vertex_shader_position_offset_{};
        VkDeviceSize vertex_shader_position_stride_{};
        descriptor_set_layout descriptor_set_layout_;
        std::vector<descriptor_set> descriptor_sets;
        uint32_t queue_family_{};
        VkFormat depth_format_{VK_FORMAT_UNDEFINED};
        std::vector<shadow_map> shadow_maps_;
        glm::uvec2 resolution_{};
        float range_{DEFAULT_RANGE};
        pipeline pipeline_;
        pipeline debug_pipeline_;
        std::vector<render_info> render_info_;
        std::vector<buffer> uniforms_;

        std::array<glm::mat4, 7> light_matrix{};
        push_constants constants_{};

        struct {
            mesh_buffers mesh;
            pipeline pipeline;
        } cube_;
    };

    namespace {
        std::unordered_map<id, impl> psm_instances;
        id next_id{1};

        impl& get(id id) {
            auto itr = psm_instances.find(id);
            if (itr == psm_instances.end()) {
                VKZ_THROW("Invalid point shadow map id")
            }
            return itr->second;
        }
    }

    id create(const params& params) {
        auto id = next_id++;
        auto [itr, inserted] = psm_instances.try_emplace(id, params);
        itr->second.init();
        return id;
    }

    void destroy(id id) {
        auto itr = psm_instances.find(id);
        if (itr == psm_instances.end()) {
            return;
        }

        itr->second.destroy();
        psm_instances.erase(itr);
    }

    void update(id id, const glm::vec3 &light_position) {
        get(id).update(light_position);
    }

    void capture(id id, VkCommandBuffer command_buffer, uint32 frame_index, const scene &scene) {
        get(id).capture(scene, command_buffer, frame_index);
    }

    texture get_shadow_map(id id, uint32 frame_index) {
        return get(id).get_shadow_map(frame_index);
    }
}
