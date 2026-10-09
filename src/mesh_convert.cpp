#include "mesh_import.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace vkz::mesh {
conversion_files convert(const vkz::path& source, const vkz::path& output_base, float scale) {
    if (!std::isfinite(scale) || scale <= 0) throw std::invalid_argument("Mesh conversion scale must be positive and finite");
    using importer = data (*)(const vkz::path&);
    static const std::pair<std::string_view, importer> converters[] = {
        {".gltf", detail::import_gltf}, {".glb", detail::import_gltf},
        {".obj", detail::import_obj},
    };
    auto extension = source.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    const auto converter = std::find_if(std::begin(converters), std::end(converters),
        [&](const auto& entry) { return entry.first == extension; });
    if (converter == std::end(converters))
        throw std::runtime_error("No mesh converter for extension: " + extension);

    // Finish import and semantic validation before opening any output files.
    auto meshes = converter->second(source);
    if (scale != 1) {
        const auto position = std::find_if(meshes.streams.attributes.begin(), meshes.streams.attributes.end(),
            [](const auto &attribute) { return attribute.location == 0; });
        if (!meshes.vertex_data.empty()) {
            if (position == meshes.streams.attributes.end() || position->format != VK_FORMAT_R32G32B32_SFLOAT ||
                meshes.streams.bindings.size() != 1 || position->binding != meshes.streams.bindings[0].binding)
                throw std::runtime_error("Mesh scaling requires one interleaved float3 position stream");
            const auto stride = meshes.streams.bindings[0].stride;
            for (size_t vertex = 0; vertex < meshes.vertex_data.size(); vertex += stride)
                for (size_t component = 0; component < 3; ++component) {
                    auto *bytes = meshes.vertex_data.data() + vertex + position->offset + component * sizeof(float);
                    float value;
                    std::memcpy(&value, bytes, sizeof(value));
                    value *= scale;
                    if (!std::isfinite(value)) throw std::runtime_error("Mesh position overflow during scaling");
                    std::memcpy(bytes, &value, sizeof(value));
                }
        }
        for (auto &box : meshes.boxes) {
            box.min *= scale;
            box.max *= scale;
        }
        auto scale_translation = [scale](glm::mat4 &model) {
            for (int c = 0; c < 3; ++c) model[3][c] *= scale;
        };
        for (auto &instance : meshes.instances) {
            scale_translation(instance.model);
            instance.model_inverse = glm::inverse(instance.model);
        }
        for (auto &light : meshes.lights) {
            light.position *= scale;
            light.range *= scale;
            scale_translation(light.model);
        }
    }
    detail::convert_textures_to_ktx(meshes);
    const auto base = output_base.empty() ? source : output_base;
    conversion_files files{base, base, base};
    files.mesh_file.replace_extension(mesh_extension);
    files.material_file.replace_extension(material_extension);
    files.bounds_file.replace_extension(bounds_extension);
    if (!base.parent_path().empty()) std::filesystem::create_directories(base.parent_path());
    save_mesh_data_materials(files.material_file, meshes);
    save_mesh_data(files.mesh_file, meshes);
    save_bounding_boxes(files.bounds_file, meshes.boxes);
    return files;
}
}
