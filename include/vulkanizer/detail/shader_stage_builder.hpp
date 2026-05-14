#pragma once

#include "types.hpp"

#include <tuple>
#include <variant>
#include <map>

namespace vkz {

    class shader_builder;

    class shader_stage_builder : public graphics_pipeline_builder {
    public:
        using shader_source = std::variant<byte_string, std::vector<uint32_t>, std::string>;

        shader_stage_builder(vkz::device device, graphics_pipeline_builder *parent);

        explicit shader_stage_builder(shader_stage_builder *parent);

        [[maybe_unused]]
        virtual shader_builder &vertex_shader(const shader_source &source);

        [[maybe_unused]]
        virtual shader_builder &task_shader(const shader_source &source);

        [[maybe_unused]]
        virtual shader_builder &mesh_shader(const shader_source &source);

        [[maybe_unused]]
        virtual shader_builder &fragment_shader(const shader_source &source);

        [[maybe_unused]]
        virtual shader_builder &geometry_shader(const shader_source &source);

        [[maybe_unused]]
        virtual shader_builder &tessellation_evaluation_shader(const shader_source &source);

        [[maybe_unused]]
        virtual shader_builder &tessellation_control_shader(const shader_source &source);

        shader_stage_builder &clear();

        void validate() const;

        void copy(const shader_stage_builder &source);

        [[nodiscard]]
        std::vector<VkPipelineShaderStageCreateInfo> &build_shader_stage();

        void clear_stages();

    protected:
        shader_builder &add_shader(const shader_source &source, VkShaderStageFlagBits stage);

        bool has_vertex_shader() const;

        bool has_tess_control_shader() const;

        bool has_tess_eval_shader() const;

        bool mesh_shaderSupported() const;

        bool task_shader_supported() const;

    private:
        std::vector<VkPipelineShaderStageCreateInfo> _vk_stages;
        std::vector<std::unique_ptr<shader_builder>> _shader_builders;
        VkPhysicalDeviceFeatures2 _features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceMeshShaderFeaturesEXT _mesh_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    };

    class shader_builder : public shader_stage_builder {
    public:
        explicit shader_builder(shader_stage_builder *parent);

        shader_builder(const shader_source &source, VkShaderStageFlagBits stage, shader_stage_builder *parent);

        ~shader_builder() override;

        template<typename T>
        shader_builder &add_specialization(T value, uint32_t constant_id) {
            auto dataSize = sizeof(value);
            VkSpecializationMapEntry entry{constant_id, _offset, dataSize};

            auto bytes = reinterpret_cast<char *>(&value);
            _data.insert(_data.end(), bytes, bytes + dataSize);
            _offset = _data.size();
            _entries.push_back(entry);
            return *this;
        }

        shader_stage_builder *parent() override;

        shader_builder &vertex_shader(const shader_source &source) override;

        shader_builder &task_shader(const shader_source &source) override;

        shader_builder &mesh_shader(const shader_source &source) override;

        shader_builder &fragment_shader(const shader_source &source) override;

        shader_builder &geometry_shader(const shader_source &source) override;

        shader_builder &tessellation_evaluation_shader(const shader_source &source) override;

        shader_builder &tessellation_control_shader(const shader_source &source) override;

        VkPipelineShaderStageCreateInfo &build_shader();

        bool is_vertex_shader() const;

        bool is_mesh_shader() const;

        bool is_tess_eval_shader() const;

        bool is_tess_control_shader() const;

        bool is_stage(VkShaderStageFlagBits stage) const;

        void copy(const shader_builder &source);

    private:
        shader_info _shader;
        std::vector<VkSpecializationMapEntry> _entries;
        std::vector<char> _data;
        uint32_t _offset{};
        VkPipelineShaderStageCreateInfo _create_info{};
        VkSpecializationInfo _specialization{};
    };
}