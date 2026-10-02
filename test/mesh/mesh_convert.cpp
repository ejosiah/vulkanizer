#include <vulkanizer/mesh.hpp>
#include <nlohmann/json.hpp>
#include <ktx.h>
#include <memory>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    using nlohmann::json;
    using namespace vkz::mesh;
    const std::vector<uint8_t> png_bytes{137, 80, 78, 71, 13,  10,  26, 10,  0,   0,   0,   13, 73, 72, 68, 82, 0,   0,   0,  2,   0,   0,   0,   2,  8,
                                         6,   0,  0,  0,  114, 182, 13, 36,  0,   0,   0,   17, 73, 68, 65, 84, 120, 156, 99, 104, 104, 248, 255, 31, 132,
                                         25,  96, 12, 0,  103, 210, 11, 249, 115, 144, 115, 69, 0,  0,  0,  0,  73,  69,  78, 68,  174, 66,  96,  130};

    void check(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }

    template <typename F> void rejects(F action) {
        try {
            action();
        } catch (const std::exception &) {
            return;
        }
        throw std::runtime_error("Expected conversion to reject invalid input");
    }

    void write(const vkz::path &file, const std::vector<uint8_t> &bytes) {
        std::ofstream stream(file, std::ios::binary);
        stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        check(bool(stream), "Fixture write failed");
    }

    void write(const vkz::path &file, const json &document) {
        std::ofstream stream(file);
        stream << document.dump();
        check(bool(stream), "JSON write failed");
    }

    data read(const conversion_files &files) {
        data result;
        load_mesh_data(files.mesh_file, result);
        load_mesh_data_materials(files.material_file, result);
        std::vector<bounding_box> boxes;
        load_bounding_boxes(files.bounds_file, boxes);
        check(boxes.size() == result.boxes.size() && (boxes.empty() || boxes[0].max == result.boxes[0].max), "Bounds sidecar differs");
        return result;
    }

    float attribute(const data &mesh, uint32_t vertex, uint32_t location, uint32_t component) {
        for (const auto &attribute : mesh.streams.attributes)
            if (attribute.location == location) {
                float result;
                std::memcpy(&result, mesh.vertex_data.data() + vertex * mesh.streams.bindings[0].stride + attribute.offset + component * sizeof(float), sizeof(float));
                return result;
            }
        throw std::runtime_error("Missing output attribute");
    }

    struct fixture {
        json document;
        std::vector<uint8_t> bytes;

        fixture() {
            // Position data is interleaved, with an accessor offset and one sparse override.
            for (const auto &point : {glm::vec3(0), glm::vec3(2, 0, 0), glm::vec3(0, 1, 0)}) {
                put(bytes, 999.0f);
                for (int c = 0; c < 3; ++c)
                    put(bytes, point[c]);
            }
            for (int i = 0; i < 3; ++i) {
                put(bytes, 0.0f);
                put(bytes, 0.0f);
                put(bytes, 1.0f);
            }
            for (const uint16_t value : {0, 0, 65535, 0, 0, 65535})
                put(bytes, value);
            for (const uint8_t value : {255, 128, 0, 0, 255, 0, 0, 0, 255})
                put(bytes, value);
            bytes.insert(bytes.end(), {0, 1, 2});    // unsigned-byte indices, offset 105
            bytes.insert(bytes.end(), {2, 0, 0, 0}); // sparse index 2, offset 108
            put(bytes, 0.0f);
            put(bytes, 3.0f);
            put(bytes, 0.0f); // sparse value, offset 112
            document = json::parse(R"({
                "asset":{"version":"2.0"},
                "buffers":[{"uri":"data.bin","byteLength":124}],
                "bufferViews":[
                    {"buffer":0,"byteOffset":0,"byteLength":48,"byteStride":16},
                    {"buffer":0,"byteOffset":48,"byteLength":36},
                    {"buffer":0,"byteOffset":84,"byteLength":12},
                    {"buffer":0,"byteOffset":96,"byteLength":9},
                    {"buffer":0,"byteOffset":105,"byteLength":3},
                    {"buffer":0,"byteOffset":108,"byteLength":1},
                    {"buffer":0,"byteOffset":112,"byteLength":12}
                ],
                "accessors":[
                    {"bufferView":0,"byteOffset":4,"componentType":5126,"type":"VEC3","count":3,"min":[0,0,0],"max":[2,3,0],
                     "sparse":{"count":1,"indices":{"bufferView":5,"componentType":5121},"values":{"bufferView":6}}},
                    {"bufferView":1,"componentType":5126,"type":"VEC3","count":3},
                    {"bufferView":2,"componentType":5123,"normalized":true,"type":"VEC2","count":3},
                    {"bufferView":3,"componentType":5121,"normalized":true,"type":"VEC3","count":3},
                    {"bufferView":4,"componentType":5121,"type":"SCALAR","count":3}
                ],
                "images":[{"uri":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEUlEQVR4nGNoaPj/H4QZYAwAZ9IL+XOQc0UAAAAASUVORK5CYII="}],
                "samplers":[{"magFilter":9728,"minFilter":9986,"wrapS":33071,"wrapT":33648}],
                "textures":[{"source":0,"sampler":0}],
                "materials":[{
                    "pbrMetallicRoughness":{"baseColorFactor":[0.5,0.6,0.7,0.8],"metallicFactor":0.2,"roughnessFactor":0.4,
                        "baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"offset":[0.2,0.3],"scale":[2,3],"rotation":0.5,"texCoord":1}}},
                        "metallicRoughnessTexture":{"index":0}},
                    "normalTexture":{"index":0,"scale":0.75},"occlusionTexture":{"index":0,"strength":0.25},
                    "emissiveTexture":{"index":0},"emissiveFactor":[0.1,0.2,0.3],"alphaMode":"MASK","alphaCutoff":0.3,"doubleSided":true
                }],
                "extensionsUsed":["KHR_texture_transform"],"extensionsRequired":["KHR_texture_transform"],
                "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TEXCOORD_1":2,"COLOR_0":3},"indices":4,"material":0}]}],
                "nodes":[{"translation":[10,0,0],"children":[1]}, {"mesh":0,"translation":[0,2,0]}, {"mesh":0,"scale":[-1,1,1]}],
                "scenes":[{"nodes":[0,2]}],"scene":0
            })");
            check(bytes.size() == 124, "Fixture offsets incorrect");
        }

        void save(const vkz::path &file) const {
            write(file.parent_path() / "data.bin", bytes);
            write(file, document);
        }

        void glb(const vkz::path &file) const {
            auto doc = document;
            doc["buffers"][0].erase("uri");
            auto text = doc.dump();
            while (text.size() % 4)
                text += ' ';
            auto bin = bytes;
            while (bin.size() % 4)
                bin.push_back(0);
            std::vector<uint8_t> result;
            put(result, uint32_t(0x46546c67));
            put(result, uint32_t(2));
            put(result, uint32_t(28 + text.size() + bin.size()));
            put(result, uint32_t(text.size()));
            put(result, uint32_t(0x4e4f534a));
            result.insert(result.end(), text.begin(), text.end());
            put(result, uint32_t(bin.size()));
            put(result, uint32_t(0x004e4942));
            result.insert(result.end(), bin.begin(), bin.end());
            write(file, result);
        }
    };
}

int main(int argc, char **argv) {
    const auto dir = std::filesystem::temp_directory_path() / vkz::path(u8"vkz-convert-\u03bb") / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(dir);

    struct cleanup {
        vkz::path directory;

        ~cleanup() {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    } guard{dir};

    try {
        // Optional real-asset test: source [output_base]. An explicit output
        // base keeps the converted resources; otherwise the test cleans them up.
        if (argc > 1) {
            const auto output_base = argc > 2 ? std::filesystem::u8path(argv[2]) : dir / "asset";
            const auto files = convert(std::filesystem::u8path(argv[1]), output_base);
            const auto imported = read(files);
            std::cout << imported.meshes.size() << " meshes, " << imported.instances.size() << " instances, " << imported.lights.size() << " lights\n";
            if (argc > 2) std::cout << files.mesh_file << '\n' << files.material_file << '\n' << files.bounds_file << '\n';
            return 0;
        }
        fixture source;
        const auto input = dir / u8"scene-\u03bb.GLTF";
        source.save(input);
        const auto files = convert(input, dir / "converted" / "scene");
        check(files.mesh_file.extension() == ".vkmesh" && files.material_file.extension() == ".vkmat" && files.bounds_file.extension() == ".vkbounds", "Output extensions");
        const auto mesh = read(files);
        check(mesh.meshes.size() == 1 && mesh.instances.size() == 2, "Shared geometry was duplicated");
        check(mesh.instances[0].mesh_id == mesh.instances[1].mesh_id, "Instances do not share a mesh");
        check(mesh.instances[0].model[3] == glm::vec4(10, 2, 0, 1) && mesh.instances[0].model_inverse[3] == glm::vec4(-10, -2, 0, 1), "World transform/inverse");
        check(mesh.instances[1].model[0].x == -1, "Negative scale lost");
        check(mesh.boxes[0].max == glm::vec3(2, 3, 0), "Strided/sparse POSITION");
        check(attribute(mesh, 1, 4, 0) == 1 && attribute(mesh, 2, 5, 1) == 1, "Normalized UVs");
        check(std::abs(attribute(mesh, 0, 3, 1) - 128.0f / 255) < 0.00001f && attribute(mesh, 0, 3, 3) == 1, "Normalized RGB color");
        const auto &mat = mesh.materials[0];
        check(mat.alpha_mode == alpha_mode::mask && mat.double_sided && mat.base_color_texture.tex_coord == 1, "Material/UV override");
        check(mat.base_color_texture.offset == glm::vec2(0.2f, 0.3f) && mat.base_color_texture.scale == glm::vec2(2, 3) && mat.base_color_texture.rotation == 0.5f, "Texture transform");
        check(mat.normal_scale == 0.75f && mat.occlusion_strength == 0.25f && mat.metallic_factor == 0.2f, "PBR factors");
        check(mesh.samplers[0].min_filter == VK_FILTER_NEAREST && mesh.samplers[0].mipmap_mode == VK_SAMPLER_MIPMAP_MODE_LINEAR && mesh.samplers[0].wrap_s == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
              "Sampler mapping");
        {
            auto extended = source;
            auto &extensions = extended.document["materials"][0]["extensions"];
            extensions["KHR_materials_specular"] = {{"specularFactor", 0.4}, {"specularColorFactor", {1.5, 0.5, 0.25}}, {"specularTexture", {{"index", 0}}}, {"specularColorTexture", {{"index", 0}}}};
            extensions["KHR_materials_emissive_strength"] = {{"emissiveStrength", 3.0}};
            extensions["KHR_materials_clearcoat"] = {{"clearcoatFactor", 0.7},
                                                     {"clearcoatRoughnessFactor", 0.2},
                                                     {"clearcoatTexture", {{"index", 0}}},
                                                     {"clearcoatRoughnessTexture", {{"index", 0}}},
                                                     {"clearcoatNormalTexture", {{"index", 0}, {"scale", 0.6}}}};
            extensions["KHR_materials_sheen"] = {
                {"sheenColorFactor", {0.1, 0.2, 0.3}}, {"sheenRoughnessFactor", 0.8}, {"sheenColorTexture", {{"index", 0}}}, {"sheenRoughnessTexture", {{"index", 0}}}};
            extensions["KHR_materials_anisotropy"] = {{"anisotropyStrength", 0.9}, {"anisotropyRotation", 0.5}, {"anisotropyTexture", {{"index", 0}}}};
            extensions["KHR_materials_iridescence"] = {{"iridescenceFactor", 0.8},
                                                       {"iridescenceIor", 1.4},
                                                       {"iridescenceThicknessMinimum", 90.0},
                                                       {"iridescenceThicknessMaximum", 450.0},
                                                       {"iridescenceTexture", {{"index", 0}}},
                                                       {"iridescenceThicknessTexture", {{"index", 0}}}};
            extensions["KHR_materials_dispersion"] = {{"dispersion", 0.5}};
            for (auto it = extensions.begin(); it != extensions.end(); ++it)
                extended.document["extensionsRequired"].push_back(it.key());
            extended.save(input);
            const auto converted = read(convert(input, dir / "extended"));
            const auto &value = converted.materials[0];
            check(value.specular_factor == 0.4f && value.specular_color_factor.x == 1.5f && value.emissive_strength == 3.0f, "Specular and emission factors");
            check(value.clearcoat_factor == 0.7f && value.clearcoat_roughness_factor == 0.2f && value.clearcoat_normal_scale == 0.6f, "Clearcoat factors");
            check(value.sheen_color_factor == glm::vec3(0.1f, 0.2f, 0.3f) && value.sheen_roughness_factor == 0.8f, "Sheen factors");
            check(value.anisotropy_strength == 0.9f && value.anisotropy_rotation == 0.5f, "Anisotropy factors");
            check(value.iridescence_factor == 0.8f && value.iridescence_ior == 1.4f && value.iridescence_thickness_minimum == 90 && value.iridescence_thickness_maximum == 450 &&
                      value.dispersion == 0.5f,
                  "Iridescence and dispersion factors");
            const auto slots = texture_slots(value);
            for (size_t i = 7; i < slots.size(); ++i)
                check(slots[i]->index == (i == 8 || i == 12 ? value.base_color_texture.index : value.normal_texture.index), "Extension texture transfer function");
            auto unlit = source;
            unlit.document["materials"][0]["extensions"]["KHR_materials_unlit"] = nlohmann::json::object();
            unlit.document["extensionsRequired"].push_back("KHR_materials_unlit");
            unlit.save(input);
            check(read(convert(input, dir / "unlit")).materials[0].unlit, "Unlit import");
            source.save(input);
        }
        check(mesh.images.size() == 2 && mesh.images[0].uri.empty() && mesh.images[0].mime_type == "image/ktx2", "KTX color/data variants missing");
        for (size_t i = 0; i < mesh.images.size(); ++i) {
            const auto &bytes = mesh.images[i].encoded_data;
            ktxTexture *raw{};
            check(ktxTexture_CreateFromMemory(bytes.data(), bytes.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &raw) == KTX_SUCCESS, "Invalid embedded KTX");
            auto destroy = [](ktxTexture *value) {
                ktxTexture_Destroy(value);
            };
            std::unique_ptr<ktxTexture, decltype(destroy)> texture(raw, destroy);
            check(raw->classId == ktxTexture2_c && raw->baseWidth == 2 && raw->baseHeight == 2 && raw->numLevels == 2, "KTX mip chain");
            check(reinterpret_cast<ktxTexture2 *>(raw)->vkFormat == (i == 0 ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM), "KTX transfer function");
        }
        check(mesh.materials[0].base_color_texture.index == mesh.materials[0].emissive_texture.index && mesh.materials[0].normal_texture.index == mesh.materials[0].metallic_roughness_texture.index,
              "KTX variant sharing");
        data geometry_only;
        load_mesh_data(files.mesh_file, geometry_only);
        check(geometry_only.images[0].encoded_data == mesh.images[0].encoded_data, "KTX payload not stored in vkmesh");
        data materials_only;
        load_mesh_data_materials(files.material_file, materials_only);
        check(materials_only.images.empty() && !materials_only.textures.empty(), "vkmat duplicates image payloads");

        auto optical = source;
        optical.document["extensionsRequired"] = {"KHR_materials_ior", "KHR_materials_transmission"};
        optical.document["materials"][0]["extensions"]["KHR_materials_ior"] = {{"ior", 1.7}};
        optical.document["materials"][0]["extensions"]["KHR_materials_transmission"] = {
            {"transmissionFactor", 0.8}, {"transmissionTexture", {{"index", 0}, {"texCoord", 1}, {"extensions", {{"KHR_texture_transform", {{"offset", {0.1, 0.2}}}}}}}}};
        optical.save(input);
        const auto transmission = read(convert(input));
        const auto &optical_material = transmission.materials[0];
        check(optical_material.ior == 1.7f && optical_material.transmission_factor == 0.8f, "Optical factors round trip");
        check(optical_material.transmission_texture.tex_coord == 1 && optical_material.transmission_texture.offset == glm::vec2(0.1f, 0.2f), "Transmission UV mapping");
        check(optical_material.transmission_texture.index == optical_material.normal_texture.index, "Transmission texture uses linear KTX variant");
        check(mesh.materials[0].ior == 1.5f && mesh.materials[0].transmission_factor == 0, "Optical defaults");
        for (const auto &invalid : {json(0.5), json(-1), json("glass")}) {
            auto bad = optical;
            bad.document["materials"][0]["extensions"]["KHR_materials_ior"]["ior"] = invalid;
            bad.save(input);
            rejects([&] {
                convert(input);
            });
        }
        for (const auto &invalid : {json(-0.1), json(1.1), json("transparent")}) {
            auto bad = optical;
            bad.document["materials"][0]["extensions"]["KHR_materials_transmission"]["transmissionFactor"] = invalid;
            bad.save(input);
            rejects([&] {
                convert(input);
            });
        }
        auto volumetric = optical;
        volumetric.document["extensionsRequired"].push_back("KHR_materials_volume");
        volumetric.document["materials"][0]["extensions"]["KHR_materials_volume"] = {
            {"thicknessFactor", 0.2}, {"attenuationDistance", 2.0}, {"attenuationColor", {0.2, 0.5, 1.0}}, {"thicknessTexture", {{"index", 0}, {"texCoord", 1}}}};
        volumetric.save(input);
        const auto volume_mesh = read(convert(input));
        const auto &volume_material = volume_mesh.materials[0];
        check(volume_material.thickness_factor == 0.2f && volume_material.attenuation_distance == 2.0f && volume_material.attenuation_color == glm::vec3(0.2f, 0.5f, 1), "Volume factor round trip");
        check(volume_material.thickness_texture.index == volume_material.normal_texture.index && volume_material.thickness_texture.tex_coord == 1, "Thickness texture linear KTX and UV set");
        check(mesh.materials[0].thickness_factor == 0 && std::isinf(mesh.materials[0].attenuation_distance) && mesh.materials[0].attenuation_color == glm::vec3(1), "Volume defaults");
        for (const auto &name : {"thicknessFactor", "attenuationDistance", "attenuationColor"}) {
            auto bad = volumetric;
            bad.document["materials"][0]["extensions"]["KHR_materials_volume"][name] = -1;
            bad.save(input);
            rejects([&] {
                convert(input);
            });
        }
        auto zero_distance = volumetric;
        zero_distance.document["materials"][0]["extensions"]["KHR_materials_volume"]["attenuationDistance"] = 0;
        zero_distance.save(input);
        rejects([&] {
            convert(input);
        });
        auto bad_color = volumetric;
        bad_color.document["materials"][0]["extensions"]["KHR_materials_volume"]["attenuationColor"] = {1, 2, 1};
        bad_color.save(input);
        rejects([&] {
            convert(input);
        });
        source.save(input);
        source.glb(dir / "binary.GLB");
        const auto binary = read(convert(dir / "binary.GLB"));
        check(binary.vertex_data == mesh.vertex_data && binary.index_data == mesh.index_data, "GLB differs from glTF");
        auto embedded = source;
        const auto image_offset = embedded.bytes.size();
        embedded.bytes.insert(embedded.bytes.end(), png_bytes.begin(), png_bytes.end());
        embedded.document["buffers"][0]["byteLength"] = embedded.bytes.size();
        embedded.document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", image_offset}, {"byteLength", png_bytes.size()}});
        embedded.document["images"][0] = {{"bufferView", 7}, {"mimeType", "image/png"}};
        embedded.glb(dir / "embedded.glb");
        check(read(convert(dir / "embedded.glb")).images[0].encoded_data == mesh.images[0].encoded_data, "GLB image buffer view");
        for (const int type : {5123, 5125}) {
            auto wide = source;
            const auto offset = wide.bytes.size();
            for (uint32_t index = 0; index < 3; ++index) {
                if (type == 5123)
                    put(wide.bytes, uint16_t(index));
                else
                    put(wide.bytes, index);
            }
            wide.document["bufferViews"][4]["byteOffset"] = offset;
            wide.document["bufferViews"][4]["byteLength"] = wide.bytes.size() - offset;
            wide.document["buffers"][0]["byteLength"] = wide.bytes.size();
            wide.document["accessors"][4]["componentType"] = type;
            wide.save(input);
            check(read(convert(input)).index_data == mesh.index_data, "16/32-bit indices");
        }
        // Sparse accessors may have no base buffer view (all other values are zero).
        auto sparse = source;
        sparse.document["accessors"][0].erase("bufferView");
        sparse.document["accessors"][0].erase("byteOffset");
        sparse.save(input);
        check(read(convert(input)).boxes[0].max == glm::vec3(0, 3, 0), "Implicit zero sparse base");
        auto transformed = source;
        transformed.document["nodes"][0].erase("translation");
        transformed.document["nodes"][0]["matrix"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 10, 0, 0, 1};
        transformed.document["nodes"][1]["rotation"] = {0, 0, 1, 0};
        transformed.save(input);
        const auto rotated = read(convert(input));
        check(rotated.instances[0].model[0].x == -1 && rotated.instances[0].model[3].x == 10, "Matrix/quaternion composition");
        // External images remain usable after moving the output away from the input.
        write(dir / "albedo image.png", png_bytes);
        source.document["images"][0]["uri"] = "albedo%20image.png";
        source.save(input);
        check(read(convert(input)).images[0].encoded_data == mesh.images[0].encoded_data, "External image bytes differ");

        // Non-indexed primitives, missing normals, default material and multiple primitives.
        auto &primitive = source.document["meshes"][0]["primitives"][0];
        primitive.erase("indices");
        primitive.erase("material");
        primitive["attributes"].erase("NORMAL");
        primitive["mode"] = 5;
        source.document["meshes"][0]["primitives"].push_back(primitive);
        source.document["meshes"][0]["primitives"][1]["mode"] = 6;
        source.save(input);
        const auto flat = read(convert(input));
        check(flat.meshes.size() == 2 && flat.instances.size() == 4, "Primitive-to-instance mapping");
        check(flat.meshes[1].vertex_offset == 3 && flat.meshes[1].index_offset == 3, "Mesh offsets");
        check(attribute(flat, 0, 1, 2) == 1 && attribute(flat, 0, 2, 3) == 0, "Flat normal/absent tangent");
        check(flat.meshes[0].material_id == 1 && flat.materials[1].base_color_factor == glm::vec4(1), "Default material");

        const auto rejected_output = dir / "rejected" / "asset";
        auto reject_document = [&](json invalid) {
            write(input, invalid);
            rejects([&] {
                convert(input, rejected_output);
            });
            check(!std::filesystem::exists(rejected_output.parent_path()), "Invalid import wrote outputs");
        };
        auto invalid = source.document;
        invalid["extensionsRequired"].push_back("KHR_draco_mesh_compression");
        reject_document(invalid);
        invalid = source.document;
        invalid["nodes"][1]["children"] = {0};
        reject_document(invalid);
        invalid = source.document;
        invalid["nodes"][1]["skin"] = 0;
        reject_document(invalid);
        invalid = source.document;
        invalid["nodes"][1]["scale"] = {0, 1, 1};
        reject_document(invalid);
        invalid = source.document;
        invalid["accessors"][0]["count"] = 99;
        reject_document(invalid);
        invalid = source.document;
        invalid["meshes"][0]["primitives"][0]["mode"] = 1;
        reject_document(invalid);
        invalid = source.document;
        invalid["images"][0]["uri"] = "missing.png";
        reject_document(invalid);
        invalid = source.document;
        invalid["meshes"][0]["primitives"][0]["material"] = 0;
        invalid["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_1");
        reject_document(invalid);
        rejects([&] {
            convert(dir / "unsupported.obj");
        });
        rejects([&] {
            convert(dir / "missing.gltf");
        });
        fixture lit;
        lit.document["extensionsUsed"].push_back("KHR_lights_punctual");
        lit.document["extensionsRequired"].push_back("KHR_lights_punctual");
        lit.document["extensions"]["KHR_lights_punctual"]["lights"] = json::parse(R"([
            {"type":"point","color":[0.2,0.5,0.8],"intensity":15,"range":9},
            {"type":"directional"},
            {"type":"spot","intensity":3,"spot":{"innerConeAngle":0.2,"outerConeAngle":0.6}},
            {"type":"spot","spot":{}}
        ])");
        // A node may carry both a mesh and a light. Parent scale changes position,
        // but never light intensity/range or spot cone shape.
        lit.document["nodes"][0]["scale"] = {2, 3, 4};
        lit.document["nodes"][1]["extensions"]["KHR_lights_punctual"]["light"] = 0;
        lit.document["nodes"][2]["extensions"]["KHR_lights_punctual"]["light"] = 0;
        lit.document["nodes"].push_back(json::parse(R"({"translation":[100,200,300],"rotation":[0,1,0,0],"scale":[2,3,4],"extensions":{"KHR_lights_punctual":{"light":1}}})"));
        lit.document["nodes"].push_back(json::parse(R"({"translation":[1,0,0],"scale":[2,3,4],"extensions":{"KHR_lights_punctual":{"light":2}}})"));
        lit.document["nodes"].push_back(json::parse(R"({"extensions":{"KHR_lights_punctual":{"light":3}}})"));
        lit.document["nodes"][0]["children"].push_back(4);
        lit.document["scenes"][0]["nodes"].push_back(3);
        lit.document["scenes"][0]["nodes"].push_back(5);
        lit.save(input);
        const auto lighting = read(convert(input));
        check(lighting.instances.size() == 2 && lighting.lights.size() == 5, "Mesh and light placements");
        check(lighting.lights[0].position == glm::vec3(10, 6, 0) && lighting.lights[2].position == glm::vec3(0), "Point world positions");
        check(lighting.lights[0].intensity == 15 && lighting.lights[0].range == 9 && lighting.lights[0].color == glm::vec3(0.2f, 0.5f, 0.8f), "Unscaled light properties");
        check(lighting.lights[1].type == light_type::spot && lighting.lights[1].position == glm::vec3(12, 0, 0) && lighting.lights[1].direction == glm::vec3(0, 0, -1) &&
                  lighting.lights[1].outer_cone_angle == 0.6f,
              "Spot transform/cone");
        check(lighting.lights[3].type == light_type::directional && lighting.lights[3].direction == glm::vec3(0, 0, 1) && lighting.lights[3].color == glm::vec3(1) &&
                  lighting.lights[3].intensity == 1 && lighting.lights[3].range == 0,
              "Directional transform/defaults");
        check(lighting.lights[4].inner_cone_angle == 0 && lighting.lights[4].outer_cone_angle == std::numbers::pi_v<float> / 4, "Spot angle defaults");
        lit.glb(dir / "lights.glb");
        check(read(convert(dir / "lights.glb")).lights.size() == 5, "GLB light serialization");

        auto only_lights = lit.document;
        only_lights.erase("meshes");
        only_lights.erase("materials");
        only_lights.erase("textures");
        only_lights.erase("images");
        only_lights.erase("accessors");
        only_lights.erase("bufferViews");
        only_lights.erase("buffers");
        only_lights["nodes"][1].erase("mesh");
        only_lights["nodes"][2].erase("mesh");
        write(input, only_lights);
        const auto light_scene = read(convert(input));
        check(light_scene.meshes.empty() && light_scene.instances.empty() && light_scene.lights.size() == 5, "Light-only scene");
        auto reject_light = [&](const char *key, const json &value) {
            auto invalid_light = lit.document;
            invalid_light["extensions"]["KHR_lights_punctual"]["lights"][0][key] = value;
            reject_document(invalid_light);
        };
        reject_light("type", "area");
        reject_light("range", 0);
        reject_light("range", -1);
        reject_light("intensity", -1);
        reject_light("intensity", "bright");
        reject_light("color", {1, 2, 1});
        invalid = lit.document;
        invalid["nodes"][1]["extensions"]["KHR_lights_punctual"]["light"] = 99;
        reject_document(invalid);
        invalid = lit.document;
        invalid["nodes"][3]["scale"] = {1, 1, 0};
        reject_document(invalid);
        invalid = lit.document;
        invalid["extensions"]["KHR_lights_punctual"]["lights"][2]["spot"]["innerConeAngle"] = 0.6;
        reject_document(invalid);
        invalid = lit.document;
        invalid["extensions"]["KHR_lights_punctual"]["lights"][2]["spot"]["outerConeAngle"] = 2;
        reject_document(invalid);
        std::cout << "Mesh and punctual light conversion checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
