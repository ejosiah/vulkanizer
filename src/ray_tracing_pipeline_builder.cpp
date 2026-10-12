#include <vulkanizer/ray_tracing_pipeline_builder.hpp>
#include <vulkanizer/creators.hpp>
#include <algorithm>

namespace vkz {
    ray_tracing_pipeline_builder::ray_tracing_pipeline_builder(vma_memory_allocator &allocator) : allocator(allocator), layout_storage(allocator.device), layout_proxy(*this) {
    }

    ray_tracing_pipeline_builder::~ray_tracing_pipeline_builder() = default;

    ray_tracing_pipeline_builder &ray_tracing_pipeline_builder::shader_groups() {
        return *this;
    }

    ray_tracing_pipeline_layout_builder &ray_tracing_pipeline_builder::layout() {
        return layout_proxy;
    }

    ray_tracing_pipeline_builder &ray_tracing_pipeline_builder::max_recursion_depth(uint32_t value) {
        recursion_depth = value;
        return *this;
    }

    ray_tracing_pipeline_builder &ray_tracing_pipeline_builder::name(const std::string &value) {
        pipeline_name = value;
        return *this;
    }

    ray_tracing_pipeline_builder &ray_tracing_pipeline_builder::pipeline_cache(VkPipelineCache value) {
        cache = value;
        return *this;
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::add_group(shader_group_type type, const std::string &name) {
        VKZ_ASSERT(!name.empty(), "Shader group requires a name");
        VKZ_ASSERT(std::none_of(groups.begin(), groups.end(),
                                [&](const auto &group) {
                                    return group->group_name == name;
                                }),
                   "Duplicate shader group name");
        groups.push_back(std::make_unique<ray_tracing_shader_group_builder>(*this, type, name));
        return *groups.back();
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::ray_generation_group(const std::string &name) {
        return add_group(shader_group_type::ray_generation, name);
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::miss_group(const std::string &name) {
        return add_group(shader_group_type::miss, name);
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::triangles_hit_group(const std::string &name) {
        return add_group(shader_group_type::triangles_hit, name);
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::procedural_hit_group(const std::string &name) {
        return add_group(shader_group_type::procedural_hit, name);
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::callable_group(const std::string &name) {
        return add_group(shader_group_type::callable, name);
    }

    ray_tracing_shader_group_builder &ray_tracing_pipeline_builder::hit_group(std::string_view name) {
        auto found = std::find_if(groups.begin(), groups.end(), [&](const auto &group) {
            return group->group_name == name;
        });
        VKZ_ASSERT(found != groups.end(), "Unknown hit group");
        VKZ_ASSERT((*found)->type == shader_group_type::triangles_hit || (*found)->type == shader_group_type::procedural_hit, "Selected group is not a hit group");
        return **found;
    }

    ray_tracing_shader_group_builder::ray_tracing_shader_group_builder(ray_tracing_pipeline_builder &parent, shader_group_type type, std::string name)
        : ray_tracing_pipeline_builder_proxy(parent), type(type), group_name(std::move(name)) {
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::add_shader(const shader_source &source, vkz::shader_stage shader_type) {
        VKZ_ASSERT(std::none_of(stages.begin(), stages.end(),
                                [&](const auto &value) {
                                    return value.type == shader_type;
                                }),
                   "Duplicate shader stage within a group");
        stages.push_back({shader_type, source});
        return *this;
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::ray_generation_shader(const shader_source &source) {
        return add_shader(source, vkz::shader_stage::raygen);
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::miss_shader(const shader_source &source) {
        return add_shader(source, vkz::shader_stage::miss);
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::closest_hit_shader(const shader_source &source) {
        return add_shader(source, vkz::shader_stage::closest_hit);
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::any_hit_shader(const shader_source &source) {
        return add_shader(source, vkz::shader_stage::any_hit);
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::intersection_shader(const shader_source &source) {
        return add_shader(source, vkz::shader_stage::intersection);
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::callable_shader(const shader_source &source) {
        return add_shader(source, vkz::shader_stage::callable);
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::entry_point(std::string value) {
        VKZ_ASSERT(!stages.empty() && !value.empty(), "Select a shader before its entry point");
        stages.back().entry = std::move(value);
        return *this;
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::record_data(std::span<const uint8_t> bytes) {
        record.assign(bytes.begin(), bytes.end());
        return *this;
    }

    ray_tracing_shader_group_builder &ray_tracing_shader_group_builder::specialization(std::span<const uint8_t> bytes, uint32_t constant_id) {
        VKZ_ASSERT(!stages.empty(), "Select a shader before specialization");
        auto &shader = stages.back();
        VKZ_ASSERT(std::none_of(shader.entries.begin(), shader.entries.end(),
                                [&](const auto &value) {
                                    return value.constantID == constant_id;
                                }),
                   "Duplicate specialization constant");
        VKZ_ASSERT(shader.constants.size() + bytes.size() <= UINT32_MAX, "Specialization data exceeds Vulkan limits");
        shader.entries.push_back({constant_id, VKZ_COUNT(shader.constants), bytes.size()});
        shader.constants.insert(shader.constants.end(), bytes.begin(), bytes.end());
        return *this;
    }

    ray_tracing_pipeline_layout_builder::ray_tracing_pipeline_layout_builder(ray_tracing_pipeline_builder &parent) : ray_tracing_pipeline_builder_proxy(parent) {
    }

    ray_tracing_pipeline_layout_builder &ray_tracing_pipeline_layout_builder::add_descriptor_set_layout(descriptor_set_layout value) {
        parent.layout_storage.layout().add_descriptor_set_layout(value);
        return *this;
    }

    ray_tracing_pipeline_layout_builder &ray_tracing_pipeline_layout_builder::add_push_constant_range(vkz::shader_stage stages, uint32_t offset, uint32_t size) {
        parent.layout_storage.layout().add_push_constant_range(stages, offset, size);
        return *this;
    }

    ray_tracing_pipeline_layout_builder &ray_tracing_pipeline_layout_builder::add_push_constant_range(VkPushConstantRange value) {
        return add_push_constant_range(static_cast<vkz::shader_stage>(value.stageFlags), value.offset, value.size);
    }

    ray_tracing_pipeline_layout_builder &ray_tracing_pipeline_layout_builder::clear() {
        parent.layout_storage.layout().clear();
        return *this;
    }

    void ray_tracing_pipeline_builder::validate(const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &properties) const {
        VKZ_ASSERT(recursion_depth > 0 && recursion_depth <= properties.maxRayRecursionDepth, "Invalid ray recursion depth");
        VKZ_ASSERT(groups.size() <= UINT32_MAX, "Too many shader groups");
        bool ray_generation{};
        for (const auto &group : groups) {
            vkz::shader_stage stages{};
            for (const auto &shader : group->stages)
                stages |= shader.type;
            switch (group->type) {
            case shader_group_type::ray_generation:
                VKZ_ASSERT(stages == vkz::shader_stage::raygen, "Ray generation group requires exactly one ray generation shader");
                ray_generation = true;
                break;
            case shader_group_type::miss:
                VKZ_ASSERT(stages == vkz::shader_stage::miss, "Miss group requires exactly one miss shader");
                break;
            case shader_group_type::callable:
                VKZ_ASSERT(stages == vkz::shader_stage::callable, "Callable group requires exactly one callable shader");
                break;
            case shader_group_type::triangles_hit:
                VKZ_ASSERT((stages & ~(vkz::shader_stage::closest_hit | vkz::shader_stage::any_hit)) == vkz::shader_stage::none, "Triangle group has an invalid shader stage");
                break;
            case shader_group_type::procedural_hit:
                VKZ_ASSERT((stages & vkz::shader_stage::intersection) != vkz::shader_stage::none &&
                               (stages & ~(vkz::shader_stage::closest_hit | vkz::shader_stage::any_hit | vkz::shader_stage::intersection)) == vkz::shader_stage::none,
                           "Procedural group requires an intersection shader");
                break;
            }
        }
        VKZ_ASSERT(ray_generation, "Ray tracing pipeline requires a ray generation group");
    }

    ray_tracing_pipeline ray_tracing_pipeline_builder::build() {
        VKZ_ASSERT(vkCreateRayTracingPipelinesKHR && vkGetRayTracingShaderGroupHandlesKHR, "Ray tracing pipeline feature must be enabled");
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 physical{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &properties};
        vkGetPhysicalDeviceProperties2(allocator.device.physical, &physical);
        validate(properties);
        ray_tracing_pipeline result;
        result.device = allocator.device;
        result.layout = layout_storage.layout().build_pipeline_layout();

        struct shader_modules {
            ~shader_modules() {
                for (auto value : values)
                    vkDestroyShaderModule(device, value, nullptr);
            }

            VkDevice device;
            std::vector<VkShaderModule> values;
        } modules{allocator.device};

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<VkSpecializationInfo> specializations;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> descriptions;
        std::vector<shader_group_record> records;
        size_t stage_count{};
        for (const auto &group : groups)
            stage_count += group->stages.size();
        VKZ_ASSERT(stage_count <= UINT32_MAX, "Too many ray tracing shader stages");
        stages.reserve(stage_count);
        specializations.reserve(stage_count);
        modules.values.reserve(stage_count);
        for (const auto &group : groups) {
            VkRayTracingShaderGroupCreateInfoKHR description{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
            description.type = group->type == shader_group_type::triangles_hit    ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR
                               : group->type == shader_group_type::procedural_hit ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
                                                                                  : VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            description.generalShader = description.closestHitShader = description.anyHitShader = description.intersectionShader = VK_SHADER_UNUSED_KHR;
            for (const auto &shader : group->stages) {
                const auto index = VKZ_COUNT(stages);
                const auto module = std::visit(
                    [&](const auto &source) {
                        return create_shader_module(allocator.device, source);
                    },
                    shader.source);
                modules.values.push_back(module);
                auto &specialization = specializations.emplace_back(VkSpecializationInfo{VKZ_COUNT(shader.entries), shader.entries.data(), shader.constants.size(), shader.constants.data()});
                VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
                stage.stage = static_cast<VkShaderStageFlagBits>(shader.type);
                stage.module = module;
                stage.pName = shader.entry.c_str();
                stage.pSpecializationInfo = shader.entries.empty() ? nullptr : &specialization;
                stages.push_back(stage);
                if (shader.type == vkz::shader_stage::closest_hit)
                    description.closestHitShader = index;
                else if (shader.type == vkz::shader_stage::any_hit)
                    description.anyHitShader = index;
                else if (shader.type == vkz::shader_stage::intersection)
                    description.intersectionShader = index;
                else
                    description.generalShader = index;
            }
            records.push_back({group->type, VKZ_COUNT(descriptions), group->group_name, group->record});
            descriptions.push_back(description);
        }
        VkRayTracingPipelineCreateInfoKHR info{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
        info.stageCount = VKZ_COUNT(stages);
        info.pStages = stages.data();
        info.groupCount = VKZ_COUNT(descriptions);
        info.pGroups = descriptions.data();
        info.layout = result.layout;
        info.maxPipelineRayRecursionDepth = recursion_depth;
        VKZ_CHECK_VULKAN(vkCreateRayTracingPipelinesKHR(allocator.device, VK_NULL_HANDLE, cache, 1, &info, nullptr, &result.handle));
        result.sbt = shader_binding_tables::create(allocator, result.handle, records);
        if (!pipeline_name.empty()) {
            set_name<VK_OBJECT_TYPE_PIPELINE>(allocator.device, pipeline_name, result.handle);
            set_name<VK_OBJECT_TYPE_PIPELINE_LAYOUT>(allocator.device, pipeline_name, result.layout);
        }
        return result;
    }
}
