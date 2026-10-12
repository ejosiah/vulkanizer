#include <vulkanizer/barrier.hpp>
#include <vulkanizer/builders.hpp>
#include <vulkanizer/camera/controller.hpp>
#include <vulkanizer/commands.hpp>
#include <vulkanizer/glfw_input_adaptor.hpp>
#include <vulkanizer/io.hpp>
#include <vulkanizer/imgui.hpp>
#include <vulkanizer/log.hpp>
#include <vulkanizer/render.hpp>
#include <vulkanizer/status.hpp>
#include <vulkanizer/vulkan_app.hpp>
#include <imgui.h>

#include <cstddef>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace vkz {

class application {
public:
    explicit application(vulkan_app_create_info create_info, uint32_t frames_in_flight = 1);

    explicit application(const std::string& name, glm::uvec2 screen_size,
                         const device_extension_chain& extension_chain = {}, uint32_t frames_in_flight = 1);

    ~application();

    virtual void setup() {}

    void main_loop();

    std::span<const VkCommandBuffer> allocate_command_buffers(std::size_t count);

    void enqueue(VkCommandBuffer command_buffer);

    void enqueue(std::span<const VkCommandBuffer> command_buffers);

    virtual void app_logic(VkCommandBuffer command_buffer) = 0;

    virtual void render_swapchain(VkCommandBuffer command_buffer);

    virtual void new_frame() {}

    // Called immediately after queue submission, before presentation can block.
    virtual void frame_submitted() {}

    virtual void end_frame() {}

    VkFormat color_format() const;

    VkFormat depth_format() const;

    [[nodiscard]] uint32_t frames_in_flight() const { return frames_in_flight_; }
    // Available in new_frame/app_logic/end_frame after this slot's fence has completed.
    [[nodiscard]] uint32_t frame_index() const { return frame_index_; }

private:
    void recreate_swapchain();
    void create_frame_attachments();
    void destroy_frame_attachments();
    uint32_t frames_in_flight_{1};
    uint32_t frame_index_{};
    bool depth_testing_{};
    std::vector<VkSemaphore> image_available_;
    std::vector<VkSemaphore> render_finished_;
    std::vector<texture> depth_buffers_;
    std::vector<bool> depth_initialized_;

protected:
    std::string name_;
    glm::uvec2 screen_size_;
    vulkan_app app_;
    context& context_;
    device& device_;
    vma_memory_allocator allocator_;
    std::unique_ptr<swapchain> swapchain_;
    std::vector<image_view> image_views_;
    std::unique_ptr<fenced_command_pools> commands_pool_;
    // Non-owning handles for the current frame slot; application manages their lifetime.
    std::optional<texture> depth_buffer_;
    render_info render_info_;
};

}
