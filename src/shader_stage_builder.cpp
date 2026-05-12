#include "vulkanizer/util.hpp"
#include "vulkanizer/graphics_pipeline_builder.hpp"
#include "vulkanizer/creators.hpp"
#include "vulkanizer/detail/shader_stage_builder.hpp"


#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace vkz {
    namespace {
        bool is_glsl_source(const std::string& source) {
            const auto first = source.find_first_not_of(" \t\r\n");
            return first != std::string::npos && source.compare(first, 8, "#version") == 0;
        }

        const char* glsl_stage_name(VkShaderStageFlagBits stage) {
            switch (stage) {
                case VK_SHADER_STAGE_VERTEX_BIT:
                    return "vertex";
                case VK_SHADER_STAGE_FRAGMENT_BIT:
                    return "fragment";
                case VK_SHADER_STAGE_GEOMETRY_BIT:
                    return "geometry";
                case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                    return "tesscontrol";
                case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                    return "tesseval";
                default:
                    throw std::runtime_error{"Inline GLSL shader stage is not supported"};
            }
        }

        std::vector<uint32_t> compile_inline_glsl(const std::string& source, VkShaderStageFlagBits stage) {
            const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto base = std::filesystem::temp_directory_path() / ("vkz-inline-shader-" + std::to_string(id));
            const auto source_path = base.string() + ".glsl";
            const auto output_path = base.string() + ".spv";

            {
                std::ofstream file{source_path, std::ios::binary};
                if (!file) {
                    throw std::runtime_error{"Failed to write inline GLSL shader source"};
                }
                file << source;
            }

            const std::string command =
                "glslc -fshader-stage=" + std::string{glsl_stage_name(stage)} +
                " \"" + source_path + "\" -o \"" + output_path + "\"";

            if (std::system(command.c_str()) != 0) {
                std::filesystem::remove(source_path);
                throw std::runtime_error{"Failed to compile inline GLSL shader with glslc"};
            }

            std::ifstream file{output_path, std::ios::binary | std::ios::ate};
            if (!file) {
                std::filesystem::remove(source_path);
                std::filesystem::remove(output_path);
                throw std::runtime_error{"Failed to read compiled inline GLSL shader"};
            }

            const auto size = file.tellg();
            std::vector<uint32_t> spirv(static_cast<size_t>(size) / sizeof(uint32_t));
            file.seekg(0);
            file.read(reinterpret_cast<char*>(spirv.data()), size);

            // std::filesystem::remove(source_path);
            // std::filesystem::remove(output_path);
            return spirv;
        }
    }

    shader_stage_builder::shader_stage_builder(vkz::device device, graphics_pipeline_builder *parent)
            : graphics_pipeline_builder(device, parent) {
        _features.pNext = &_mesh_features;
        vkGetPhysicalDeviceFeatures2(device.physical, &_features);
    }

    shader_stage_builder::shader_stage_builder(shader_stage_builder *parent)
            : graphics_pipeline_builder(parent->_device, parent) {
        _mesh_features = parent->_mesh_features;
    }

    shader_builder &shader_stage_builder::vertex_shader(const shader_source &source) {
        return add_shader(source, VK_SHADER_STAGE_VERTEX_BIT);
    }

    shader_builder &shader_stage_builder::task_shader(const shader_source &source) {
        if (!task_shader_supported()) throw std::runtime_error{"Task Shader not supported"};
        return add_shader(source, VK_SHADER_STAGE_TASK_BIT_EXT);
    }

    shader_builder &shader_stage_builder::mesh_shader(const shader_source &source) {
        if (!mesh_shaderSupported()) throw std::runtime_error{"Mesh Shader not supported"};
        return add_shader(source, VK_SHADER_STAGE_MESH_BIT_EXT);
    }

    shader_builder &shader_stage_builder::fragment_shader(const shader_source &source) {
        return add_shader(source, VK_SHADER_STAGE_FRAGMENT_BIT);
    }


    shader_builder &shader_stage_builder::geometry_shader(const shader_source &source) {
        return add_shader(source, VK_SHADER_STAGE_GEOMETRY_BIT);
    }

    shader_builder &shader_stage_builder::tessellation_evaluation_shader(const shader_source &source) {
        return add_shader(source, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }

    shader_builder &shader_stage_builder::tessellation_control_shader(const shader_source &source) {
        return add_shader(source, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    }

    void shader_stage_builder::validate() const {

        if (!has_vertex_shader()) {
            throw std::runtime_error{"at least vertex/mesh shader should be provided"};
        }

        if (has_tess_control_shader() && !has_tess_eval_shader()) {
            throw std::runtime_error{"tessellation eval shader required if tessellation control shader provided"};
        }
    }

    std::vector<VkPipelineShaderStageCreateInfo> &shader_stage_builder::build_shader_stage() {
        validate();

        for (auto &builder: _shader_builders) {
            auto &stage = builder->build_shader();
            _vk_stages.push_back(stage);
        }

        return _vk_stages;
    }

    shader_stage_builder &shader_stage_builder::clear() {
        _shader_builders.clear();
        return *this;
    }

    void shader_stage_builder::copy(const shader_stage_builder &source) {
        for (auto &sBuilder: source._shader_builders) {
            auto builder = std::make_unique<shader_builder>(this);
            builder->copy(*sBuilder);
            _shader_builders.push_back(std::move(builder));
        }
    }

    shader_builder &
    shader_stage_builder::add_shader(const shader_stage_builder::shader_source &source, VkShaderStageFlagBits stage) {
        auto itr = std::find_if(_shader_builders.begin(), _shader_builders.end(), [&](const auto &builder) {
            return builder->is_stage(stage);
        });
        if (itr != _shader_builders.end()) {
            _shader_builders.erase(itr);
        }
        _shader_builders.push_back(std::make_unique<shader_builder>(source, stage, this));
        return *_shader_builders.back();
    }

    bool shader_stage_builder::has_vertex_shader() const {
        auto vertItr = std::find_if(_shader_builders.begin(), _shader_builders.end(),
                                    [](const auto &builder) { return builder->is_vertex_shader(); });
        auto meshItr = std::find_if(_shader_builders.begin(), _shader_builders.end(),
                                    [](const auto &builder) { return builder->is_mesh_shader(); });
        return vertItr != _shader_builders.end() || meshItr != _shader_builders.end();
    }

    bool shader_stage_builder::has_tess_control_shader() const {
        auto itr = std::find_if(_shader_builders.begin(), _shader_builders.end(),
                                [](const auto &builder) { return builder->is_tess_control_shader(); });
        return itr != _shader_builders.end();
    }

    bool shader_stage_builder::has_tess_eval_shader() const {
        auto itr = std::find_if(_shader_builders.begin(), _shader_builders.end(),
                                [](const auto &builder) { return builder->is_tess_eval_shader(); });
        return itr != _shader_builders.end();
    }

    void shader_stage_builder::clear_stages() {
        _vk_stages.clear();
    }

    bool shader_stage_builder::mesh_shaderSupported() const {
        return static_cast<bool>(_mesh_features.meshShader);
    }

    bool shader_stage_builder::task_shader_supported() const {
        return static_cast<bool>(_mesh_features.taskShader);
    }


    shader_builder::shader_builder(shader_stage_builder *parent)
            : shader_stage_builder(parent) {}

    shader_builder::shader_builder(const shader_source &source, VkShaderStageFlagBits stage, shader_stage_builder *parent)
            : shader_stage_builder(parent) {
        _shader.stage = stage;
        std::visit(overloaded{
                [&](const byte_string& source) { _shader.module = create_shader_module(_device, source); },
                [&](const std::vector<uint32_t>& source) { _shader.module = create_shader_module(_device, source); },
                [&](const std::string &source) {
                    if (is_glsl_source(source)) {
                        _shader.module = create_shader_module(_device, compile_inline_glsl(source, stage));
                    } else {
                        _shader.module = create_shader_module(_device, source);
                    }
                },
        }, source);
    }

    shader_builder::~shader_builder() {
        assert(_device.logical);
        if(_shader.module) {
            vkDestroyShaderModule(_device.logical, _shader.module, nullptr);
        }
    }

    VkPipelineShaderStageCreateInfo &shader_builder::build_shader() {
        _create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        _create_info.stage = _shader.stage;
        _create_info.module = _shader.module;
        _create_info.pName = _shader.entry;

        _specialization.mapEntryCount = _entries.size();
        _specialization.pMapEntries = _entries.data();
        _specialization.dataSize = _data.size();
        _specialization.pData = _data.data();
        _create_info.pSpecializationInfo = &_specialization;

        return _create_info;
    }

    shader_builder &shader_builder::vertex_shader(const shader_stage_builder::shader_source &source) {
        return parent()->vertex_shader(source);
    }

    shader_builder &shader_builder::task_shader(const shader_stage_builder::shader_source &source) {
        return parent()->task_shader(source);
    }

    shader_builder &shader_builder::mesh_shader(const shader_stage_builder::shader_source &source) {
        return parent()->mesh_shader(source);
    }

    shader_builder &shader_builder::fragment_shader(const shader_stage_builder::shader_source &source) {
        return parent()->fragment_shader(source);
    }

    shader_stage_builder *shader_builder::parent() {
        return reinterpret_cast<shader_stage_builder *>(_parent);
    }

    shader_builder &shader_builder::geometry_shader(const shader_stage_builder::shader_source &source) {
        return parent()->geometry_shader(source);
    }

    shader_builder &shader_builder::tessellation_evaluation_shader(const shader_stage_builder::shader_source &source) {
        return parent()->tessellation_evaluation_shader(source);
    }

    shader_builder &shader_builder::tessellation_control_shader(const shader_stage_builder::shader_source &source) {
        return parent()->tessellation_control_shader(source);
    }

    bool shader_builder::is_vertex_shader() const {
        return _shader.stage == VK_SHADER_STAGE_VERTEX_BIT;
    }

    bool shader_builder::is_mesh_shader() const {
        return _shader.stage == VK_SHADER_STAGE_MESH_BIT_EXT;
    }

    bool shader_builder::is_tess_eval_shader() const {
        return _shader.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }

    bool shader_builder::is_tess_control_shader() const {
        return _shader.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }

    void shader_builder::copy(const shader_builder &source) {
        _shader = source._shader;
        _entries = source._entries;
        _data.resize(source._data.size());
        std::memcpy(_data.data(), source._data.data(), source._data.size());
    }

    bool shader_builder::is_stage(VkShaderStageFlagBits stage) const {
        return _shader.stage == stage;
    }

}
