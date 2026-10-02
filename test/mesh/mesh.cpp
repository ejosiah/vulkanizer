#include <vulkanizer/mesh.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void check(bool value) {
        if (!value) throw std::runtime_error("Mesh check failed");
    }

    template <typename F> void rejects(F action) {
        try {
            action();
        } catch (const std::runtime_error &) {
            return;
        }
        throw std::runtime_error("Expected invalid data to be rejected");
    }

    vkz::mesh::data triangle(float x) {
        using namespace vkz::mesh;
        data result;
        // Position has a nonzero byte offset; vertex_offset skips a padding vertex.
        result.streams.bindings = {{0, 16, VK_VERTEX_INPUT_RATE_VERTEX}};
        result.streams.attributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 4}};
        for (const auto &p : {glm::vec3(99), glm::vec3(x, 0, 0), glm::vec3(x + 2, 0, 0), glm::vec3(x, 3, 0)}) {
            put(result.vertex_data, 0.0f);
            put(result.vertex_data, p);
        }
        result.index_data = {0, 1, 2};
        base mesh;
        mesh.vertex_offset = 1;
        mesh.vertex_count = 3;
        mesh.lod_offset[1] = 3;
        result.meshes.push_back(mesh);
        result.materials.emplace_back();
        result.materials.front().base_color_texture.index = 0;
        result.images.push_back({.uri = "textures/albedo.png"});
        result.samplers.emplace_back();
        result.textures.push_back({.image_id = 0, .sampler_id = 0});
        recalculate_bounding_boxes(result);
        return result;
    }
}

int main() {
    using namespace vkz::mesh;
    const auto dir = std::filesystem::temp_directory_path() / ("vkz-mesh-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);

    struct cleanup {
        std::filesystem::path path;

        ~cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } guard{dir};

    try {
        const vkz::path geometry = dir / u8"mesh-\u03bb.bin";
        const vkz::path materials = dir / "materials.bin";
        const vkz::path bounds = dir / "bounds.bin";
        auto first = triangle(0);
        first.instances.emplace_back();
        first.instances[0].model[3].x = 5;
        first.instances[0].model_inverse[3].x = -5;
        check(first.boxes[0].min == glm::vec3(0));
        check(first.boxes[0].max == glm::vec3(2, 3, 0));
        save_mesh_data(geometry, first);
        save_mesh_data_materials(materials, first);
        save_bounding_boxes(bounds, first.boxes);
        check(is_mesh_data_valid(geometry));
        {
            std::fstream file(geometry, std::ios::binary | std::ios::in | std::ios::out);
            uint32_t magic{};
            file.read(reinterpret_cast<char *>(&magic), sizeof(magic));
            check(magic == 0x475A4B56);
            const uint32_t cookbook_magic = 0x12345678;
            file.seekp(0);
            file.write(reinterpret_cast<const char *>(&cookbook_magic), sizeof(cookbook_magic));
        }
        check(!is_mesh_data_valid(geometry));
        data rejected;
        rejects([&] {
            load_mesh_data(geometry, rejected);
        });
        save_mesh_data(geometry, first);
        check(is_mesh_materials_valid(materials));
        check(is_mesh_hierarchy_valid(geometry));
        data loaded;
        load_mesh_data_materials(materials, loaded);
        const auto info = load_mesh_data(geometry, loaded);
        check(info.mesh_count == 1 && loaded.materials.size() == 1);
        check(loaded.instances.size() == 1 && loaded.instances[0].mesh_id == 0);
        check(loaded.instances[0].model[3].x == 5 && loaded.instances[0].model_inverse[3].x == -5);
        check(loaded.vertex_data == first.vertex_data && loaded.index_data == first.index_data);
        check(loaded.streams.bindings[0].stride == 16 && loaded.streams.attributes[0].offset == 4);
        check(loaded.images[0].uri == first.images[0].uri && loaded.materials[0].normal_texture.index == -1);
        std::vector<bounding_box> boxes;
        load_bounding_boxes(bounds, boxes);
        check(boxes[0].max == first.boxes[0].max);
        auto second = triangle(10);
        second.instances.emplace_back();
        second.instances[0].material_id = 0;
        data merged;
        merge_mesh_data(merged, {&first, &second});
        check(merged.meshes[1].index_offset == 3 && merged.index_data[3] == 4);
        check(merged.meshes[1].material_id == 1 && merged.materials[1].base_color_texture.index == 1);
        check(merged.materials[1].normal_texture.index == -1);
        check(merged.instances.size() == 2);
        check(merged.instances[1].mesh_id == 1 && merged.instances[1].material_id == 1);
        check(merged.instances[0].model[3].x == 5);
        check(merged.instances[0].material_id == inherit_material);
        check(merged.textures[1].image_id == 1 && merged.textures[1].sampler_id == 1);
        check(merged.images.size() == 2 && merged.samplers.size() == 2);
        recalculate_bounding_boxes(merged);
        check(merged.boxes[1].min == glm::vec3(10, 0, 0));
        check(merged.boxes[1].max == glm::vec3(12, 3, 0));
        merge_mesh_data(first, {&first, &second}); // destination may alias a source
        check(first.meshes.size() == 2);
        second.streams.bindings[0].stride = 32;
        rejects([&] {
            merge_mesh_data(merged, {&first, &second});
        });
        check(merged.meshes.size() == 2);
        std::filesystem::resize_file(geometry, std::filesystem::file_size(geometry) - 1);
        check(!is_mesh_data_valid(geometry));
        rejects([&] {
            load_mesh_data(geometry, loaded);
        });
        check(loaded.vertex_data.size() == 64);
        std::filesystem::resize_file(materials, std::filesystem::file_size(materials) - 1);
        check(!is_mesh_materials_valid(materials));
        save_mesh_data(geometry, data{});
        check(is_mesh_data_valid(geometry));
        {
            std::fstream file(geometry, std::ios::binary | std::ios::in | std::ios::out);
            file.put('\0');
        }
        check(!is_mesh_data_valid(geometry));
        check(!is_mesh_data_valid((dir / "missing")));

        // Core glTF defaults and all texture slots survive a material round-trip.
        material defaults;
        check(defaults.metallic_factor == 1 && defaults.roughness_factor == 1);
        check(defaults.alpha_mode == alpha_mode::opaque && defaults.alpha_cutoff == 0.5f);
        check(!defaults.double_sided && defaults.normal_scale == 1 && defaults.occlusion_strength == 1);
        check(defaults.emissive_factor == glm::vec3(0) && defaults.base_color_factor == glm::vec4(1));
        auto rich = triangle(0);
        rich.images.push_back({.mime_type = "image/png", .encoded_data = {137, 80, 78, 71}});
        rich.textures.push_back({.image_id = 1, .sampler_id = -1});
        auto &mat = rich.materials[0];
        mat.base_color_factor = {0.2f, 0.3f, 0.4f, 0.6f};
        mat.emissive_factor = {0.1f, 0.2f, 0.3f};
        mat.thickness_factor = 0.3f;
        mat.attenuation_color = {0.2f, 0.3f, 0.4f};
        mat.attenuation_distance = 1.2f;
        mat.ior = 1.7f;
        mat.transmission_factor = 0.6f;
        mat.metallic_factor = 0.25f;
        mat.roughness_factor = 0.75f;
        mat.normal_scale = 0.7f;
        mat.occlusion_strength = 0.4f;
        mat.alpha_mode = alpha_mode::mask;
        mat.alpha_cutoff = 0.3f;
        mat.double_sided = true;
        for (auto *slot : texture_slots(mat)) {
            slot->index = 1;
            slot->tex_coord = 1;
            slot->offset = {0.25f, 0.5f};
            slot->scale = {2, -1};
            slot->rotation = 0.9f;
        }
        rich.samplers[0].wrap_s = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        rich.samplers[0].wrap_t = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        rich.samplers[0].min_filter = VK_FILTER_NEAREST;
        rich.samplers[0].use_mipmaps = false;
        save_mesh_data(geometry, rich);
        load_mesh_data(geometry, loaded);
        save_mesh_data_materials(materials, rich);
        load_mesh_data_materials(materials, loaded);
        const auto &restored = loaded.materials[0];
        check(restored.base_color_factor == mat.base_color_factor && restored.emissive_factor == mat.emissive_factor);
        check(restored.thickness_factor == 0.3f && restored.attenuation_distance == 1.2f && restored.attenuation_color == glm::vec3(0.2f, 0.3f, 0.4f));
        check(restored.ior == 1.7f && restored.transmission_factor == 0.6f);
        check(restored.metallic_factor == 0.25f && restored.roughness_factor == 0.75f);
        check(restored.normal_scale == 0.7f && restored.occlusion_strength == 0.4f);
        check(restored.alpha_mode == alpha_mode::mask && restored.double_sided && restored.alpha_cutoff == 0.3f);
        for (const auto *slot : texture_slots(restored)) {
            check(slot->index == 1 && slot->tex_coord == 1 && slot->rotation == 0.9f);
            check(slot->offset == glm::vec2(0.25f, 0.5f) && slot->scale == glm::vec2(2, -1));
        }
        check(loaded.images[1].encoded_data == rich.images[1].encoded_data && loaded.images[1].mime_type == "image/png");
        check(loaded.textures[1].sampler_id == -1 && !loaded.samplers[0].use_mipmaps);
        check(loaded.samplers[0].wrap_s == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        check(loaded.samplers[0].wrap_t == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
        check(loaded.samplers[0].min_filter == VK_FILTER_NEAREST);
        merge_mesh_data(merged, {&rich, &rich});
        check(merged.textures[2].image_id == 2 && merged.textures[2].sampler_id == 1);
        check(merged.textures[3].image_id == 3 && merged.textures[3].sampler_id == -1);
        for (const auto *slot : texture_slots(merged.materials[1]))
            check(slot->index == 3);
        auto invalid = rich;
        invalid.textures[0].image_id = 99;
        rejects([&] {
            save_mesh_data_materials(materials, invalid);
        });
        invalid = rich;
        invalid.materials[0].normal_texture.index = -2;
        rejects([&] {
            save_mesh_data_materials(materials, invalid);
        });
        invalid = rich;
        invalid.textures[0].sampler_id = 99;
        rejects([&] {
            merge_mesh_data(merged, {&invalid});
        });
        check(merged.materials.size() == 2); // Invalid merge leaves destination untouched.
        invalid = rich;
        invalid.materials[0].roughness_factor = 2;
        rejects([&] {
            save_mesh_data_materials(materials, invalid);
        });
        // Explicit version checking and truncated payload detection preserve output.
        save_mesh_data_materials(materials, rich);
        std::filesystem::resize_file(materials, std::filesystem::file_size(materials) - 1);
        check(!is_mesh_materials_valid(materials));
        rejects([&] {
            load_mesh_data_materials(materials, loaded);
        });
        check(loaded.images.size() == 2);
        save_mesh_data_materials(materials, rich);
        {
            std::fstream file(materials, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(4);
            file.put('\x7f');
        }
        check(!is_mesh_materials_valid(materials));
        save_mesh_data_materials(materials, data{});
        load_mesh_data_materials(materials, loaded);
        check(loaded.materials.empty() && loaded.images.size() == 2 && loaded.textures.empty() && loaded.samplers.empty());
        // Punctual light properties and placements live beside mesh instances.
        auto lit = triangle(0);
        lit.instances.emplace_back();
        lit.lights = {{.type = light_type::point, .color = {0.2f, 0.4f, 0.8f}, .intensity = 12, .range = 8, .position = {1, 2, 3}},
                      {.type = light_type::directional, .intensity = 2, .direction = {1, 0, 0}},
                      {.type = light_type::spot, .intensity = 4, .position = {4, 5, 6}, .direction = {0, 1, 0}, .inner_cone_angle = 0.2f, .outer_cone_angle = 0.6f}};
        save_mesh_data(geometry, lit);
        load_mesh_data(geometry, loaded);
        check(loaded.instances.size() == 1 && loaded.lights.size() == 3);
        check(loaded.lights[0].type == light_type::point && loaded.lights[0].color == glm::vec3(0.2f, 0.4f, 0.8f));
        check(loaded.lights[0].intensity == 12 && loaded.lights[0].range == 8 && loaded.lights[0].position == glm::vec3(1, 2, 3));
        check(loaded.lights[1].type == light_type::directional && loaded.lights[1].direction == glm::vec3(1, 0, 0));
        check(loaded.lights[2].inner_cone_angle == 0.2f && loaded.lights[2].outer_cone_angle == 0.6f);
        check(loaded.lights[2].position == glm::vec3(4, 5, 6) && loaded.lights[2].direction == glm::vec3(0, 1, 0));
        load_mesh_data_materials(materials, loaded);
        check(loaded.lights.size() == 3); // Material loads preserve scene lighting.
        merge_mesh_data(lit, {&lit, &lit});
        check(lit.lights.size() == 6 && lit.instances.size() == 2);
        check(lit.lights[3].position == glm::vec3(1, 2, 3));
        data lights_only;
        lights_only.lights.push_back(light{});
        merge_mesh_data(merged, {&lights_only, &lit, &lights_only});
        check(merged.lights.size() == 8 && merged.meshes.size() == 2);
        merge_mesh_data(merged, {&lights_only, &lights_only});
        check(merged.lights.size() == 2 && merged.meshes.empty());
        save_mesh_data(geometry, lights_only);
        load_mesh_data(geometry, loaded);
        check(loaded.meshes.empty() && loaded.lights.size() == 1);
        auto bad_light = lights_only;
        bad_light.lights[0].intensity = -1;
        rejects([&] {
            save_mesh_data(geometry, bad_light);
        });
        bad_light = lights_only;
        bad_light.lights[0].range = -1;
        rejects([&] {
            save_mesh_data(geometry, bad_light);
        });
        bad_light = lights_only;
        bad_light.lights[0].color.x = 2;
        rejects([&] {
            save_mesh_data(geometry, bad_light);
        });
        bad_light = lights_only;
        bad_light.lights[0].position.x = std::numeric_limits<float>::quiet_NaN();
        rejects([&] {
            save_mesh_data(geometry, bad_light);
        });
        bad_light = lights_only;
        bad_light.lights[0].type = light_type::spot;
        bad_light.lights[0].inner_cone_angle = bad_light.lights[0].outer_cone_angle;
        rejects([&] {
            save_mesh_data(geometry, bad_light);
        });
        bad_light = lights_only;
        bad_light.lights[0].type = light_type::directional;
        bad_light.lights[0].direction = glm::vec3(0);
        rejects([&] {
            merge_mesh_data(merged, {&bad_light});
        });
        check(merged.lights.size() == 2);
        // Corrupt serialized type and truncated light payloads fail transactionally.
        {
            std::fstream file(geometry, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(std::filesystem::file_size(geometry) - 60);
            const uint32_t invalid_type = 99;
            file.write(reinterpret_cast<const char *>(&invalid_type), sizeof(invalid_type));
        }
        rejects([&] {
            load_mesh_data(geometry, loaded);
        });
        check(loaded.lights.size() == 1 && loaded.lights[0].type == light_type::point);
        save_mesh_data(geometry, lights_only);
        std::filesystem::resize_file(geometry, std::filesystem::file_size(geometry) - 1);
        check(!is_mesh_data_valid(geometry));
        // Versions 1/2 remain readable, clearing lights absent from the old file.
        auto legacy = triangle(0);
        legacy.instances.emplace_back();
        legacy.images.clear();
        save_mesh_data(geometry, legacy);
        std::filesystem::resize_file(geometry, std::filesystem::file_size(geometry) - 2 * sizeof(uint32_t));
        auto set_version = [&](uint32_t version) {
            std::fstream file(geometry, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(sizeof(header));
            file.write(reinterpret_cast<const char *>(&version), sizeof(version));
        };
        set_version(0x565A0002);
        load_mesh_data(geometry, loaded);
        check(loaded.lights.empty() && loaded.instances.size() == 1);
        std::filesystem::resize_file(geometry, std::filesystem::file_size(geometry) - sizeof(uint32_t) - sizeof(instance));
        set_version(0x565A0001);
        load_mesh_data(geometry, loaded);
        check(loaded.lights.empty() && loaded.instances.empty() && loaded.meshes.size() == 1);
        auto old_material = triangle(0);
        save_mesh_data_materials(materials, old_material);
        std::ifstream input_material(materials, std::ios::binary);
        std::vector<char> old_bytes((std::istreambuf_iterator<char>(input_material)), {});
        input_material.close();
        old_bytes.erase(old_bytes.begin() + 12 + 200, old_bytes.begin() + 12 + 644);
        const uint32_t old_version = 2;
        std::memcpy(old_bytes.data() + 4, &old_version, sizeof(old_version));
        {
            std::ofstream file(materials, std::ios::binary);
            file.write(old_bytes.data(), old_bytes.size());
        }
        load_mesh_data_materials(materials, loaded);
        check(loaded.materials[0].ior == 1.5f && loaded.materials[0].transmission_factor == 0 && loaded.materials[0].transmission_texture.index == -1);
        check(loaded.materials[0].thickness_factor == 0 && std::isinf(loaded.materials[0].attenuation_distance));
        save_mesh_data_materials(materials, old_material);
        {
            std::ifstream file(materials, std::ios::binary);
            old_bytes.assign(std::istreambuf_iterator<char>(file), {});
        }
        old_bytes.erase(old_bytes.begin() + 12 + 236, old_bytes.begin() + 12 + 644);
        const uint32_t version_three = 3;
        std::memcpy(old_bytes.data() + 4, &version_three, sizeof(version_three));
        {
            std::ofstream file(materials, std::ios::binary);
            file.write(old_bytes.data(), old_bytes.size());
        }
        load_mesh_data_materials(materials, loaded);
        check(loaded.materials[0].thickness_factor == 0 && loaded.materials[0].attenuation_color == glm::vec3(1));
        save_mesh_data_materials(materials, old_material);
        {
            std::ifstream file(materials, std::ios::binary);
            old_bytes.assign(std::istreambuf_iterator<char>(file), {});
        }
        old_bytes.erase(old_bytes.begin() + 12 + 284, old_bytes.begin() + 12 + 644);
        const uint32_t version_four = 4;
        std::memcpy(old_bytes.data() + 4, &version_four, sizeof(version_four));
        {
            std::ofstream file(materials, std::ios::binary);
            file.write(old_bytes.data(), old_bytes.size());
        }
        load_mesh_data_materials(materials, loaded);
        check(loaded.materials[0].specular_factor == 1 && loaded.materials[0].specular_color_factor == glm::vec3(1));
        check(loaded.materials[0].emissive_strength == 1 && !loaded.materials[0].unlit && loaded.materials[0].clearcoat_factor == 0 && loaded.materials[0].dispersion == 0);
        // Legacy material version 1 still owns its image payloads.
        std::vector<uint8_t> legacy_material;
        for (uint32_t word : {0x4D5A4B56u, 1u, 0u, 0u, 0u, 1u, 0u, 9u})
            put(legacy_material, word);
        const std::string mime = "image/png";
        legacy_material.insert(legacy_material.end(), mime.begin(), mime.end());
        put(legacy_material, uint32_t(4));
        legacy_material.insert(legacy_material.end(), {137, 80, 78, 71});
        {
            std::ofstream file(materials, std::ios::binary);
            file.write(reinterpret_cast<const char *>(legacy_material.data()), legacy_material.size());
        }
        load_mesh_data_materials(materials, loaded);
        check(loaded.images.size() == 1 && loaded.images[0].mime_type == "image/png" && loaded.images[0].encoded_data == std::vector<uint8_t>({137, 80, 78, 71}));
        std::cout << "Mesh/light I/O, validation, bounds and merge checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
