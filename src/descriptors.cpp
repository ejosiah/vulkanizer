#include "vulkanizer/descriptors.hpp"
#include "vulkanizer/status.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

namespace vkz {
    namespace {
        template<class... Ts>
        struct overloaded : Ts... { using Ts::operator()...; };

        template<class... Ts>
        overloaded(Ts...) -> overloaded<Ts...>;

        struct pending_write {
            VkDescriptorSet set{};
            uint32_t binding{};
            uint32_t array_element{};
            VkDescriptorType type{};
            size_t info_offset{};
            uint32_t count{};
            bool uses_buffer_info{};
        };

        constexpr uint32_t automatic_binding = ~uint32_t{0};

        uint32_t descriptor_binding(const descriptor_t& value) {
            return std::visit(overloaded{
                [](const buffer_descriptor& d) { return d.binding; },
                [](const buffer_element_descriptor& d) { return d.descriptor.binding; },
                [](const buffer_array_descriptor& d) { return d.binding; },
                [](const ubo_descriptor& d) { return d.binding; },
                [](const ubo_element_descriptor& d) { return d.descriptor.binding; },
                [](const ubo_array_descriptor& d) { return d.binding; },
                [](const image_descriptor& d) { return d.binding; },
                [](const input_attachment_descriptor& d) { return d.binding; },
                [](const image_element_descriptor& d) { return d.image.binding; },
                [](const image_array_descriptor& d) { return d.binding; },
                [](const texture_descriptor& d) { return d.binding; },
                [](const texture_element_descriptor& d) { return d.texture.binding; },
                [](const texture_array_descriptor& d) { return d.binding; },
            }, value);
        }

        uint32_t resolve_binding(uint32_t binding, size_t position) {
            if (binding != automatic_binding) {
                return binding;
            }
            if (position > std::numeric_limits<uint32_t>::max()) {
                VKZ_THROW("Descriptor binding position exceeds the Vulkan uint32_t limit")
            }
            return static_cast<uint32_t>(position);
        }
    }

    void update_descriptor(vkz::device device, descriptor_bindings bindings) {
        update_descriptors(device, {std::move(bindings)});
    }

    void update_descriptors(
        vkz::device device,
        std::initializer_list<descriptor_bindings> bindings) {
        if (!device.logical) {
            VKZ_THROW("Cannot update descriptors with a null logical device")
        }

        std::vector<descriptor_bindings> sorted_bindings{bindings};
        for (auto& set_bindings : sorted_bindings) {
            std::stable_sort(
                set_bindings.bindings.begin(),
                set_bindings.bindings.end(),
                [](const descriptor_t& lhs, const descriptor_t& rhs) {
                    return descriptor_binding(lhs) < descriptor_binding(rhs);
                });
        }

        size_t write_count = 0;
        size_t buffer_info_count = 0;
        size_t image_info_count = 0;

        for (const auto& set_bindings : sorted_bindings) {
            if (!set_bindings.descriptor_set.handle && !set_bindings.bindings.empty()) {
                VKZ_THROW("Cannot update a null descriptor set")
            }
            for (const auto& value : set_bindings.bindings) {
                std::visit(overloaded{
                    [&](const buffer_array_descriptor& descriptors) {
                        write_count += !descriptors.empty();
                        buffer_info_count += descriptors.size();
                    },
                    [&](const ubo_array_descriptor& descriptors) {
                        write_count += !descriptors.empty();
                        buffer_info_count += descriptors.size();
                    },
                    [&](const image_array_descriptor& descriptors) {
                        write_count += !descriptors.empty();
                        image_info_count += descriptors.size();
                    },
                    [&](const texture_array_descriptor& descriptors) {
                        write_count += !descriptors.empty();
                        image_info_count += descriptors.size();
                    },
                    [&](const auto& descriptor) {
                        ++write_count;
                        using T = std::decay_t<decltype(descriptor)>;
                        if constexpr (std::is_same_v<T, buffer_descriptor> ||
                                      std::is_same_v<T, buffer_element_descriptor> ||
                                      std::is_same_v<T, ubo_descriptor> ||
                                      std::is_same_v<T, ubo_element_descriptor>) {
                            ++buffer_info_count;
                        } else {
                            ++image_info_count;
                        }
                    }
                }, value);
            }
        }

        if (write_count > std::numeric_limits<uint32_t>::max() ||
            buffer_info_count > std::numeric_limits<uint32_t>::max() ||
            image_info_count > std::numeric_limits<uint32_t>::max()) {
            VKZ_THROW("Descriptor update count exceeds the Vulkan uint32_t limit")
        }
        if (!write_count) {
            return;
        }

        std::vector<VkDescriptorBufferInfo> buffer_infos;
        std::vector<VkDescriptorImageInfo> image_infos;
        std::vector<pending_write> pending;
        buffer_infos.reserve(buffer_info_count);
        image_infos.reserve(image_info_count);
        pending.reserve(write_count);

        const auto add_buffer = [&](VkDescriptorSet set, uint32_t binding,
                                    uint32_t element, VkDescriptorType type,
                                    const auto& descriptor) {
            const size_t offset = buffer_infos.size();
            buffer_infos.push_back({descriptor.buffer, descriptor.start, descriptor.end});
            pending.push_back({set, binding, element, type, offset, 1, true});
        };
        const auto add_image = [&](VkDescriptorSet set, uint32_t binding,
                                   uint32_t element, VkDescriptorType type,
                                   const auto& descriptor) {
            const size_t offset = image_infos.size();
            if constexpr (std::is_same_v<std::decay_t<decltype(descriptor)>, texture_descriptor>) {
                image_infos.push_back({descriptor.sampler, descriptor.view, descriptor.layout});
            } else {
                image_infos.push_back({VK_NULL_HANDLE, descriptor.view, descriptor.layout});
            }
            pending.push_back({set, binding, element, type, offset, 1, false});
        };

        for (const auto& set_bindings : sorted_bindings) {
            const VkDescriptorSet set = set_bindings.descriptor_set.handle;
            for (size_t position = 0; position < set_bindings.bindings.size(); ++position) {
                const auto& value = set_bindings.bindings[position];
                std::visit(overloaded{
                    [&](const buffer_descriptor& d) {
                        add_buffer(set, resolve_binding(d.binding, position), 0,
                                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, d);
                    },
                    [&](const buffer_element_descriptor& d) {
                        add_buffer(set, resolve_binding(d.descriptor.binding, position),
                                   d.array_element_location,
                                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, d.descriptor);
                    },
                    [&](const buffer_array_descriptor& ds) {
                        if (ds.empty()) return;
                        const size_t offset = buffer_infos.size();
                        for (const auto& d : ds)
                            buffer_infos.push_back({d.buffer, d.start, d.end});
                        pending.push_back({set, resolve_binding(ds.binding, position), 0,
                                           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                           offset, static_cast<uint32_t>(ds.size()), true});
                    },
                    [&](const ubo_descriptor& d) {
                        add_buffer(set, resolve_binding(d.binding, position), 0,
                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, d);
                    },
                    [&](const ubo_element_descriptor& d) {
                        add_buffer(set, resolve_binding(d.descriptor.binding, position),
                                   d.array_element_location,
                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, d.descriptor);
                    },
                    [&](const ubo_array_descriptor& ds) {
                        if (ds.empty()) return;
                        const size_t offset = buffer_infos.size();
                        for (const auto& d : ds)
                            buffer_infos.push_back({d.buffer, d.start, d.end});
                        pending.push_back({set, resolve_binding(ds.binding, position), 0,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           offset, static_cast<uint32_t>(ds.size()), true});
                    },
                    [&](const image_descriptor& d) {
                        add_image(set, resolve_binding(d.binding, position), 0,
                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, d);
                    },
                    [&](const input_attachment_descriptor& d) {
                        add_image(set, resolve_binding(d.binding, position), 0,
                                  VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, d);
                    },
                    [&](const image_element_descriptor& d) {
                        add_image(set, resolve_binding(d.image.binding, position),
                                  d.array_element_location,
                                  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, d.image);
                    },
                    [&](const image_array_descriptor& ds) {
                        if (ds.empty()) return;
                        const size_t offset = image_infos.size();
                        for (const auto& d : ds)
                            image_infos.push_back({VK_NULL_HANDLE, d.view, d.layout});
                        pending.push_back({set, resolve_binding(ds.binding, position), 0,
                                           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           offset, static_cast<uint32_t>(ds.size()), false});
                    },
                    [&](const texture_descriptor& d) {
                        add_image(set, resolve_binding(d.binding, position), 0,
                                  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, d);
                    },
                    [&](const texture_element_descriptor& d) {
                        add_image(set, resolve_binding(d.texture.binding, position),
                                  d.array_element_location,
                                  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, d.texture);
                    },
                    [&](const texture_array_descriptor& ds) {
                        if (ds.empty()) return;
                        const size_t offset = image_infos.size();
                        for (const auto& d : ds)
                            image_infos.push_back({d.sampler, d.view, d.layout});
                        pending.push_back({set, resolve_binding(ds.binding, position), 0,
                                           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                           offset, static_cast<uint32_t>(ds.size()), false});
                    }
                }, value);
            }
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(pending.size());
        for (const auto& item : pending) {
            VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = item.set,
                .dstBinding = item.binding,
                .dstArrayElement = item.array_element,
                .descriptorCount = item.count,
                .descriptorType = item.type,
            };
            if (item.uses_buffer_info)
                write.pBufferInfo = buffer_infos.data() + item.info_offset;
            else
                write.pImageInfo = image_infos.data() + item.info_offset;
            writes.push_back(write);
        }

        vkUpdateDescriptorSets(device.logical, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }
}
