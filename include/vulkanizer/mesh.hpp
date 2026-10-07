#pragma once

#include "types.hpp"
#include "enum_bit_flags.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <numbers>
#include <string>
#include <type_traits>
#include <vector>

namespace vkz::mesh {

    constexpr uint32 max_lods = 7;
    constexpr uint32 inherit_material = std::numeric_limits<uint32>::max();
    inline constexpr const char *mesh_extension = ".vkmesh";
    inline constexpr const char *material_extension = ".vkmat";
    inline constexpr const char *bounds_extension = ".vkbounds";

    struct conversion_files {
        vkz::path mesh_file;
        vkz::path material_file;
        vkz::path bounds_file;
    };

    // Dispatch by source extension (.gltf/.glb, case-insensitive). An empty
    // output_base uses the source path; its extension is replaced for each output.
    // Throws on unsupported input or I/O failure. See docs/mesh.md for the layout.
    conversion_files convert(const vkz::path &source, const vkz::path &output_base = {});

    struct base {
        uint32 lod_count{1};
        uint32 index_offset{};
        uint32 vertex_offset{};
        uint32 vertex_count{};
        std::array<uint32, max_lods + 1> lod_offset{};
        uint32 material_id{};

        [[nodiscard]] inline uint32 get_lod_indices_count(uint32 lod) const {
            return lod < lod_count ? lod_offset[lod + 1] - lod_offset[lod] : 0;
        }
    };

    struct header {
        uint32 magic_value{0x475A4B56}; // "VKZG" (Vulkanizer geometry) on little-endian hosts.
        uint32 mesh_count{};
        uint32 index_data_size{};
        uint32 vertex_data_size{};
    };

    // One placement of a base mesh. IDs index data::meshes and data::materials.
    // Keep model_inverse in sync when changing model.
    struct instance {
        glm::mat4 model{1.0f};
        glm::mat4 model_inverse{1.0f};
        uint32 mesh_id{};
        uint32 material_id{inherit_material}; // Use base::material_id unless overridden.
    };

    enum class light_type : uint32 {
        directional,
        point,
        spot
    };

    struct light {
        light_type type{light_type::point};
        glm::vec3 color{1.0f};                  // Linear RGB.
        float intensity{1.0f};                  // Candela for point/spot, lux for directional.
        float range{};                          // Zero means infinite; ignored for directional lights.
        glm::vec3 position{};                   // World space; ignored for directional lights.
        glm::vec3 direction{0.0f, 0.0f, -1.0f}; // Unit emission direction; ignored for point lights.
        float inner_cone_angle{};               // Spot half-angles, in radians.
        float outer_cone_angle{std::numbers::pi_v<float> / 4.0f};
        uint32 mesh_id{std::numeric_limits<uint32>::max()};
        glm::mat4 model{1.0f};
    };

    enum class material_flags : uint32 {
        cast_shadow = 0x1,
        receive_shadow = 0x2,
    };

}

template <> struct enable_bitmask_operators<vkz::mesh::material_flags> {
    static constexpr bool enable = true;
};

namespace vkz::mesh {

    enum class alpha_mode : uint32 {
        opaque,
        mask,
        blend
    };

    struct texture_info {
        int32 index{-1};    // data::textures; -1 means no texture.
        uint32 tex_coord{}; // Resolved UV set, including any importer override.
        glm::vec2 offset{};
        glm::vec2 scale{1.0f};
        float rotation{}; // Radians; apply offset + rotation * (scale * uv).
    };

    // Converted images contain complete KTX2 files in encoded_data, stored in
    // .vkmesh. URI/other encoded formats remain supported for legacy/import data.
    struct image {
        std::string uri;
        std::string mime_type;
        std::vector<uint8_t> encoded_data;
    };

    struct sampler {
        VkFilter mag_filter{VK_FILTER_LINEAR};
        VkFilter min_filter{VK_FILTER_LINEAR};
        VkSamplerMipmapMode mipmap_mode{VK_SAMPLER_MIPMAP_MODE_LINEAR};
        VkSamplerAddressMode wrap_s{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        VkSamplerAddressMode wrap_t{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        bool use_mipmaps{true}; // False represents glTF NEAREST/LINEAR minification.
    };

    struct texture {
        uint32 image_id{};    // data::images
        int32 sampler_id{-1}; // data::samplers; -1 uses sampler{} defaults.
    };

    struct material {
        glm::vec4 base_color_factor{1.0f};
        glm::vec3 emissive_factor{0.0f};
        float metallic_factor{1.0f};
        float roughness_factor{1.0f};
        float ior{1.5f};
        float transmission_factor{};
        float thickness_factor{};
        float attenuation_distance{std::numeric_limits<float>::infinity()};
        glm::vec3 attenuation_color{1};
        float specular_factor{1};
        glm::vec3 specular_color_factor{1};
        float emissive_strength{1};
        bool unlit{false};
        float clearcoat_factor{};
        float clearcoat_roughness_factor{};
        float clearcoat_normal_scale{1};
        glm::vec3 sheen_color_factor{0};
        float sheen_roughness_factor{};
        float anisotropy_strength{};
        float anisotropy_rotation{};
        float iridescence_factor{};
        float iridescence_ior{1.3f};
        float iridescence_thickness_minimum{100};
        float iridescence_thickness_maximum{400};
        float dispersion{};
        float normal_scale{1.0f};
        float occlusion_strength{1.0f};
        vkz::mesh::alpha_mode alpha_mode{alpha_mode::opaque};
        float alpha_cutoff{0.5f};
        bool double_sided{false};
        // Base color RGB and emissive RGB use sRGB texture decoding. Alpha is
        // linear; metallic (B), roughness (G), normal (RGB), occlusion (R) are linear.
        texture_info base_color_texture;
        texture_info metallic_roughness_texture;
        texture_info normal_texture;
        texture_info occlusion_texture;
        texture_info emissive_texture;
        texture_info transmission_texture;
        texture_info thickness_texture;
        texture_info specular_texture;
        texture_info specular_color_texture;
        texture_info clearcoat_texture;
        texture_info clearcoat_roughness_texture;
        texture_info clearcoat_normal_texture;
        texture_info sheen_color_texture;
        texture_info sheen_roughness_texture;
        texture_info anisotropy_texture;
        texture_info iridescence_texture;
        texture_info iridescence_thickness_texture;

        material_flags flags{material_flags::cast_shadow | material_flags::receive_shadow};
    };

    inline constexpr size_t material_texture_count = 17;

    template <typename Material> auto texture_slots(Material &value) {
        return std::array{&value.base_color_texture,
                          &value.metallic_roughness_texture,
                          &value.normal_texture,
                          &value.occlusion_texture,
                          &value.emissive_texture,
                          &value.transmission_texture,
                          &value.thickness_texture,
                          &value.specular_texture,
                          &value.specular_color_texture,
                          &value.clearcoat_texture,
                          &value.clearcoat_roughness_texture,
                          &value.clearcoat_normal_texture,
                          &value.sheen_color_texture,
                          &value.sheen_roughness_texture,
                          &value.anisotropy_texture,
                          &value.iridescence_texture,
                          &value.iridescence_thickness_texture};
    }

    // Owned descriptions accepted by vertex_input_state_builder's bulk setters.
    struct vertex_input {
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };

    struct bounding_box {
        glm::vec3 min{};
        glm::vec3 max{};

        bounding_box() = default;

        bounding_box(const glm::vec3 &min, const glm::vec3 &max) : min(glm::min(min, max)), max(glm::max(min, max)) {
        }

        bounding_box(const glm::vec3 *points, size_t num_points) : min(std::numeric_limits<float>::max()), max(std::numeric_limits<float>::lowest()) {
            for (size_t i = 0; i < num_points; ++i) {
                combine_point(points[i]);
            }
        }

        [[nodiscard]] glm::vec3 get_size() const {
            return max - min;
        }

        [[nodiscard]] glm::vec3 get_center() const {
            return 0.5f * (min + max);
        }

        void combine_point(const glm::vec3 &point) {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        void transform(const glm::mat4 &matrix) {
            std::array<glm::vec3, 8> corners{
                glm::vec3{min.x, min.y, min.z}, glm::vec3{min.x, max.y, min.z}, glm::vec3{min.x, min.y, max.z}, glm::vec3{min.x, max.y, max.z},
                glm::vec3{max.x, min.y, min.z}, glm::vec3{max.x, max.y, min.z}, glm::vec3{max.x, min.y, max.z}, glm::vec3{max.x, max.y, max.z},
            };
            for (auto &corner : corners) {
                corner = glm::vec3(matrix * glm::vec4(corner, 1.0f));
            }
            *this = bounding_box(corners.data(), corners.size());
        }

        [[nodiscard]] bounding_box get_transformed(const glm::mat4 &matrix) const {
            auto result = *this;
            result.transform(matrix);
            return result;
        }
    };

    // Geometry, materials, placements of shared meshes, and placed lights.
    struct data {
        vertex_input streams{};
        std::vector<uint32> index_data;
        std::vector<uint8> vertex_data;
        std::vector<base> meshes;
        std::vector<instance> instances;
        std::vector<light> lights;
        std::vector<bounding_box> boxes;
        std::vector<material> materials;
        std::vector<image> images;
        std::vector<sampler> samplers;
        std::vector<texture> textures;

        [[nodiscard]] header get_mesh_file_header() const {
            return {
                .mesh_count = static_cast<uint32>(meshes.size()),
                .index_data_size = static_cast<uint32>(index_data.size() * sizeof(uint32)),
                .vertex_data_size = static_cast<uint32>(vertex_data.size()),
            };
        }
    };

    static_assert(sizeof(bounding_box) == sizeof(float) * 6);

    // I/O throws std::runtime_error on failure. Geometry files use a versioned
    // Vulkan layout block and are not binary-compatible with cookbook LVK files.
    // .vkmesh stores geometry, instances, lights and image payloads. .vkmat stores
    // materials, samplers and texture references. Older cookbook files must be
    // regenerated. Legacy .vkmat version 1 image payloads remain readable.
    bool is_mesh_data_valid(const vkz::path &file_name);

    bool is_mesh_materials_valid(const vkz::path &file_name);

    bool is_mesh_hierarchy_valid(const vkz::path &file_name);

    header load_mesh_data(const vkz::path &mesh_file, data &out);

    void load_mesh_data_materials(const vkz::path &mesh_file, data &out);

    void save_mesh_data(const vkz::path &file_name, const data &meshes);

    void save_mesh_data_materials(const vkz::path &file_name, const data &meshes);

    void save_bounding_boxes(const vkz::path &file_name, const std::vector<bounding_box> &boxes);

    void load_bounding_boxes(const vkz::path &file_name, std::vector<bounding_box> &boxes);

    // Requires one interleaved vertex binding and float3 position at location 0.
    void recalculate_bounding_boxes(data &meshes);

    // Replaces meshes with merged sources, including materials and texture references.
    header merge_mesh_data(data &meshes, const std::vector<data *> sources);

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    inline void put(std::vector<uint8_t> &bytes, const T &value) {
        // Copy first so value may safely refer to storage invalidated by resize.
        std::array<unsigned char, sizeof(T)> copy;
        std::memcpy(copy.data(), &value, sizeof(T));
        const auto offset = bytes.size();
        bytes.resize(offset + sizeof(T));
        std::memcpy(bytes.data() + offset, copy.data(), sizeof(T));
    }
}
