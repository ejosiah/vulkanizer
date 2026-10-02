#include "mesh_import.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace vkz::mesh {
conversion_files convert(const vkz::path& source, const vkz::path& output_base) {
    using importer = data (*)(const vkz::path&);
    static const std::pair<std::string_view, importer> converters[] = {
        {".gltf", detail::import_gltf}, {".glb", detail::import_gltf},
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
