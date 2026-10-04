#include "vulkanizer/mesh.hpp"
#include "mesh_light.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

// Adapted from Packt's 3D Graphics Rendering Cookbook, Second Edition (2024).
// MIT License
//
// Copyright (c) 2024 Packt
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
namespace vkz::mesh {
    namespace {
        // Follows header; distinguishes our vector-based layout from lvk::VertexInput.
        constexpr uint32 stream_version = 0x565A0005;

        void require(bool condition, const char *message) {
            if (!condition) throw std::runtime_error(message);
        }

        uint32 size32(size_t value) {
            require(value <= std::numeric_limits<uint32>::max(), "Mesh data exceeds 32-bit format limits");
            return static_cast<uint32>(value);
        }

        struct reader {
            std::ifstream file;
            uint64 remaining{};

            explicit reader(const vkz::path &path) : file(path, std::ios::binary | std::ios::ate) {
                require(file.is_open(), "Cannot open mesh data file");
                const auto end = file.tellg();
                require(end >= 0, "Cannot determine mesh file size");
                remaining = static_cast<uint64>(end);
                file.seekg(0);
            }

            void read(void *destination, size_t bytes) {
                require(bytes <= remaining, "Truncated mesh data file");
                if (bytes) file.read(static_cast<char *>(destination), bytes);
                require(bool(file), "Failed to read mesh data");
                remaining -= bytes;
            }

            template <typename T> T value() {
                static_assert(std::is_trivially_copyable_v<T>);
                T result{};
                read(&result, sizeof(T));
                return result;
            }

            template <typename T> std::vector<T> array(uint64 count) {
                static_assert(std::is_trivially_copyable_v<T>);
                require(count <= remaining / sizeof(T), "Invalid mesh array size");
                std::vector<T> result(static_cast<size_t>(count));
                read(result.data(), result.size() * sizeof(T));
                return result;
            }
        };

        struct writer {
            std::ofstream file;

            explicit writer(const vkz::path &path) : file(path, std::ios::binary) {
                require(file.is_open(), "Cannot open mesh data file for writing");
            }

            void write(const void *source, size_t bytes) {
                if (bytes) file.write(static_cast<const char *>(source), bytes);
                require(bool(file), "Failed to write mesh data");
            }

            template <typename T> void value(const T &item) {
                static_assert(std::is_trivially_copyable_v<T>);
                write(&item, sizeof(T));
            }

            template <typename T> void array(const std::vector<T> &items) {
                static_assert(std::is_trivially_copyable_v<T>);
                write(items.data(), items.size() * sizeof(T));
            }

            void finish() {
                file.close();
                require(bool(file), "Failed to finish writing mesh data");
            }
        };

        uint32 vertex_stride(const data &meshes) {
            require(meshes.streams.bindings.size() == 1, "Mesh processing requires one interleaved vertex binding");
            const auto &binding = meshes.streams.bindings.front();
            require(binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX && binding.stride > 0, "Invalid mesh vertex binding");
            require(meshes.vertex_data.size() % binding.stride == 0, "Vertex data is not stride aligned");
            return binding.stride;
        }

        bool same_layout(const vertex_input &a, const vertex_input &b) {
            return a.bindings.size() == b.bindings.size() && a.attributes.size() == b.attributes.size() &&
                   std::equal(a.bindings.begin(), a.bindings.end(), b.bindings.begin(),
                              [](const auto &x, const auto &y) {
                                  return x.binding == y.binding && x.stride == y.stride && x.inputRate == y.inputRate;
                              }) &&
                   std::equal(a.attributes.begin(), a.attributes.end(), b.attributes.begin(), [](const auto &x, const auto &y) {
                       return x.location == y.location && x.binding == y.binding && x.format == y.format && x.offset == y.offset;
                   });
        }

        constexpr uint32 materialMagic = 0x4D5A4B56; // VKZM
        constexpr uint32 materialVersion = 5;

        template <typename T>
            requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
        void field(reader &in, T &value) {
            value = in.value<T>();
        }

        template <typename T>
            requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
        void field(writer &out, T &value) {
            out.value(value);
        }

        void field(reader &in, bool &value) {
            const auto raw = in.value<uint32>();
            require(raw <= 1, "Invalid boolean in material file");
            value = raw != 0;
        }

        void field(writer &out, bool &value) {
            out.value(uint32(value));
        }

        template <typename Archive, glm::length_t N, typename T, glm::qualifier Q> void field(Archive &archive, glm::vec<N, T, Q> &value) {
            for (glm::length_t i = 0; i < N; ++i)
                field(archive, value[i]);
        }

        template <typename Archive> void fields(Archive &archive, texture_info &slot) {
            field(archive, slot.index);
            field(archive, slot.tex_coord);
            field(archive, slot.offset);
            field(archive, slot.scale);
            field(archive, slot.rotation);
        }

        template <typename Archive> void fields(Archive &archive, material &mat, uint32 version = materialVersion) {
            field(archive, mat.base_color_factor);
            field(archive, mat.emissive_factor);
            field(archive, mat.metallic_factor);
            field(archive, mat.roughness_factor);
            field(archive, mat.normal_scale);
            field(archive, mat.occlusion_strength);
            field(archive, mat.alpha_mode);
            field(archive, mat.alpha_cutoff);
            field(archive, mat.double_sided);
            fields(archive, mat.base_color_texture);
            fields(archive, mat.metallic_roughness_texture);
            fields(archive, mat.normal_texture);
            fields(archive, mat.occlusion_texture);
            fields(archive, mat.emissive_texture);
            field(archive, mat.flags);
            if (version >= 3) {
                field(archive, mat.ior);
                field(archive, mat.transmission_factor);
                fields(archive, mat.transmission_texture);
            }
            if (version >= 4) {
                field(archive, mat.thickness_factor);
                field(archive, mat.attenuation_distance);
                field(archive, mat.attenuation_color);
                fields(archive, mat.thickness_texture);
            }
            if (version >= 5) {
                field(archive, mat.specular_factor);
                field(archive, mat.specular_color_factor);
                field(archive, mat.emissive_strength);
                field(archive, mat.unlit);
                field(archive, mat.clearcoat_factor);
                field(archive, mat.clearcoat_roughness_factor);
                field(archive, mat.clearcoat_normal_scale);
                field(archive, mat.sheen_color_factor);
                field(archive, mat.sheen_roughness_factor);
                field(archive, mat.anisotropy_strength);
                field(archive, mat.anisotropy_rotation);
                field(archive, mat.iridescence_factor);
                field(archive, mat.iridescence_ior);
                field(archive, mat.iridescence_thickness_minimum);
                field(archive, mat.iridescence_thickness_maximum);
                field(archive, mat.dispersion);
                fields(archive, mat.specular_texture);
                fields(archive, mat.specular_color_texture);
                fields(archive, mat.clearcoat_texture);
                fields(archive, mat.clearcoat_roughness_texture);
                fields(archive, mat.clearcoat_normal_texture);
                fields(archive, mat.sheen_color_texture);
                fields(archive, mat.sheen_roughness_texture);
                fields(archive, mat.anisotropy_texture);
                fields(archive, mat.iridescence_texture);
                fields(archive, mat.iridescence_thickness_texture);
            }
        }

        template <typename Archive> void fields(Archive &archive, sampler &value) {
            field(archive, value.mag_filter);
            field(archive, value.min_filter);
            field(archive, value.mipmap_mode);
            field(archive, value.wrap_s);
            field(archive, value.wrap_t);
            field(archive, value.use_mipmaps);
        }

        template <typename Archive> void fields(Archive &archive, texture &value) {
            field(archive, value.image_id);
            field(archive, value.sampler_id);
        }

        template <typename Archive> void fields(Archive &archive, light &value, uint32 version = stream_version) {
            field(archive, value.type);
            field(archive, value.color);
            field(archive, value.intensity);
            field(archive, value.range);
            field(archive, value.position);
            field(archive, value.direction);
            field(archive, value.inner_cone_angle);
            field(archive, value.outer_cone_angle);
            if (version >= 0x565A0005) {
                field(archive, value.mesh_id);
                for (int column = 0; column < 4; ++column) field(archive, value.model[column]);
            }
        }

        std::string readString(reader &in) {
            auto bytes = in.array<char>(in.value<uint32>());
            return {bytes.begin(), bytes.end()};
        }

        void writeString(writer &out, const std::string &text) {
            out.value(size32(text.size()));
            out.write(text.data(), text.size());
        }

        // Bound counts by the on-disk minimum size before allocating containers.
        template <typename T> std::vector<T> readRecords(reader &in, uint32 recordSize) {
            const auto count = in.value<uint32>();
            require(count <= in.remaining / recordSize, "Invalid resource record count");
            std::vector<T> result(count);
            for (auto &value : result)
                fields(in, value);
            return result;
        }

        template <typename T> void writeRecords(writer &out, const std::vector<T> &values) {
            out.value(size32(values.size()));
            for (auto value : values)
                fields(out, value);
        }

        void validate_images(const data &meshes) {
            for (const auto &img : meshes.images) {
                require(img.uri.empty() != img.encoded_data.empty(), "Image needs exactly one URI or encoded payload");
                require(img.encoded_data.empty() || !img.mime_type.empty(), "Embedded image needs a MIME type");
                size32(img.uri.size());
                size32(img.mime_type.size());
                size32(img.encoded_data.size());
            }
        }

        void validateMaterials(const data &meshes, uint64 image_count = std::numeric_limits<uint64>::max()) {
            auto finite = [](float value) {
                return std::isfinite(value);
            };
            auto unit = [&](float value) {
                return finite(value) && value >= 0 && value <= 1;
            };
            if (image_count == std::numeric_limits<uint64>::max()) {
                validate_images(meshes);
                image_count = meshes.images.size();
            }
            for (const auto &tex : meshes.textures) {
                require(tex.image_id < image_count, "Texture image ID is out of range");
                require(tex.sampler_id == -1 || (tex.sampler_id >= 0 && size_t(tex.sampler_id) < meshes.samplers.size()), "Texture sampler ID is out of range");
            }
            for (const auto &sample : meshes.samplers) {
                require((sample.mag_filter == VK_FILTER_NEAREST || sample.mag_filter == VK_FILTER_LINEAR) && (sample.min_filter == VK_FILTER_NEAREST || sample.min_filter == VK_FILTER_LINEAR),
                        "Invalid texture filter");
                require(sample.mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || sample.mipmap_mode == VK_SAMPLER_MIPMAP_MODE_LINEAR, "Invalid mipmap mode");
                auto validWrap = [](auto wrap) {
                    return wrap == VK_SAMPLER_ADDRESS_MODE_REPEAT || wrap == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT || wrap == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                };
                require(validWrap(sample.wrap_s) && validWrap(sample.wrap_t), "Invalid texture wrap mode");
            }
            for (const auto &mat : meshes.materials) {
                for (int i = 0; i < 4; ++i)
                    require(unit(mat.base_color_factor[i]), "Invalid base color factor");
                for (int i = 0; i < 3; ++i)
                    require(unit(mat.emissive_factor[i]), "Invalid core emissive factor");
                require(unit(mat.metallic_factor) && unit(mat.roughness_factor) && unit(mat.occlusion_strength), "Invalid PBR factor");
                require(finite(mat.ior) && (mat.ior == 0 || mat.ior >= 1) && unit(mat.transmission_factor), "Invalid IOR/transmission factor");
                require(finite(mat.thickness_factor) && mat.thickness_factor >= 0, "Invalid volume thickness");
                require(mat.attenuation_distance > 0, "Invalid attenuation distance");
                for (int i = 0; i < 3; ++i)
                    require(unit(mat.attenuation_color[i]), "Invalid attenuation color");
                require(unit(mat.specular_factor) && unit(mat.clearcoat_factor) && unit(mat.clearcoat_roughness_factor) && unit(mat.sheen_roughness_factor) && unit(mat.anisotropy_strength) &&
                            unit(mat.iridescence_factor),
                        "Invalid extension factor");
                require(finite(mat.emissive_strength) && mat.emissive_strength >= 0 && finite(mat.dispersion) && mat.dispersion >= 0, "Invalid emission or dispersion");
                require(finite(mat.clearcoat_normal_scale) && finite(mat.anisotropy_rotation), "Invalid extension scale or rotation");
                require(finite(mat.iridescence_ior) && mat.iridescence_ior >= 1 && finite(mat.iridescence_thickness_minimum) && mat.iridescence_thickness_minimum >= 0 &&
                            finite(mat.iridescence_thickness_maximum) && mat.iridescence_thickness_maximum >= mat.iridescence_thickness_minimum,
                        "Invalid iridescence film");
                for (int i = 0; i < 3; ++i) {
                    require(finite(mat.specular_color_factor[i]) && mat.specular_color_factor[i] >= 0 && unit(mat.sheen_color_factor[i]), "Invalid extension color");
                }
                require(finite(mat.normal_scale) && finite(mat.alpha_cutoff) && mat.alpha_cutoff >= 0, "Invalid material scale/cutoff");
                require(mat.alpha_mode == alpha_mode::opaque || mat.alpha_mode == alpha_mode::mask || mat.alpha_mode == alpha_mode::blend, "Invalid alpha mode");
                for (const auto *slot : texture_slots(mat)) {
                    require(slot->index == -1 || (slot->index >= 0 && size_t(slot->index) < meshes.textures.size()), "Material texture ID is out of range");
                    require(finite(slot->offset.x) && finite(slot->offset.y) && finite(slot->scale.x) && finite(slot->scale.y) && finite(slot->rotation), "Invalid UV transform");
                }
            }
        }

        void validate(const data &meshes) {
            validate_images(meshes);
            for (const auto &value : meshes.lights) {
                detail::validate_light(value);
                if (value.mesh_id != std::numeric_limits<uint32>::max()) {
                    require(value.mesh_id < meshes.meshes.size(), "Light mesh ID is out of range");
                }
                for (int column = 0; column < 4; ++column) {
                    for (int row = 0; row < 4; ++row) require(std::isfinite(value.model[column][row]), "Invalid mesh light transform");
                }
            }
            require(meshes.boxes.size() == meshes.meshes.size(), "Each mesh must have a bounding box");
            for (const auto &placement : meshes.instances) {
                require(placement.mesh_id < meshes.meshes.size(), "Instance mesh ID is out of range");
            }
            for (const auto &mesh : meshes.meshes) {
                require(mesh.lod_count > 0 && mesh.lod_count <= max_lods, "Invalid mesh LOD count");
                require(std::is_sorted(mesh.lod_offset.begin(), mesh.lod_offset.begin() + mesh.lod_count + 1), "Mesh LOD offsets are not ordered");
                require(uint64(mesh.index_offset) + mesh.lod_offset[mesh.lod_count] <= meshes.index_data.size(), "Mesh index range is outside index data");
            }
        }
    }

    header load_mesh_data(const vkz::path &mesh_file, data &out) {
        reader in(mesh_file);
        const auto info = in.value<header>();
        require(info.magic_value == header{}.magic_value, "Invalid mesh magic value");
        require(info.index_data_size % sizeof(uint32) == 0, "Invalid mesh index byte count");
        const auto version = in.value<uint32>();
        require(version >= 0x565A0001 && version <= stream_version, "Unsupported mesh vertex-layout format");
        data loaded;
        loaded.streams.bindings = in.array<VkVertexInputBindingDescription>(in.value<uint32>());
        loaded.streams.attributes = in.array<VkVertexInputAttributeDescription>(in.value<uint32>());
        loaded.meshes = in.array<base>(info.mesh_count);
        loaded.boxes = in.array<bounding_box>(info.mesh_count);
        loaded.index_data = in.array<uint32>(info.index_data_size / sizeof(uint32));
        loaded.vertex_data = in.array<uint8_t>(info.vertex_data_size);
        if (version >= 0x565A0002) loaded.instances = in.array<instance>(in.value<uint32>());
        if (version >= 0x565A0003) {
            const auto count = in.value<uint32>();
            require(count <= in.remaining / (version >= 0x565A0005 ? 124 : 56), "Invalid light record count");
            loaded.lights.resize(count);
            for (auto &value : loaded.lights) fields(in, value, version);
        }
        if (version >= 0x565A0004) {
            const auto count = in.value<uint32>();
            require(count <= in.remaining / 12, "Invalid image count");
            for (uint32 i = 0; i < count; ++i) {
                image img;
                img.uri = readString(in);
                img.mime_type = readString(in);
                img.encoded_data = in.array<uint8_t>(in.value<uint32>());
                loaded.images.push_back(std::move(img));
            }
        }
        require(in.remaining == 0, "Unexpected trailing mesh data");
        validate(loaded);
        out.streams = std::move(loaded.streams);
        out.meshes = std::move(loaded.meshes);
        out.instances = std::move(loaded.instances);
        out.lights = std::move(loaded.lights);
        if (version >= 0x565A0004) out.images = std::move(loaded.images);
        out.boxes = std::move(loaded.boxes);
        out.index_data = std::move(loaded.index_data);
        out.vertex_data = std::move(loaded.vertex_data);
        return info;
    }

    void save_mesh_data(const vkz::path &file_name, const data &meshes) {
        validate(meshes);
        size32(meshes.meshes.size());
        size32(meshes.index_data.size() * sizeof(uint32));
        size32(meshes.vertex_data.size());
        const auto instanceCount = size32(meshes.instances.size());
        size32(meshes.lights.size());
        size32(meshes.images.size());
        const auto bindings = size32(meshes.streams.bindings.size());
        const auto attributes = size32(meshes.streams.attributes.size());
        writer out(file_name);
        out.value(meshes.get_mesh_file_header());
        out.value(stream_version);
        out.value(bindings);
        out.array(meshes.streams.bindings);
        out.value(attributes);
        out.array(meshes.streams.attributes);
        out.array(meshes.meshes);
        out.array(meshes.boxes);
        out.array(meshes.index_data);
        out.array(meshes.vertex_data);
        out.value(instanceCount);
        out.array(meshes.instances);
        writeRecords(out, meshes.lights);
        out.value(size32(meshes.images.size()));
        for (const auto &img : meshes.images) {
            writeString(out, img.uri);
            writeString(out, img.mime_type);
            out.value(size32(img.encoded_data.size()));
            out.array(img.encoded_data);
        }
        out.finish();
    }

    void load_mesh_data_materials(const vkz::path &mesh_file, data &out) {
        reader in(mesh_file);
        require(in.value<uint32>() == materialMagic, "Unsupported material file; reimport legacy mesh materials");
        const auto version = in.value<uint32>();
        require(version >= 1 && version <= materialVersion, "Unsupported material file version");
        data loaded;
        const auto material_count = in.value<uint32>();
        require(material_count <= in.remaining / (version >= 5 ? 644 : version >= 4 ? 284 : version >= 3 ? 236 : 200), "Invalid material count");
        loaded.materials.resize(material_count);
        for (auto &mat : loaded.materials)
            fields(in, mat, version);
        loaded.samplers = readRecords<sampler>(in, 24);
        loaded.textures = readRecords<texture>(in, 8);
        const auto count = in.value<uint32>();
        if (version == 1) require(count <= in.remaining / 12, "Invalid image count");
        for (uint32 i = 0; version == 1 && i < count; ++i) {
            image img;
            img.uri = readString(in);
            img.mime_type = readString(in);
            img.encoded_data = in.array<uint8_t>(in.value<uint32>());
            loaded.images.push_back(std::move(img));
        }
        require(in.remaining == 0, "Unexpected trailing material data");
        if (version == 1)
            validateMaterials(loaded);
        else
            validateMaterials(loaded, count);
        out.materials = std::move(loaded.materials);
        out.samplers = std::move(loaded.samplers);
        out.textures = std::move(loaded.textures);
        if (version == 1) out.images = std::move(loaded.images);
    }

    void save_mesh_data_materials(const vkz::path &file_name, const data &meshes) {
        validateMaterials(meshes);
        size32(meshes.materials.size());
        size32(meshes.samplers.size());
        size32(meshes.textures.size());
        size32(meshes.images.size());
        writer out(file_name);
        out.value(materialMagic);
        out.value(materialVersion);
        writeRecords(out, meshes.materials);
        writeRecords(out, meshes.samplers);
        writeRecords(out, meshes.textures);
        out.value(size32(meshes.images.size()));
        out.finish();
    }

    bool is_mesh_data_valid(const vkz::path &file_name) {
        try {
            data meshes;
            load_mesh_data(file_name, meshes);
            return true;
        } catch (const std::exception &) {
            return false;
        }
    }

    bool is_mesh_materials_valid(const vkz::path &file_name) {
        try {
            data meshes;
            load_mesh_data_materials(file_name, meshes);
            return true;
        } catch (const std::exception &) {
            return false;
        }
    }

    bool is_mesh_hierarchy_valid(const vkz::path &file_name) {
        // Same existence/readability check as the cookbook; Scene serialization is separate.
        return std::ifstream(file_name, std::ios::binary).is_open();
    }

    void save_bounding_boxes(const vkz::path &file_name, const std::vector<bounding_box> &boxes) {
        const auto count = size32(boxes.size());
        writer out(file_name);
        out.value(count);
        out.array(boxes);
        out.finish();
    }

    void load_bounding_boxes(const vkz::path &file_name, std::vector<bounding_box> &boxes) {
        reader in(file_name);
        auto loaded = in.array<bounding_box>(in.value<uint32>());
        require(in.remaining == 0, "Unexpected trailing bounding-box data");
        boxes = std::move(loaded);
    }

    void recalculate_bounding_boxes(data &meshes) {
        if (meshes.meshes.empty()) {
            meshes.boxes.clear();
            return;
        }
        const auto stride = vertex_stride(meshes);
        const auto &attributes = meshes.streams.attributes;
        const auto position = std::find_if(attributes.begin(), attributes.end(), [](const auto &a) {
            return a.location == 0;
        });
        require(position != attributes.end() && position->format == VK_FORMAT_R32G32B32_SFLOAT && position->binding == meshes.streams.bindings.front().binding &&
                    uint64(position->offset) + sizeof(glm::vec3) <= stride,
                "Position must be a float3 at location zero within the vertex binding");
        std::vector<bounding_box> boxes;
        for (const auto &mesh : meshes.meshes) {
            require(mesh.lod_count > 0 && mesh.lod_count <= max_lods && mesh.lod_offset[1] >= mesh.lod_offset[0], "Invalid LOD zero");
            const auto begin = uint64(mesh.index_offset) + mesh.lod_offset[0];
            const auto end = uint64(mesh.index_offset) + mesh.lod_offset[1];
            require(end <= meshes.index_data.size(), "Mesh index range is outside index data");
            bounding_box box;
            for (auto i = begin; i < end; ++i) {
                const uint64 vertex = uint64(meshes.index_data[i]) + mesh.vertex_offset;
                require(vertex < meshes.vertex_data.size() / stride, "Mesh vertex index is outside vertex data");
                glm::vec3 point;
                std::memcpy(&point, meshes.vertex_data.data() + vertex * stride + position->offset, sizeof(point));
                if (i == begin)
                    box = bounding_box(point, point);
                else
                    box.combine_point(point);
            }
            boxes.push_back(box);
        }
        meshes.boxes = std::move(boxes);
    }

    header merge_mesh_data(data &meshes, const std::vector<data *> sources) {
        data merged;
        auto has_geometry = [](const data &source) {
            return !source.meshes.empty() || !source.vertex_data.empty() || !source.index_data.empty();
        };
        for (const auto *source : sources) {
            require(source != nullptr, "Null mesh merge source");
            if (has_geometry(*source)) {
                merged.streams = source->streams;
                break;
            }
        }
        for (const auto *source : sources) {
            require(source != nullptr, "Null mesh merge source");
            const bool geometry = has_geometry(*source);
            require(!geometry || same_layout(merged.streams, source->streams), "Cannot merge different vertex layouts");
            validate(*source);
            validateMaterials(*source);
            const auto vertex_offset = geometry ? merged.vertex_data.size() / vertex_stride(*source) : 0;
            const auto index_offset = merged.index_data.size();
            const auto materialOffset = merged.materials.size();
            const auto meshOffset = merged.meshes.size();
            const auto textureOffset = merged.textures.size();
            const auto imageOffset = merged.images.size();
            const auto samplerOffset = merged.samplers.size();
            for (auto placement : source->instances) {
                placement.mesh_id = size32(uint64(placement.mesh_id) + meshOffset);
                if (placement.material_id != inherit_material) {
                    require(placement.material_id < source->materials.size(), "Instance material ID is out of range");
                    placement.material_id = size32(uint64(placement.material_id) + materialOffset);
                }
                merged.instances.push_back(placement);
            }
            for (auto mesh : source->meshes) {
                mesh.index_offset = size32(uint64(mesh.index_offset) + index_offset);
                mesh.material_id = size32(uint64(mesh.material_id) + materialOffset);
                merged.meshes.push_back(mesh);
            }
            // Preserve each mesh's local vertex_offset, as in the cookbook.
            for (auto index : source->index_data)
                merged.index_data.push_back(size32(uint64(index) + vertex_offset));
            auto remap = [](int32 id, size_t offset) {
                if (id == -1) return id;
                require(uint64(id) + offset <= std::numeric_limits<int32>::max(), "Material resource index overflow");
                return static_cast<int32>(uint64(id) + offset);
            };
            for (auto mat : source->materials) {
                for (auto *slot : texture_slots(mat)) {
                    slot->index = remap(slot->index, textureOffset);
                }
                merged.materials.push_back(mat);
            }
            for (auto tex : source->textures) {
                tex.image_id = size32(uint64(tex.image_id) + imageOffset);
                tex.sampler_id = remap(tex.sampler_id, samplerOffset);
                merged.textures.push_back(tex);
            }
            merged.images.insert(merged.images.end(), source->images.begin(), source->images.end());
            merged.samplers.insert(merged.samplers.end(), source->samplers.begin(), source->samplers.end());
            merged.vertex_data.insert(merged.vertex_data.end(), source->vertex_data.begin(), source->vertex_data.end());
            merged.boxes.insert(merged.boxes.end(), source->boxes.begin(), source->boxes.end());
            for (auto value : source->lights) {
                if (value.mesh_id != std::numeric_limits<uint32>::max()) value.mesh_id = size32(uint64(value.mesh_id) + meshOffset);
                merged.lights.push_back(value);
            }
        }
        size32(merged.meshes.size());
        size32(merged.index_data.size() * sizeof(uint32));
        size32(merged.vertex_data.size());
        size32(merged.lights.size());
        meshes = std::move(merged);
        return meshes.get_mesh_file_header();
    }
}
