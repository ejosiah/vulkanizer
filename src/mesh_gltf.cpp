#include "mesh_import.hpp"
#include "mesh_light.hpp"

// Preserve encoded image payloads; decoding belongs to the texture uploader.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <bit>
#include <charconv>
#include <cctype>
#include <cmath>
#include <fstream>
#include <numeric>
#include <set>
#include <span>
#include <stdexcept>

namespace vkz::mesh::detail {
    namespace {
        void require(bool condition, const std::string &message) {
            if (!condition) throw std::runtime_error("glTF conversion: " + message);
        }

        uint32 size32(size_t size) {
            require(size <= std::numeric_limits<uint32>::max(), "32-bit mesh size exceeded");
            return static_cast<uint32>(size);
        }

        template <typename T> const T &checked(const std::vector<T> &values, int index) {
            require(index >= 0 && size_t(index) < values.size(), "resource index out of range");
            return values[size_t(index)];
        }

        std::string utf8(const path &file) {
            const auto text = file.generic_u8string();
            return {text.begin(), text.end()};
        }

        std::vector<unsigned char> read_file(const path &file) {
            std::ifstream stream(file, std::ios::binary | std::ios::ate);
            require(stream.is_open(), "cannot open " + utf8(file));
            const auto size = stream.tellg();
            require(size > 0 && uint64(size) <= std::numeric_limits<uint32>::max(), "empty or oversized file");
            std::vector<unsigned char> bytes(static_cast<size_t>(size));
            stream.seekg(0);
            stream.read(reinterpret_cast<char *>(bytes.data()), size);
            require(bool(stream), "cannot read " + utf8(file));
            return bytes;
        }

        tinygltf::Model load(const path &source) {
            tinygltf::TinyGLTF loader;
            loader.SetFsCallbacks({[](const std::string &name, void *) {
                                       return std::filesystem::is_regular_file(std::filesystem::u8path(name));
                                   },
                                   [](const std::string &name, void *) {
                                       return name;
                                   },
                                   [](std::vector<unsigned char> *out, std::string *error, const std::string &name, void *) {
                                       try {
                                           *out = read_file(std::filesystem::u8path(name));
                                           return true;
                                       } catch (const std::exception &e) {
                                           if (error) *error = e.what();
                                           return false;
                                       }
                                   },
                                   [](std::string *, const std::string &, const std::vector<unsigned char> &, void *) {
                                       return false;
                                   },
                                   [](size_t *size, std::string *, const std::string &name, void *) {
                                       std::error_code error;
                                       const auto count = std::filesystem::file_size(std::filesystem::u8path(name), error);
                                       if (error || count > std::numeric_limits<uint32>::max()) return false;
                                       *size = size_t(count);
                                       return true;
                                   },
                                   nullptr});
            loader.SetImageLoader(
                [](tinygltf::Image *image, int, std::string *, std::string *, int, int, const unsigned char *bytes, int count, void *) {
                    if (count <= 0) return false;
                    image->image.assign(bytes, bytes + count);
                    return true;
                },
                nullptr);
            const auto bytes = read_file(source);
            auto extension = source.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return char(std::tolower(c));
            });
            tinygltf::Model model;
            std::string error, warning;
            const auto directory = utf8(std::filesystem::absolute(source).parent_path());
            const bool success = extension == ".glb" ? loader.LoadBinaryFromMemory(&model, &error, &warning, bytes.data(), size32(bytes.size()), directory)
                                                     : loader.LoadASCIIFromString(&model, &error, &warning, reinterpret_cast<const char *>(bytes.data()), size32(bytes.size()), directory);
            require(success, error + warning);
            require(model.asset.version == "2.0" && (model.asset.minVersion.empty() || model.asset.minVersion == "2.0"), "only glTF 2.0 is supported");
            for (const auto &extension_name : model.extensionsRequired)
                require(extension_name == "KHR_texture_transform" || extension_name == "KHR_lights_punctual" || extension_name == "KHR_materials_ior" ||
                            extension_name == "KHR_materials_transmission" || extension_name == "KHR_materials_volume" || extension_name == "KHR_materials_specular" ||
                            extension_name == "KHR_materials_emissive_strength" || extension_name == "KHR_materials_unlit" || extension_name == "KHR_materials_clearcoat" ||
                            extension_name == "KHR_materials_sheen" || extension_name == "KHR_materials_anisotropy" || extension_name == "KHR_materials_iridescence" ||
                            extension_name == "KHR_materials_dispersion",
                        "unsupported required extension " + extension_name);
            return model;
        }

        std::vector<light> light_definitions(const tinygltf::Model &model) {
            std::vector<light> result;
            const auto extension = model.extensions.find("KHR_lights_punctual");
            if (extension == model.extensions.end()) return result;
            const auto &root = extension->second;
            require(root.IsObject() && root.Has("lights") && root.Get("lights").IsArray(), "invalid punctual light definitions");
            const auto &definitions = root.Get("lights");
            require(definitions.ArrayLen() == model.lights.size(), "invalid punctual light definition count");
            for (size_t i = 0; i < model.lights.size(); ++i) {
                const auto &source = model.lights[i];
                const auto &raw = definitions.Get(int(i));
                require(raw.IsObject(), "invalid punctual light definition");
                light value;
                require(source.type == "directional" || source.type == "point" || source.type == "spot", "invalid punctual light type");
                value.type = source.type == "directional" ? light_type::directional : source.type == "spot" ? light_type::spot : light_type::point;
                if (raw.Has("color")) {
                    require(raw.Get("color").IsArray() && source.color.size() == 3, "invalid light color");
                    for (int c = 0; c < 3; ++c) {
                        require(std::isfinite(source.color[c]) && source.color[c] >= 0 && source.color[c] <= 1, "invalid light color");
                        value.color[c] = float(source.color[c]);
                    }
                }
                if (raw.Has("intensity")) require(raw.Get("intensity").IsNumber(), "invalid light intensity");
                require(std::isfinite(source.intensity) && source.intensity >= 0, "invalid light intensity");
                value.intensity = float(source.intensity);
                // tinygltf uses zero both for omitted range and explicit zero. glTF
                // only allows omission (unbounded) or a strictly positive distance.
                if (raw.Has("range")) {
                    require(raw.Get("range").IsNumber() && std::isfinite(source.range) && source.range > 0, "explicit light range must be positive");
                    value.range = float(source.range);
                    require(std::isfinite(value.range) && value.range > 0, "light range cannot be represented");
                }
                if (value.type == light_type::directional) value.range = 0;
                if (value.type == light_type::spot) {
                    // tinygltf validates the required spot object while parsing, but
                    // its generic Value map drops empty objects (the valid defaults).
                    if (raw.Has("spot")) {
                        const auto &spot = raw.Get("spot");
                        require(spot.IsObject(), "invalid spotlight properties");
                        for (const char *key : {"innerConeAngle", "outerConeAngle"})
                            if (spot.Has(key)) require(spot.Get(key).IsNumber(), "invalid spotlight angle");
                    }
                    require(source.spot.innerConeAngle >= 0 && source.spot.innerConeAngle < source.spot.outerConeAngle && source.spot.outerConeAngle <= std::numbers::pi / 2.0,
                            "invalid spotlight cone angles");
                    value.inner_cone_angle = float(source.spot.innerConeAngle);
                    value.outer_cone_angle = float(source.spot.outerConeAngle);
                }
                validate_light(value);
                result.push_back(value);
            }
            return result;
        }

        // Accessors are read component-wise: no unaligned casts, and sparse overrides
        // are applied after the strided base data (or its implicit zero initialization).
        uint32 unsigned_value(const unsigned char *bytes, int size) {
            uint32 value = 0;
            for (int i = 0; i < size; ++i)
                value |= uint32(bytes[i]) << (8 * i);
            return value;
        }

        double component(const unsigned char *bytes, int type, bool normalized) {
            switch (type) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return std::bit_cast<float>(unsigned_value(bytes, 4));
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return normalized ? bytes[0] / 255.0 : bytes[0];
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const auto value = unsigned_value(bytes, 2);
                return normalized ? value / 65535.0 : value;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                return unsigned_value(bytes, 4);
            case TINYGLTF_COMPONENT_TYPE_BYTE: {
                const int value = bytes[0] < 128 ? bytes[0] : int(bytes[0]) - 256;
                return normalized ? std::max(-1.0, value / 127.0) : value;
            }
            case TINYGLTF_COMPONENT_TYPE_SHORT: {
                const int raw = int(unsigned_value(bytes, 2));
                const int value = raw < 32768 ? raw : raw - 65536;
                return normalized ? std::max(-1.0, value / 32767.0) : value;
            }
            default:
                throw std::runtime_error("glTF conversion: unsupported accessor component type");
            }
        }

        std::span<const unsigned char> view_bytes(const tinygltf::Model &model, int id) {
            const auto &view = checked(model.bufferViews, id);
            const auto &buffer = checked(model.buffers, view.buffer).data;
            require(view.byteOffset <= buffer.size() && view.byteLength <= buffer.size() - view.byteOffset, "buffer view exceeds buffer");
            return std::span(buffer).subspan(view.byteOffset, view.byteLength);
        }

        std::vector<double> read_accessor(const tinygltf::Model &model, int id, int dimensions) {
            const auto &accessor = checked(model.accessors, id);
            const int expected_type = dimensions == 1 ? TINYGLTF_TYPE_SCALAR : dimensions == 2 ? TINYGLTF_TYPE_VEC2 : dimensions == 3 ? TINYGLTF_TYPE_VEC3 : TINYGLTF_TYPE_VEC4;
            require(accessor.type == expected_type && dimensions >= 1 && dimensions <= 4, "unexpected accessor shape");
            const auto width = tinygltf::GetComponentSizeInBytes(accessor.componentType);
            require(width > 0 && width <= 4 && accessor.count > 0, "invalid accessor size");
            require(accessor.count <= std::numeric_limits<uint32>::max() / (dimensions * sizeof(float)), "accessor too large");
            std::vector<double> values(accessor.count * dimensions);
            const size_t packed = size_t(width) * dimensions;
            auto read = [&](int view_id, size_t offset, size_t count, size_t stride, auto destination) {
                const auto bytes = view_bytes(model, view_id);
                require(stride >= packed && offset <= bytes.size(), "invalid accessor stride/offset");
                require(count == 0 || (packed <= bytes.size() - offset && (count - 1) <= (bytes.size() - offset - packed) / stride), "accessor exceeds buffer view");
                for (size_t i = 0; i < count; ++i) {
                    const size_t dest = destination(i);
                    for (int c = 0; c < dimensions; ++c) {
                        const double value = component(bytes.data() + offset + i * stride + c * width, accessor.componentType, accessor.normalized);
                        require(std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max(), "non-finite attribute");
                        values[dest * dimensions + c] = value;
                    }
                }
            };
            if (accessor.bufferView >= 0) {
                const auto &view = checked(model.bufferViews, accessor.bufferView);
                read(accessor.bufferView, accessor.byteOffset, accessor.count, view.byteStride ? view.byteStride : packed, [](size_t index) {
                    return index;
                });
            } else
                require(accessor.byteOffset == 0, "offset without buffer view");
            if (accessor.sparse.isSparse) {
                const auto &sparse = accessor.sparse;
                require(sparse.count > 0 && size_t(sparse.count) <= accessor.count, "invalid sparse count");
                const int type = sparse.indices.componentType;
                require(type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, "invalid sparse index type");
                const size_t index_width = tinygltf::GetComponentSizeInBytes(type);
                const auto bytes = view_bytes(model, sparse.indices.bufferView);
                const auto offset = sparse.indices.byteOffset;
                require(offset <= bytes.size() && size_t(sparse.count) <= (bytes.size() - offset) / index_width, "sparse indices exceed view");
                std::vector<uint32> indices;
                for (int i = 0; i < sparse.count; ++i) {
                    const auto index = unsigned_value(bytes.data() + offset + i * index_width, int(index_width));
                    require(index < accessor.count && (indices.empty() || index > indices.back()), "invalid sparse index ordering/range");
                    indices.push_back(index);
                }
                read(sparse.values.bufferView, sparse.values.byteOffset, sparse.count, packed, [&](size_t index) {
                    return indices[index];
                });
            }
            return values;
        }

        template <typename Slot> texture_info texture_slot(const Slot &source) {
            require(source.index >= -1 && source.texCoord >= 0, "invalid texture slot");
            texture_info result{.index = source.index, .tex_coord = uint32(source.texCoord)};
            if (const auto it = source.extensions.find("KHR_texture_transform"); it != source.extensions.end()) {
                const auto &transform = it->second;
                require(transform.IsObject(), "invalid texture transform");
                auto vector = [&](const char *key, glm::vec2 &value) {
                    if (!transform.Has(key)) return;
                    const auto &array = transform.Get(key);
                    require(array.IsArray() && array.ArrayLen() == 2, "invalid texture transform vector");
                    for (int i = 0; i < 2; ++i) {
                        require(array.Get(i).IsNumber(), "invalid texture transform number");
                        value[i] = float(array.Get(i).GetNumberAsDouble());
                    }
                };
                vector("offset", result.offset);
                vector("scale", result.scale);
                if (transform.Has("rotation")) {
                    require(transform.Get("rotation").IsNumber(), "invalid texture rotation");
                    result.rotation = float(transform.Get("rotation").GetNumberAsDouble());
                }
                if (transform.Has("texCoord")) {
                    const auto &coord = transform.Get("texCoord");
                    require(coord.IsInt() && coord.GetNumberAsInt() >= 0, "invalid texture coordinate override");
                    result.tex_coord = uint32(coord.GetNumberAsInt());
                }
            }
            return result;
        }

        VkSamplerAddressMode wrap(int value) {
            switch (value) {
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            default:
                throw std::runtime_error("glTF conversion: invalid sampler wrap mode");
            }
        }

        void materials(const tinygltf::Model &model, data &out) {
            for (const auto &source : model.images) {
                require(!source.image.empty(), "image is missing or empty: " + source.uri);
                image result;
                result.encoded_data = source.image;
                result.mime_type = source.mimeType;
                if (result.mime_type.empty()) {
                    const auto &bytes = result.encoded_data;
                    if (bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G')
                        result.mime_type = "image/png";
                    else if (bytes.size() >= 3 && bytes[0] == 255 && bytes[1] == 216 && bytes[2] == 255)
                        result.mime_type = "image/jpeg";
                    else
                        require(false, "unrecognized encoded image type");
                }
                out.images.push_back(std::move(result));
            }
            for (const auto &source : model.samplers) {
                sampler result;
                require(source.magFilter == -1 || source.magFilter == 9728 || source.magFilter == 9729, "invalid magnification filter");
                result.mag_filter = source.magFilter == 9728 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
                switch (source.minFilter) {
                case -1:
                    break;
                case 9728:
                    result.min_filter = VK_FILTER_NEAREST;
                    result.use_mipmaps = false;
                    break;
                case 9729:
                    result.use_mipmaps = false;
                    break;
                case 9984:
                    result.min_filter = VK_FILTER_NEAREST;
                    result.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                    break;
                case 9985:
                    result.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                    break;
                case 9986:
                    result.min_filter = VK_FILTER_NEAREST;
                    break;
                case 9987:
                    break;
                default:
                    require(false, "invalid minification filter");
                }
                result.wrap_s = wrap(source.wrapS);
                result.wrap_t = wrap(source.wrapT);
                out.samplers.push_back(result);
            }
            for (const auto &source : model.textures) {
                checked(model.images, source.source);
                require(source.sampler >= -1, "invalid sampler index");
                if (source.sampler >= 0) checked(model.samplers, source.sampler);
                out.textures.push_back({.image_id = uint32(source.source), .sampler_id = source.sampler});
            }
            for (const auto &source : model.materials) {
                material result;
                const auto &pbr = source.pbrMetallicRoughness;
                require(pbr.baseColorFactor.size() == 4 && source.emissiveFactor.size() == 3, "invalid material factor dimensions");
                for (int i = 0; i < 4; ++i)
                    result.base_color_factor[i] = float(pbr.baseColorFactor[i]);
                for (int i = 0; i < 3; ++i)
                    result.emissive_factor[i] = float(source.emissiveFactor[i]);
                result.metallic_factor = float(pbr.metallicFactor);
                result.roughness_factor = float(pbr.roughnessFactor);
                result.normal_scale = float(source.normalTexture.scale);
                result.occlusion_strength = float(source.occlusionTexture.strength);
                require(source.alphaMode == "OPAQUE" || source.alphaMode == "MASK" || source.alphaMode == "BLEND", "invalid alpha mode");
                result.alpha_mode = source.alphaMode == "MASK" ? alpha_mode::mask : source.alphaMode == "BLEND" ? alpha_mode::blend : alpha_mode::opaque;
                result.alpha_cutoff = float(source.alphaCutoff);
                result.double_sided = source.doubleSided;
                result.base_color_texture = texture_slot(pbr.baseColorTexture);
                result.metallic_roughness_texture = texture_slot(pbr.metallicRoughnessTexture);
                result.normal_texture = texture_slot(source.normalTexture);
                result.occlusion_texture = texture_slot(source.occlusionTexture);
                result.emissive_texture = texture_slot(source.emissiveTexture);
                auto optical_factor = [&](const tinygltf::Value &extension, const char *name, float &target) {
                    require(extension.IsObject(), "invalid optical material extension");
                    if (extension.Has(name)) {
                        require(extension.Get(name).IsNumber(), "invalid optical material factor");
                        target = float(extension.Get(name).GetNumberAsDouble());
                    }
                };
                if (const auto it = source.extensions.find("KHR_materials_ior"); it != source.extensions.end()) optical_factor(it->second, "ior", result.ior);
                auto optical_texture = [&](const tinygltf::Value &extension, const char *name, texture_info &target) {
                    if (!extension.Has(name)) return;
                    const auto &value = extension.Get(name);
                    require(value.IsObject() && value.Has("index") && value.Get("index").IsInt() && value.Get("index").GetNumberAsInt() >= 0, "invalid optical texture");
                    tinygltf::TextureInfo slot;
                    slot.index = value.Get("index").GetNumberAsInt();
                    if (value.Has("texCoord")) {
                        require(value.Get("texCoord").IsInt(), "invalid optical UV set");
                        slot.texCoord = value.Get("texCoord").GetNumberAsInt();
                    }
                    if (value.Has("extensions")) {
                        require(value.Get("extensions").IsObject(), "invalid optical texture extensions");
                        slot.extensions = value.Get("extensions").Get<tinygltf::Value::Object>();
                    }
                    target = texture_slot(slot);
                };
                if (const auto it = source.extensions.find("KHR_materials_transmission"); it != source.extensions.end()) {
                    optical_factor(it->second, "transmissionFactor", result.transmission_factor);
                    optical_texture(it->second, "transmissionTexture", result.transmission_texture);
                }
                if (const auto it = source.extensions.find("KHR_materials_volume"); it != source.extensions.end()) {
                    optical_factor(it->second, "thicknessFactor", result.thickness_factor);
                    optical_factor(it->second, "attenuationDistance", result.attenuation_distance);
                    if (it->second.Has("attenuationDistance")) require(std::isfinite(result.attenuation_distance) && result.attenuation_distance > 0, "invalid attenuation distance");
                    optical_texture(it->second, "thicknessTexture", result.thickness_texture);
                    if (it->second.Has("attenuationColor")) {
                        const auto &color = it->second.Get("attenuationColor");
                        require(color.IsArray() && color.ArrayLen() == 3, "invalid attenuation color");
                        for (int i = 0; i < 3; ++i) {
                            require(color.Get(i).IsNumber(), "invalid attenuation color component");
                            result.attenuation_color[i] = float(color.Get(i).GetNumberAsDouble());
                            require(std::isfinite(result.attenuation_color[i]) && result.attenuation_color[i] >= 0 && result.attenuation_color[i] <= 1, "invalid attenuation color range");
                        }
                    }
                    require(std::isfinite(result.thickness_factor) && result.thickness_factor >= 0, "invalid volume thickness");
                }
                auto extension_color = [&](const tinygltf::Value &extension, const char *name, glm::vec3 &target) {
                    if (!extension.Has(name)) return;
                    const auto &value = extension.Get(name);
                    require(value.IsArray() && value.ArrayLen() == 3, "Invalid extension color");
                    for (int i = 0; i < 3; ++i) {
                        require(value.Get(i).IsNumber(), "Invalid extension color component");
                        target[i] = float(value.Get(i).GetNumberAsDouble());
                    }
                };
                if (const auto it = source.extensions.find("KHR_materials_specular"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    optical_factor(it->second, "specularFactor", result.specular_factor);
                    extension_color(it->second, "specularColorFactor", result.specular_color_factor);
                    optical_texture(it->second, "specularTexture", result.specular_texture);
                    optical_texture(it->second, "specularColorTexture", result.specular_color_texture);
                }
                if (const auto it = source.extensions.find("KHR_materials_emissive_strength"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    optical_factor(it->second, "emissiveStrength", result.emissive_strength);
                }
                if (const auto it = source.extensions.find("KHR_materials_unlit"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    result.unlit = true;
                }
                if (const auto it = source.extensions.find("KHR_materials_clearcoat"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    optical_factor(it->second, "clearcoatFactor", result.clearcoat_factor);
                    optical_factor(it->second, "clearcoatRoughnessFactor", result.clearcoat_roughness_factor);
                    optical_texture(it->second, "clearcoatTexture", result.clearcoat_texture);
                    optical_texture(it->second, "clearcoatRoughnessTexture", result.clearcoat_roughness_texture);
                    optical_texture(it->second, "clearcoatNormalTexture", result.clearcoat_normal_texture);
                    if (it->second.Has("clearcoatNormalTexture")) optical_factor(it->second.Get("clearcoatNormalTexture"), "scale", result.clearcoat_normal_scale);
                }
                if (const auto it = source.extensions.find("KHR_materials_sheen"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    extension_color(it->second, "sheenColorFactor", result.sheen_color_factor);
                    optical_factor(it->second, "sheenRoughnessFactor", result.sheen_roughness_factor);
                    optical_texture(it->second, "sheenColorTexture", result.sheen_color_texture);
                    optical_texture(it->second, "sheenRoughnessTexture", result.sheen_roughness_texture);
                }
                if (const auto it = source.extensions.find("KHR_materials_anisotropy"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    optical_factor(it->second, "anisotropyStrength", result.anisotropy_strength);
                    optical_factor(it->second, "anisotropyRotation", result.anisotropy_rotation);
                    optical_texture(it->second, "anisotropyTexture", result.anisotropy_texture);
                }
                if (const auto it = source.extensions.find("KHR_materials_iridescence"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    optical_factor(it->second, "iridescenceFactor", result.iridescence_factor);
                    optical_factor(it->second, "iridescenceIor", result.iridescence_ior);
                    optical_factor(it->second, "iridescenceThicknessMinimum", result.iridescence_thickness_minimum);
                    optical_factor(it->second, "iridescenceThicknessMaximum", result.iridescence_thickness_maximum);
                    optical_texture(it->second, "iridescenceTexture", result.iridescence_texture);
                    optical_texture(it->second, "iridescenceThicknessTexture", result.iridescence_thickness_texture);
                }
                if (const auto it = source.extensions.find("KHR_materials_dispersion"); it != source.extensions.end()) {
                    require(it->second.IsObject(), "Invalid material extension");
                    optical_factor(it->second, "dispersion", result.dispersion);
                }
                require(std::isfinite(result.ior) && (result.ior == 0 || result.ior >= 1), "invalid IOR");
                require(std::isfinite(result.transmission_factor) && result.transmission_factor >= 0 && result.transmission_factor <= 1, "invalid transmission factor");
                for (const auto *slot : texture_slots(result))
                    if (slot->index >= 0) checked(model.textures, slot->index);
                out.materials.push_back(result);
            }
            // The final material is the glTF default for primitives without a material.
            out.materials.emplace_back();
        }

        uint32 uv_set(const std::string &name) {
            uint32 result{};
            const auto suffix = std::string_view(name).substr(9);
            const auto parsed = std::from_chars(suffix.data(), suffix.data() + suffix.size(), result);
            require(parsed.ec == std::errc{} && parsed.ptr == suffix.data() + suffix.size() && result <= std::numeric_limits<uint32>::max() - 4, "invalid TEXCOORD semantic");
            return result;
        }

        struct vertex {
            glm::vec3 position{};
            glm::vec3 normal{};
            glm::vec4 tangent{}; // Zero means absent; derive a frame from triangle UVs.
            glm::vec4 color{1};
            std::vector<glm::vec2> uv;
        };

        void primitive(const tinygltf::Model &model, const tinygltf::Primitive &source, const std::set<uint32> &uv_sets, data &out) {
            require(source.targets.empty(), "morph targets are not supported");
            require(!source.extensions.contains("KHR_draco_mesh_compression"), "Draco geometry is not supported");
            const auto position = source.attributes.find("POSITION");
            require(position != source.attributes.end(), "primitive has no POSITION");
            const auto &positions = checked(model.accessors, position->second);
            require(positions.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && !positions.normalized, "POSITION must be float3");
            const auto coordinates = read_accessor(model, position->second, 3);
            std::vector<vertex> vertices(positions.count);
            for (size_t i = 0; i < vertices.size(); ++i) {
                vertices[i].position = glm::vec3(coordinates[i * 3], coordinates[i * 3 + 1], coordinates[i * 3 + 2]);
                vertices[i].uv.resize(uv_sets.size());
            }
            bool has_normals = false;
            for (const auto &[semantic, id] : source.attributes) {
                if (semantic == "POSITION" || semantic.starts_with('_')) continue;
                require(!semantic.starts_with("JOINTS_") && !semantic.starts_with("WEIGHTS_"), "skinned attributes are not supported");
                const auto &accessor = checked(model.accessors, id);
                require(accessor.count == vertices.size(), "attribute vertex counts differ");
                int dimensions = 0;
                size_t uv_index = 0;
                if (semantic == "NORMAL") {
                    dimensions = 3;
                    has_normals = true;
                } else if (semantic == "TANGENT")
                    dimensions = 4;
                else if (semantic == "COLOR_0") {
                    dimensions = tinygltf::GetNumComponentsInType(accessor.type);
                    require(dimensions == 3 || dimensions == 4, "COLOR_0 must be vec3 or vec4");
                } else if (semantic.starts_with("TEXCOORD_")) {
                    dimensions = 2;
                    uv_index = size_t(std::distance(uv_sets.begin(), uv_sets.find(uv_set(semantic))));
                } else {
                    require(false, "unsupported vertex semantic " + semantic);
                }
                if (semantic == "NORMAL" || semantic == "TANGENT")
                    require(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && !accessor.normalized, "normal/tangent must use floats");
                else
                    require((accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && !accessor.normalized) ||
                                ((accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) && accessor.normalized),
                            "invalid color/UV component type");
                const auto values = read_accessor(model, id, dimensions);
                for (size_t i = 0; i < vertices.size(); ++i) {
                    if (dimensions == 2)
                        vertices[i].uv[uv_index] = glm::vec2(values[i * 2], values[i * 2 + 1]);
                    else
                        for (int c = 0; c < dimensions; ++c) {
                            if (semantic == "NORMAL")
                                vertices[i].normal[c] = float(values[i * dimensions + c]);
                            else if (semantic == "TANGENT")
                                vertices[i].tangent[c] = float(values[i * dimensions + c]);
                            else
                                vertices[i].color[c] = float(values[i * dimensions + c]);
                        }
                }
            }
            require(source.material >= -1, "invalid primitive material");
            const auto material_id = source.material < 0 ? size32(model.materials.size()) : uint32(source.material);
            if (source.material >= 0) checked(model.materials, source.material);
            const auto &mat = out.materials[material_id];
            for (const auto *slot : texture_slots(mat))
                require(slot->index < 0 || source.attributes.contains("TEXCOORD_" + std::to_string(slot->tex_coord)), "material references a missing UV set");
            std::vector<uint32> indices;
            require(source.indices >= -1, "invalid index accessor");
            if (source.indices >= 0) {
                const auto &accessor = checked(model.accessors, source.indices);
                require(!accessor.normalized && (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
                                                 accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT),
                        "invalid index type");
                for (const auto value : read_accessor(model, source.indices, 1)) {
                    require(value < vertices.size(), "index exceeds vertex count");
                    indices.push_back(uint32(value));
                }
            } else {
                indices.resize(vertices.size());
                std::iota(indices.begin(), indices.end(), 0u);
            }
            const int mode = source.mode < 0 ? TINYGLTF_MODE_TRIANGLES : source.mode;
            if (mode == TINYGLTF_MODE_TRIANGLE_STRIP || mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                require(indices.size() >= 3, "too few triangle vertices");
                std::vector<uint32> triangles;
                for (size_t i = 2; i < indices.size(); ++i) {
                    if (mode == TINYGLTF_MODE_TRIANGLE_FAN)
                        triangles.insert(triangles.end(), {indices[0], indices[i - 1], indices[i]});
                    else if (i % 2)
                        triangles.insert(triangles.end(), {indices[i - 1], indices[i - 2], indices[i]});
                    else
                        triangles.insert(triangles.end(), {indices[i - 2], indices[i - 1], indices[i]});
                }
                indices = std::move(triangles);
            } else
                require(mode == TINYGLTF_MODE_TRIANGLES && indices.size() % 3 == 0, "only triangle primitives are supported");
            if (!has_normals) {
                std::vector<vertex> flat;
                flat.reserve(indices.size());
                for (size_t i = 0; i < indices.size(); i += 3) {
                    auto normal = glm::cross(vertices[indices[i + 1]].position - vertices[indices[i]].position, vertices[indices[i + 2]].position - vertices[indices[i]].position);
                    const auto length = glm::length(normal);
                    normal = length > 0 ? normal / length : glm::vec3(0, 0, 1);
                    for (int c = 0; c < 3; ++c) {
                        auto value = vertices[indices[i + c]];
                        value.normal = normal;
                        value.tangent = glm::vec4(0);
                        flat.push_back(std::move(value));
                    }
                }
                vertices = std::move(flat);
                std::iota(indices.begin(), indices.end(), 0u);
            }
            base mesh;
            mesh.vertex_offset = size32(out.vertex_data.size() / out.streams.bindings[0].stride);
            mesh.vertex_count = size32(vertices.size());
            mesh.index_offset = size32(out.index_data.size());
            mesh.lod_offset[1] = size32(indices.size());
            mesh.material_id = material_id;
            out.index_data.insert(out.index_data.end(), indices.begin(), indices.end());
            for (const auto &value : vertices) {
                for (int c = 0; c < 3; ++c)
                    put(out.vertex_data, value.position[c]);
                for (int c = 0; c < 3; ++c)
                    put(out.vertex_data, value.normal[c]);
                for (int c = 0; c < 4; ++c)
                    put(out.vertex_data, value.tangent[c]);
                for (int c = 0; c < 4; ++c)
                    put(out.vertex_data, value.color[c]);
                for (const auto &uv : value.uv) {
                    put(out.vertex_data, uv.x);
                    put(out.vertex_data, uv.y);
                }
            }
            out.meshes.push_back(mesh);
        }

        glm::mat4 local_transform(const tinygltf::Node &node) {
            glm::mat4 result(1);
            if (!node.matrix.empty()) {
                require(node.matrix.size() == 16 && node.translation.empty() && node.rotation.empty() && node.scale.empty(), "invalid matrix/TRS transform");
                for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r)
                        result[c][r] = float(node.matrix[c * 4 + r]);
            } else {
                if (!node.translation.empty()) {
                    require(node.translation.size() == 3, "invalid translation");
                    result = glm::translate(result, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
                }
                if (!node.rotation.empty()) {
                    require(node.rotation.size() == 4, "invalid rotation");
                    glm::quat rotation(float(node.rotation[3]), float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2]));
                    require(std::isfinite(glm::length(rotation)) && glm::length(rotation) > 0, "invalid quaternion");
                    result *= glm::mat4_cast(glm::normalize(rotation));
                }
                if (!node.scale.empty()) {
                    require(node.scale.size() == 3, "invalid scale");
                    result = glm::scale(result, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
                }
            }
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    require(std::isfinite(result[c][r]), "non-finite transform");
            require(result[0][3] == 0 && result[1][3] == 0 && result[2][3] == 0 && result[3][3] == 1, "non-affine node matrix");
            return result;
        }
    }

    data import_gltf(const path &source) {
        const auto model = load(source);
        const auto lights = light_definitions(model);
        data out;
        materials(model, out);
        std::set<uint32> uv_sets;
        for (const auto &mesh : model.meshes)
            for (const auto &primitive : mesh.primitives)
                for (const auto &[semantic, id] : primitive.attributes)
                    if (semantic.starts_with("TEXCOORD_")) uv_sets.insert(uv_set(semantic));
        out.streams.attributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}, {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 24}, {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 40}};
        uint32 stride = 56;
        for (const auto set : uv_sets) {
            out.streams.attributes.push_back({4 + set, 0, VK_FORMAT_R32G32_SFLOAT, stride});
            stride += 8;
        }
        out.streams.bindings.push_back({0, stride, VK_VERTEX_INPUT_RATE_VERTEX});
        std::vector<std::vector<uint32>> mesh_ids(model.meshes.size());
        std::vector<bool> imported(model.meshes.size());
        std::vector<bool> visited(model.nodes.size());
        auto visit = [&](auto &&self, int id, const glm::mat4 &parent) -> void {
            const auto &node = checked(model.nodes, id);
            require(!visited[id], "cycle or repeated node in scene hierarchy");
            visited[id] = true;
            require(node.skin < 0 && node.weights.empty(), "skinning/morph weights are not supported");
            const auto world = parent * local_transform(node);
            if (node.extensions.contains("KHR_lights_punctual")) {
                auto value = checked(lights, node.light);
                if (value.type != light_type::directional) value.position = glm::vec3(world[3]);
                if (value.type != light_type::point) {
                    // Transform the local -Z emission axis, then remove its length.
                    // Scale never changes intensity, range, or the spotlight cone.
                    const auto axis = -glm::vec3(world[2]);
                    const auto length = glm::length(axis);
                    require(std::isfinite(length) && length > 0, "light transform has no valid emission direction");
                    value.direction = axis / length;
                }
                validate_light(value);
                out.lights.push_back(value);
            }
            if (node.mesh >= 0) {
                const auto &source_mesh = checked(model.meshes, node.mesh);
                require(source_mesh.weights.empty(), "morph weights are not supported");
                if (!imported[node.mesh]) {
                    for (const auto &source_primitive : source_mesh.primitives) {
                        mesh_ids[node.mesh].push_back(size32(out.meshes.size()));
                        primitive(model, source_primitive, uv_sets, out);
                    }
                    imported[node.mesh] = true;
                }
                const auto determinant = glm::determinant(world);
                require(std::isfinite(determinant) && determinant != 0, "singular instance transform");
                const auto inverse = glm::inverse(world);
                for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r)
                        require(std::isfinite(world[c][r]) && std::isfinite(inverse[c][r]), "non-finite instance transform");
                for (const auto mesh_id : mesh_ids[node.mesh])
                    out.instances.push_back({.model = world, .model_inverse = inverse, .mesh_id = mesh_id});
            }
            for (const auto child : node.children)
                self(self, child, world);
        };
        if (!model.scenes.empty()) {
            const auto &scene = checked(model.scenes, model.defaultScene < 0 ? 0 : model.defaultScene);
            for (const auto root : scene.nodes)
                visit(visit, root, glm::mat4(1));
        } else {
            std::vector<bool> child(model.nodes.size());
            for (const auto &node : model.nodes)
                for (const auto id : node.children) {
                    checked(model.nodes, id);
                    child[id] = true;
                }
            for (size_t id = 0; id < model.nodes.size(); ++id)
                if (!child[id]) visit(visit, int(id), glm::mat4(1));
            require(std::all_of(visited.begin(), visited.end(),
                                [](bool value) {
                                    return value;
                                }),
                    "cyclic node hierarchy");
        }
        require(!out.meshes.empty() || !out.lights.empty(), "selected scene has no meshes or lights");
        recalculate_bounding_boxes(out);
        return out;
    }
}
