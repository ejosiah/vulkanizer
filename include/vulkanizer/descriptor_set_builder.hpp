#pragma once

#include "vkz.hpp"
#include "descriptors.hpp"
#include "memory.hpp"

#include <vector>
#include <algorithm>

namespace vkz {

    class descriptor_set_layout_builder {
    public:
        explicit descriptor_set_layout_builder(vkz::device device) : device(device) {}

        class descriptor_set_layout_binding_builder {
        public:
            explicit descriptor_set_layout_binding_builder(
                    const vkz::device &device,
                    std::vector<VkDescriptorSetLayoutBinding> &bindings,
                    std::vector<std::vector<VkSampler>> &immutable_sampler_storage,
                    std::string name,
                    bool bindless_enabled,
                    uint32_t binding_value
            )
                    : device(device), bindings(bindings), immutable_sampler_storage(immutable_sampler_storage),
                      _name{name}, bindless_enabled(bindless_enabled) {
                _binding.binding = binding_value;
            };

            descriptor_set_layout_binding_builder binding(uint32_t value) const {
                assert_binding();
                bindings.push_back(_binding);
                return descriptor_set_layout_binding_builder{
                        device, bindings, immutable_sampler_storage, _name, bindless_enabled, value};
            }

            const descriptor_set_layout_binding_builder &descriptor_count(uint32_t count) const {
                _binding.descriptorCount = count;
                return *this;
            }

            const descriptor_set_layout_binding_builder &descriptor_type(VkDescriptorType type) const {
                _binding.descriptorType = type;
                return *this;
            }

            const descriptor_set_layout_binding_builder &shader_stages(VkShaderStageFlags flags) const {
                _binding.stageFlags = flags;
                return *this;
            }

            const descriptor_set_layout_binding_builder &immutable_sampler(const vkz::sampler &sampler) const {
                immutable_sampler_storage.push_back({sampler.handle});
                _binding.pImmutableSamplers = immutable_sampler_storage.back().data();
                return *this;
            }

            const descriptor_set_layout_binding_builder &immutable_samplers(std::span<const vkz::sampler> samplers) const {
                auto &handles = immutable_sampler_storage.emplace_back();
                handles.reserve(samplers.size());
                std::ranges::transform(samplers, std::back_inserter(handles),
                                       [](const auto &sampler) { return sampler.handle; });
                _binding.pImmutableSamplers = handles.data();
                return *this;
            }

            [[nodiscard]]
            descriptor_set_layout create_layout(VkDescriptorSetLayoutCreateFlags flags = 0) const {
                assert_binding();
                bindings.push_back(_binding);

                std::vector<VkDescriptorBindingFlags> bindless_flags(bindings.size());
                VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info{
                        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
                void *next = nullptr;
                if (bindless_enabled) {
                    flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                    std::generate(bindless_flags.begin(), bindless_flags.end(), [] {
                        return VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
                    });
                    extended_info.bindingCount = VKZ_COUNT(bindless_flags);
                    extended_info.pBindingFlags = bindless_flags.data();
                    next = &extended_info;
                }

                VkDescriptorSetLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
                create_info.pNext = next;
                create_info.flags = flags;
                create_info.bindingCount = VKZ_COUNT(bindings);
                create_info.pBindings = bindings.data();

                VkDescriptorSetLayout set_layout;
                VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device.logical, &create_info, nullptr, &set_layout));
                if (!_name.empty()) {
                    set_name<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(device, _name, set_layout);
                }
                return { .handle = set_layout, .device = device };
            }

            std::vector<VkDescriptorSetLayoutBinding> build() const {
                assert_binding();
                bindings.push_back(_binding);
                return bindings;
            }

            void assert_binding() const {
                assert(_binding.binding >= 0 && _binding.descriptorCount >= 1);
            }

        private:
            mutable VkDescriptorSetLayoutBinding _binding{};
            std::vector<VkDescriptorSetLayoutBinding> &bindings;
            std::vector<std::vector<VkSampler>> &immutable_sampler_storage;
            mutable std::string _name;
            bool bindless_enabled;
            const vkz::device &device;
        };

        descriptor_set_layout_builder &name(const std::string &name) {
            _name = name;
            return *this;
        }

        descriptor_set_layout_builder &bindless() {
            bindless_enabled = true;
            return *this;
        }

        descriptor_set_layout_binding_builder binding(uint32_t value) const {
            return descriptor_set_layout_binding_builder{
                    device, bindings, immutable_sampler_storage, _name, bindless_enabled, value};
        }

    private:
        mutable std::vector<VkDescriptorSetLayoutBinding> bindings;
        mutable std::vector<std::vector<VkSampler>> immutable_sampler_storage;
        mutable std::string _name;
        mutable bool bindless_enabled{};
        mutable vkz::device device;
    };
}
