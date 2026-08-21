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

#include <iostream>

namespace vkz {

class application {
public:
    explicit application(vulkan_app_create_info create_info);

    explicit application(const std::string& name, glm::uvec2 screen_size,
                         const device_extension_chain& extension_chain = {});

    ~application();

    virtual void setup() {}

    void main_loop();

    std::span<const VkCommandBuffer> allocate_command_buffers(std::size_t count);

    void enqueue(VkCommandBuffer command_buffer);

    void enqueue(std::span<const VkCommandBuffer> command_buffers);

    virtual void app_logic(VkCommandBuffer command_buffer) = 0;

    virtual void render_swapchain(VkCommandBuffer command_buffer);

    virtual void new_frame() {}

    virtual void end_frame() {}

private:
    void recreate_swapchain();

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
    render_info render_info_;
};

}
