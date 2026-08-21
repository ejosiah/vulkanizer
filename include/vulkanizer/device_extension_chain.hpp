#pragma once

#include "types.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace vkz {
    class device_extension_chain {
    public:
        device_extension_chain() = default;

        device_extension_chain(const device_extension_chain& other)
            : nodes_{other.nodes_} {
            relink();
        }

        device_extension_chain& operator=(const device_extension_chain& other) {
            if (this != &other) {
                nodes_ = other.nodes_;
                relink();
            }
            return *this;
        }

        device_extension_chain(device_extension_chain&&) noexcept = default;
        device_extension_chain& operator=(device_extension_chain&&) noexcept = default;

        template <vulkan_structure T>
        device_extension_chain& add(const T& extension) {
            static_assert(std::is_trivially_copyable_v<T>);
            assert(extension.pNext == nullptr);
            nodes_.emplace(nodes_.begin(), extension);
            relink();
            return *this;
        }

        device_extension_chain& add(const device_extension_chain& extensions) {
            if (this == &extensions) {
                const device_extension_chain copy{extensions};
                return add(copy);
            }
            nodes_.insert(nodes_.end(), extensions.nodes_.begin(), extensions.nodes_.end());
            relink();
            return *this;
        }

        template <typename Visitor>
        void visit(Visitor&& visitor) {
            for (auto& node : nodes_) visitor(node.get());
        }

        template <typename Visitor>
        void visit(Visitor&& visitor) const {
            for (const auto& node : nodes_) visitor(node.get());
        }

        [[nodiscard]] void* head() {
            return nodes_.empty() ? nullptr : nodes_.front().get();
        }

        [[nodiscard]] const void* head() const {
            return nodes_.empty() ? nullptr : nodes_.front().get();
        }

        [[nodiscard]] bool empty() const { return nodes_.empty(); }

    private:
        class stored_node {
        public:
            template <typename T>
            explicit stored_node(const T& value)
                : data_{::operator new(sizeof(T))}, size_{sizeof(T)} {
                std::memcpy(data_, &value, size_);
                get()->pNext = nullptr;
            }

            stored_node(const stored_node& other)
                : data_{::operator new(other.size_)}, size_{other.size_} {
                std::memcpy(data_, other.data_, size_);
                get()->pNext = nullptr;
            }

            stored_node& operator=(const stored_node& other) {
                if (this != &other) {
                    stored_node copy{other};
                    swap(copy);
                }
                return *this;
            }

            stored_node(stored_node&& other) noexcept
                : data_{std::exchange(other.data_, nullptr)},
                  size_{std::exchange(other.size_, 0)} {}

            stored_node& operator=(stored_node&& other) noexcept {
                if (this != &other) {
                    ::operator delete(data_);
                    data_ = std::exchange(other.data_, nullptr);
                    size_ = std::exchange(other.size_, 0);
                }
                return *this;
            }

            ~stored_node() { ::operator delete(data_); }

            [[nodiscard]] VkBaseOutStructure* get() {
                return static_cast<VkBaseOutStructure*>(data_);
            }

            [[nodiscard]] const VkBaseOutStructure* get() const {
                return static_cast<const VkBaseOutStructure*>(data_);
            }

        private:
            void swap(stored_node& other) noexcept {
                std::swap(data_, other.data_);
                std::swap(size_, other.size_);
            }

            void* data_{};
            std::size_t size_{};
        };

        void relink() {
            for (std::size_t i = 0; i < nodes_.size(); ++i) {
                nodes_[i].get()->pNext = i + 1 < nodes_.size() ? nodes_[i + 1].get() : nullptr;
            }
        }

        std::vector<stored_node> nodes_;
    };
}
