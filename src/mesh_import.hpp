#pragma once

#include <vulkanizer/mesh.hpp>

namespace vkz::mesh::detail {
    data import_gltf(const vkz::path& source);
    data import_obj(const vkz::path& source);
    void convert_textures_to_ktx(data& meshes);
}
