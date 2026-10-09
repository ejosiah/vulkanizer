#include "mesh_import.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace vkz::mesh::detail {
namespace {
    std::string utf8(const path &file) {
        const auto value = file.generic_u8string();
        return {reinterpret_cast<const char *>(value.data()), value.size()};
    }

    struct material_reader : tinyobj::MaterialReader {
        path directory;
        explicit material_reader(path base) : directory(std::move(base)) {}
        bool operator()(const std::string &name, std::vector<tinyobj::material_t> *materials,
                        std::map<std::string, int> *map, std::string *warning, std::string *error) override {
            auto normalized = name;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            std::ifstream file(directory / std::filesystem::u8path(normalized));
            if (!file) {
                *error += "Cannot read MTL: " + name;
                return false;
            }
            tinyobj::LoadMtl(map, materials, &file, warning, error);
            return true;
        }
    };

    void require(bool valid, const std::string &message) {
        if (!valid) throw std::runtime_error("OBJ: " + message);
    }

    uint32 size32(size_t size) {
        require(size <= std::numeric_limits<uint32>::max(), "asset exceeds 32-bit mesh limits");
        return static_cast<uint32>(size);
    }

    glm::vec3 vector3(const std::vector<tinyobj::real_t> &values, int index) {
        require(index >= 0 && size_t(index) < values.size() / 3, "invalid position/normal index");
        glm::vec3 result(values[size_t(index) * 3], values[size_t(index) * 3 + 1], values[size_t(index) * 3 + 2]);
        require(std::isfinite(result.x) && std::isfinite(result.y) && std::isfinite(result.z), "non-finite position/normal");
        return result;
    }

    glm::vec3 unit(glm::vec3 value) {
        const float length = glm::length(value);
        require(std::isfinite(length), "non-finite generated normal");
        return length > 0 ? value / length : glm::vec3(0, 1, 0);
    }

    void put(data &out, float value) {
        require(std::isfinite(value), "non-finite vertex attribute");
        const auto *bytes = reinterpret_cast<const uint8 *>(&value);
        out.vertex_data.insert(out.vertex_data.end(), bytes, bytes + sizeof(value));
    }

    struct textures {
        data &out;
        path directory;
        std::map<std::string, uint32> images;

        void alpha(texture_info &base_color, const std::string &name, const tinyobj::texture_option_t &options) {
            if (name.empty()) return;
            const auto mask = add(name, options);
            const auto &mask_image = out.images[out.textures[mask.index].image_id];
            int width, height, channels;
            using pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
            pixels opacity(stbi_load_from_memory(mask_image.encoded_data.data(), static_cast<int>(mask_image.encoded_data.size()),
                                                &width, &height, &channels, 4), stbi_image_free);
            require(bool(opacity), "cannot decode opacity map " + name);
            std::vector<uint8> rgba(size_t(width) * height * 4, 255);
            if (base_color.index >= 0) {
                require(base_color.scale == mask.scale && base_color.offset == mask.offset, "diffuse and opacity maps must use the same UV transform");
                const auto &base_image = out.images[out.textures[base_color.index].image_id];
                int base_width, base_height;
                pixels color(stbi_load_from_memory(base_image.encoded_data.data(), static_cast<int>(base_image.encoded_data.size()),
                                                  &base_width, &base_height, &channels, 4), stbi_image_free);
                require(bool(color), "cannot decode diffuse map for opacity composition");
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x) {
                        const size_t from = (size_t(y) * base_height / height * base_width + size_t(x) * base_width / width) * 4;
                        const size_t to = (size_t(y) * width + x) * 4;
                        std::copy_n(color.get() + from, 4, rgba.data() + to);
                    }
            }
            for (size_t pixel = 0; pixel < rgba.size() / 4; ++pixel)
                rgba[pixel * 4 + 3] = uint8((uint32(rgba[pixel * 4 + 3]) * opacity.get()[pixel * 4]) / 255);
            image combined;
            combined.uri = mask_image.uri + "#base-color-alpha";
            combined.mime_type = "image/png";
            const auto append = [](void *context, void *bytes, int count) {
                auto &output = *static_cast<std::vector<uint8> *>(context);
                const auto *begin = static_cast<const uint8 *>(bytes);
                output.insert(output.end(), begin, begin + count);
            };
            require(stbi_write_png_to_func(append, &combined.encoded_data, width, height, 4, rgba.data(), width * 4) != 0, "cannot encode diffuse/opacity texture");
            const auto image_id = size32(out.images.size());
            out.images.push_back(std::move(combined));
            const auto sampler_id = out.textures[mask.index].sampler_id;
            base_color = mask;
            base_color.index = static_cast<int32>(size32(out.textures.size()));
            out.textures.push_back({image_id, sampler_id});
        }

        texture_info add(std::string name, const tinyobj::texture_option_t &options) {
            if (name.empty()) return {};
            std::replace(name.begin(), name.end(), '\\', '/');
            const auto file = (directory / std::filesystem::u8path(name)).lexically_normal();
            const auto uri = utf8(file);
            auto found = images.find(uri);
            uint32 id;
            if (found == images.end()) {
                std::ifstream input(file, std::ios::binary | std::ios::ate);
                const auto length = input.tellg();
                require(input && length > 0 && uint64_t(length) <= std::numeric_limits<uint32>::max(), "cannot read texture " + uri);
                image value;
                value.uri = uri;
                value.encoded_data.resize(static_cast<size_t>(length));
                input.seekg(0);
                input.read(reinterpret_cast<char *>(value.encoded_data.data()), length);
                require(bool(input), "truncated texture " + uri);
                const auto &bytes = value.encoded_data;
                if (bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') value.mime_type = "image/png";
                else if (bytes.size() >= 3 && bytes[0] == 255 && bytes[1] == 216 && bytes[2] == 255) value.mime_type = "image/jpeg";
                else throw std::runtime_error("OBJ: texture must be PNG or JPEG: " + uri);
                id = size32(out.images.size());
                images.emplace(uri, id);
                out.images.push_back(std::move(value));
            } else id = found->second;
            sampler sampling;
            if (options.clamp) sampling.wrap_s = sampling.wrap_t = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            const auto sampler_id = size32(out.samplers.size());
            out.samplers.push_back(sampling);
            texture_info result;
            result.index = static_cast<int32>(size32(out.textures.size()));
            out.textures.push_back({id, static_cast<int32>(sampler_id)});
            result.scale = {options.scale[0], options.scale[1]};
            // OBJ V runs bottom-to-top; the reference renderer samples unflipped images.
            result.offset = {options.origin_offset[0], 1 - options.scale[1] - options.origin_offset[1]};
            return result;
        }
    };
}

data import_obj(const path &source) {
    std::ifstream file(source);
    require(bool(file), "cannot read source " + utf8(source));
    material_reader reader(source.parent_path());
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning, error;
    const bool loaded = tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, &file, &reader, true, true);
    require(loaded && error.empty(), error);
    require(warning.find("Failed to load material") == std::string::npos &&
            warning.find("not found in .mtl") == std::string::npos, warning);
    data out;
    const bool uv = !attributes.texcoords.empty();
    const uint32 stride = uv ? 64 : 56;
    out.streams.bindings = {{0, stride, VK_VERTEX_INPUT_RATE_VERTEX}};
    out.streams.attributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
                             {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 24}, {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 40}};
    if (uv) out.streams.attributes.push_back({4, 0, VK_FORMAT_R32G32_SFLOAT, 56});
    textures images{out, source.parent_path(), {}};
    for (const auto &input : materials) {
        material value;
        value.base_color_factor = {input.diffuse[0], input.diffuse[1], input.diffuse[2], input.dissolve};
        value.emissive_factor = {input.emission[0], input.emission[1], input.emission[2]};
        value.emissive_strength = std::max({1.0f, value.emissive_factor.x, value.emissive_factor.y, value.emissive_factor.z});
        value.emissive_factor /= value.emissive_strength;
        value.specular_color_factor = {input.specular[0], input.specular[1], input.specular[2]};
        value.metallic_factor = std::clamp(float(input.metallic), 0.0f, 1.0f);
        value.roughness_factor = input.roughness > 0 ? std::clamp(float(input.roughness), 0.0f, 1.0f)
            : std::clamp(std::sqrt(2.0f / (std::max(0.0f, float(input.shininess)) + 2.0f)), 0.0f, 1.0f);
        value.ior = input.ior > 0 ? float(input.ior) : 1.5f;
        if (input.dissolve < 1) value.alpha_mode = alpha_mode::blend;
        value.base_color_texture = images.add(input.diffuse_texname, input.diffuse_texopt);
        images.alpha(value.base_color_texture, input.alpha_texname, input.alpha_texopt);
        if (!input.alpha_texname.empty() && input.dissolve >= 1) value.alpha_mode = alpha_mode::mask;
        // An emission map multiplied by zero cannot affect the material. Some
        // Bistro MTL entries reference missing images in precisely this case.
        if (value.emissive_factor != glm::vec3(0))
            value.emissive_texture = images.add(input.emissive_texname, input.emissive_texopt);
        value.specular_color_texture = images.add(input.specular_texname, input.specular_texopt);
        value.normal_texture = images.add(input.normal_texname, input.normal_texopt);
        // Bistro's DDNA files contain tangent-space normals in RGB, unlike scalar OBJ bump maps.
        if (input.normal_texname.empty() && input.bump_texname.find("_ddna.") != std::string::npos)
            value.normal_texture = images.add(input.bump_texname, input.bump_texopt);
        require(uv || (value.base_color_texture.index < 0 && value.emissive_texture.index < 0 &&
                       value.specular_color_texture.index < 0 && value.normal_texture.index < 0), "textured material has no UVs");
        out.materials.push_back(value);
    }
    const uint32 fallback = size32(out.materials.size());
    material default_material;
    default_material.metallic_factor = 0;
    out.materials.push_back(default_material);

    for (const auto &shape : shapes) {
        const auto &mesh = shape.mesh;
        std::map<int, std::vector<size_t>> groups;
        std::map<std::pair<int, uint32>, glm::vec3> smooth;
        const bool generate_normals = std::any_of(mesh.indices.begin(), mesh.indices.end(),
            [](const auto &index) { return index.normal_index < 0; });
        std::vector<glm::vec3> face_normals(mesh.num_face_vertices.size());
        size_t cursor = 0;
        for (size_t face = 0; face < mesh.num_face_vertices.size(); ++face) {
            require(mesh.num_face_vertices[face] == 3 && cursor + 3 <= mesh.indices.size(), "non-triangle or invalid face");
            require(face < mesh.material_ids.size() && face < mesh.smoothing_group_ids.size(), "invalid face metadata");
            const auto a = vector3(attributes.vertices, mesh.indices[cursor].vertex_index);
            const auto b = vector3(attributes.vertices, mesh.indices[cursor + 1].vertex_index);
            const auto c = vector3(attributes.vertices, mesh.indices[cursor + 2].vertex_index);
            const auto normal = glm::cross(b - a, c - a);
            face_normals[face] = unit(normal);
            const auto smoothing = mesh.smoothing_group_ids[face];
            if (smoothing && generate_normals)
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto key = std::make_pair(mesh.indices[cursor + corner].vertex_index, smoothing);
                    auto [it, inserted] = smooth.try_emplace(key, glm::vec3(0));
                    it->second += normal;
                }
            const int material_id = mesh.material_ids[face];
            require(material_id >= -1 && (material_id < 0 || size_t(material_id) < materials.size()), "invalid material index");
            groups[material_id].push_back(face);
            cursor += 3;
        }
        for (const auto &[material_id, faces] : groups) {
            base output;
            output.vertex_offset = size32(out.vertex_data.size() / stride);
            output.index_offset = size32(out.index_data.size());
            output.material_id = material_id < 0 ? fallback : uint32(material_id);
            using key_type = std::tuple<int, int, int, uint64_t>;
            std::map<key_type, uint32> vertices;
            glm::vec3 minimum(std::numeric_limits<float>::max()), maximum(std::numeric_limits<float>::lowest());
            for (const auto face : faces) {
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto index = mesh.indices[face * 3 + corner];
                    const auto smoothing = mesh.smoothing_group_ids[face];
                    const uint64_t normal_id = index.normal_index >= 0 ? 0 : smoothing ? smoothing : uint64_t(face) + (uint64_t(1) << 32);
                    auto [it, inserted] = vertices.try_emplace({index.vertex_index, index.normal_index, index.texcoord_index, normal_id}, size32(vertices.size()));
                    out.index_data.push_back(it->second);
                    if (!inserted) continue;
                    const auto position = vector3(attributes.vertices, index.vertex_index);
                    const auto normal = index.normal_index >= 0 ? unit(vector3(attributes.normals, index.normal_index))
                        : smoothing ? unit(smooth.at({index.vertex_index, smoothing})) : face_normals[face];
                    minimum = glm::min(minimum, position);
                    maximum = glm::max(maximum, position);
                    for (int c = 0; c < 3; ++c) put(out, position[c]);
                    for (int c = 0; c < 3; ++c) put(out, normal[c]);
                    for (int c = 0; c < 4; ++c) put(out, 0); // Derivative tangent frame.
                    const auto color = attributes.colors.empty() ? glm::vec3(1) : vector3(attributes.colors, index.vertex_index);
                    for (int c = 0; c < 3; ++c) put(out, color[c]);
                    put(out, 1);
                    if (uv) {
                        glm::vec2 texcoord(0);
                        if (index.texcoord_index >= 0) {
                            require(size_t(index.texcoord_index) < attributes.texcoords.size() / 2, "invalid UV index");
                            texcoord = {attributes.texcoords[size_t(index.texcoord_index) * 2], 1 - attributes.texcoords[size_t(index.texcoord_index) * 2 + 1]};
                        }
                        put(out, texcoord.x);
                        put(out, texcoord.y);
                    }
                }
            }
            output.vertex_count = size32(vertices.size());
            output.lod_offset[1] = size32(out.index_data.size() - output.index_offset);
            out.instances.push_back({.mesh_id = size32(out.meshes.size())});
            out.meshes.push_back(output);
            out.boxes.emplace_back(minimum, maximum);
        }
    }
    require(!out.meshes.empty(), "source contains no triangle meshes");
    size32(out.vertex_data.size());
    size32(out.index_data.size() * sizeof(uint32));
    return out;
}
}
