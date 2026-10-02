#include <vulkanizer/reference.hpp>
#include <vulkanizer/status.hpp>
#include <algorithm>
#include <cmath>

namespace vkz::reference {
    std::vector<light_uniform> make_light_uniforms(const mesh::data &scene) {
        std::vector<light_uniform> result;
        result.reserve(std::max(size_t(1), scene.lights.size()));
        for (const auto &light : scene.lights) {
            result.push_back({glm::vec4(light.position, light.range),
                              glm::vec4(light.direction, float(light.type)),
                              glm::vec4(light.color * light.intensity, 0),
                              {std::cos(light.inner_cone_angle), std::cos(light.outer_cone_angle), 0, 0}});
        }
        if (result.empty()) result.emplace_back();
        return result;
    }

    uint32_t vertex_stride(const mesh::data &scene) {
        VKZ_ASSERT(scene.streams.bindings.size() == 1, "Reference pipelines require one interleaved vertex stream");
        const auto &binding = scene.streams.bindings.front();
        VKZ_ASSERT(binding.binding == 0 && binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX && binding.stride >= 56 && binding.stride % sizeof(float) == 0,
                   "Reference pipelines require the canonical float vertex layout");
        constexpr std::array<uint32_t, 4> offsets{0, 12, 24, 40};
        for (uint32_t location = 0; location < offsets.size(); ++location) {
            const auto expected = location < 2 ? VK_FORMAT_R32G32B32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
            const auto attribute = std::find_if(scene.streams.attributes.begin(), scene.streams.attributes.end(), [&](const auto &value) {
                return value.location == location && value.binding == 0 && value.offset == offsets[location] && value.format == expected;
            });
            VKZ_ASSERT(attribute != scene.streams.attributes.end(), "Reference pipelines require position, normal, tangent and color attributes");
        }
        return binding.stride / sizeof(float);
    }

    uint32_t material_index(const mesh::data &scene, const mesh::instance &instance) {
        VKZ_ASSERT(instance.mesh_id < scene.meshes.size(), "Invalid reference mesh index");
        const auto id = instance.material_id == mesh::inherit_material ? scene.meshes[instance.mesh_id].material_id : instance.material_id;
        VKZ_ASSERT(id < scene.materials.size(), "Invalid reference material index");
        return id;
    }

    material_uniform make_material_uniform(const mesh::data &scene, const mesh::material &material) {
        const auto stride = vertex_stride(scene);
        material_uniform result;
        result.base_color = material.base_color_factor;
        result.emissive_metallic = glm::vec4(material.emissive_factor * material.emissive_strength, material.metallic_factor);
        result.roughness_normal_occlusion_cutoff = {material.roughness_factor, material.normal_scale, material.occlusion_strength, material.alpha_cutoff};
        result.optical = {material.ior, material.transmission_factor, material.thickness_factor, material.dispersion};
        result.attenuation = glm::vec4(material.attenuation_color, material.attenuation_distance);
        result.flags = {int(material.alpha_mode), int(material.double_sided), int(material.unlit), 0};
        result.specular = glm::vec4(material.specular_color_factor, material.specular_factor);
        result.clearcoat = {material.clearcoat_factor, material.clearcoat_roughness_factor, material.clearcoat_normal_scale, 0};
        result.sheen = glm::vec4(material.sheen_color_factor, material.sheen_roughness_factor);
        result.anisotropy = {material.anisotropy_strength, std::cos(material.anisotropy_rotation), std::sin(material.anisotropy_rotation), 0};
        result.iridescence = {material.iridescence_factor, material.iridescence_ior, material.iridescence_thickness_minimum, material.iridescence_thickness_maximum};
        const auto slots = mesh::texture_slots(material);
        for (size_t i = 0; i < slots.size(); ++i) {
            const auto &slot = *slots[i];
            if (slot.index < 0) continue;
            VKZ_ASSERT(static_cast<size_t>(slot.index) < scene.textures.size(), "Invalid reference texture index");
            const auto attribute = std::find_if(scene.streams.attributes.begin(), scene.streams.attributes.end(), [&](const auto &value) {
                return value.location >= 4 && value.location - 4 == slot.tex_coord;
            });
            VKZ_ASSERT(attribute != scene.streams.attributes.end() && attribute->binding == 0 && attribute->format == VK_FORMAT_R32G32_SFLOAT && attribute->offset % sizeof(float) == 0 &&
                           attribute->offset / sizeof(float) + 2 <= stride,
                       "Missing or invalid reference texture coordinate stream");
            const bool transformed = slot.offset != glm::vec2(0) || slot.scale != glm::vec2(1) || slot.rotation != 0;
            const float c = std::cos(slot.rotation), s = std::sin(slot.rotation);
            result.textures[i] = {int(attribute->offset / sizeof(float)), 1, slot.index + 1, int(transformed)};
            result.uv_transform[i] = {c * slot.scale.x, s * slot.scale.y, -s * slot.scale.x, c * slot.scale.y};
            result.uv_offset[i] = glm::vec4(slot.offset, 0, 0);
        }
        return result;
    }
}
