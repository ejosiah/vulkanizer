#pragma once

#include "vkz.hpp"

#include <vector>
#include <algorithm>

namespace vkz {

    class DescriptorSetLayoutBuilder {
    public:
        explicit DescriptorSetLayoutBuilder(Device device) : device(device) {}

        class DescriptorSetLayoutBindingBuilder {
        public:
            explicit DescriptorSetLayoutBindingBuilder(
                    const Device &device,
                    std::vector<VkDescriptorSetLayoutBinding> &bindings,
                    std::string name,
                    bool bindlessEnabled,
                    uint32_t bindingValue
            )
                    : device(device), bindings(bindings), _name{name}, bindlessEnabled(bindlessEnabled) {
                _binding.binding = bindingValue;
            };

            DescriptorSetLayoutBindingBuilder binding(uint32_t value) const {
                assertBinding();
                bindings.push_back(_binding);
                return DescriptorSetLayoutBindingBuilder{device, bindings, _name, bindlessEnabled, value};
            }

            const DescriptorSetLayoutBindingBuilder &descriptorCount(uint32_t count) const {
                _binding.descriptorCount = count;
                return *this;
            }

            const DescriptorSetLayoutBindingBuilder &descriptorType(VkDescriptorType type) const {
                _binding.descriptorType = type;
                return *this;
            }

            const DescriptorSetLayoutBindingBuilder &shaderStages(VkShaderStageFlags flags) const {
                _binding.stageFlags = flags;
                return *this;
            }

            const DescriptorSetLayoutBindingBuilder &immutableSampler(const VkSampler &sampler) const {
                _binding.pImmutableSamplers = &sampler;
                return *this;
            }

            const DescriptorSetLayoutBindingBuilder &immutableSamplers(VkSampler *samplers) const {
                _binding.pImmutableSamplers = samplers;
                return *this;
            }

            [[nodiscard]]
            VkDescriptorSetLayout createLayout(VkDescriptorSetLayoutCreateFlags flags = 0) const {
                assertBinding();
                bindings.push_back(_binding);

                std::vector<VkDescriptorBindingFlags> bindlessFlags(bindings.size());
                VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
                        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
                void *next = nullptr;
                if (bindlessEnabled) {
                    flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                    std::generate(bindlessFlags.begin(), bindlessFlags.end(), [] {
                        return VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
                    });
                    extendedInfo.bindingCount = VKZ_COUNT(bindlessFlags);
                    extendedInfo.pBindingFlags = bindlessFlags.data();
                    next = &extendedInfo;
                }

                VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
                createInfo.pNext = next;
                createInfo.flags = flags;
                createInfo.bindingCount = VKZ_COUNT(bindings);
                createInfo.pBindings = bindings.data();

                VkDescriptorSetLayout setLayout;
                VKZ_CHECK_VULKAN(vkCreateDescriptorSetLayout(device.logical, &createInfo, nullptr, &setLayout));
                if (!_name.empty()) {
                    setName<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>(device, _name, setLayout);
                }
                return setLayout;
            }

            std::vector<VkDescriptorSetLayoutBinding> build() const {
                assertBinding();
                bindings.push_back(_binding);
                return bindings;
            }

            void assertBinding() const {
                assert(_binding.binding >= 0 && _binding.descriptorCount >= 1);
            }

        private:
            mutable VkDescriptorSetLayoutBinding _binding{};
            std::vector<VkDescriptorSetLayoutBinding> &bindings;
            mutable std::string _name;
            bool bindlessEnabled;
            const Device &device;
        };

        DescriptorSetLayoutBuilder &name(const std::string &name) {
            _name = name;
            return *this;
        }

        DescriptorSetLayoutBuilder &bindless() {
            bindlessEnabled = true;
            return *this;
        }

        DescriptorSetLayoutBindingBuilder binding(uint32_t value) const {
            return DescriptorSetLayoutBindingBuilder{device, bindings, _name, bindlessEnabled, value};
        }

    private:
        mutable std::vector<VkDescriptorSetLayoutBinding> bindings;
        mutable std::string _name;
        mutable bool bindlessEnabled{};
        mutable Device device;
    };
}