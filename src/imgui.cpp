#include "vulkanizer/imgui.hpp"

#include "vulkanizer/status.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <memory>
#include <utility>

namespace vkz::imgui {
    namespace {
        class impl {
        public:
            explicit impl(const params& params);

            ~impl();

            impl(const impl&) = delete;

            impl& operator=(const impl&) = delete;

            impl(impl&& other) noexcept;

            impl& operator=(impl&& other) noexcept;

            void new_frame();

            void render(VkCommandBuffer command_buffer);

            void destroy();

        private:
            params params_{};
            ImGuiContext* context_{};
            bool destroyed_{true};
        };

        std::unique_ptr<impl> instance;

        void check_vk_result(VkResult result) {
            VKZ_CHECK_VULKAN(result);
        }

        void validate(const params& params) {
            if (!params.window) {
                VKZ_THROW("vkz::imgui requires a GLFW window")
            }

            if (!params.vulkan_context) {
                VKZ_THROW("vkz::imgui requires a Vulkan context")
            }

            if (!params.vulkan_context->instance || !params.vulkan_context->device.physical || !params.vulkan_context->device.logical) {
                VKZ_THROW("vkz::imgui requires a fully initialized Vulkan context")
            }

            if (!params.queue) {
                VKZ_THROW("vkz::imgui requires a Vulkan queue")
            }

            if (params.min_image_count < 2 || params.image_count < params.min_image_count) {
                VKZ_THROW("vkz::imgui requires image_count >= min_image_count >= 2")
            }

            if (params.use_dynamic_rendering && params.color_attachment_format == VK_FORMAT_UNDEFINED) {
                VKZ_THROW("vkz::imgui dynamic rendering requires a color attachment format")
            }

            if (!params.use_dynamic_rendering && !params.render_pass) {
                VKZ_THROW("vkz::imgui render pass mode requires a render pass")
            }

            if (!params.descriptor_pool && params.descriptor_pool_size < IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE) {
                VKZ_THROW("vkz::imgui descriptor_pool_size is too small")
            }
        }
    }

    namespace {
    impl::impl(const params& params)
        : params_{params} {
        validate(params_);

        IMGUI_CHECKVERSION();
        context_ = ImGui::CreateContext();
        ImGui::SetCurrentContext(context_);

        auto& io = ImGui::GetIO();
        if (params_.enable_keyboard_navigation) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        }
        if (params_.enable_gamepad_navigation) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(params_.window, params_.install_callbacks)) {
            ImGui::DestroyContext(context_);
            context_ = nullptr;
            VKZ_THROW("Failed to initialize ImGui GLFW backend")
        }

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = params_.api_version;
        init_info.Instance = params_.vulkan_context->instance;
        init_info.PhysicalDevice = params_.vulkan_context->device.physical;
        init_info.Device = params_.vulkan_context->device.logical;
        init_info.QueueFamily = params_.queue_family;
        init_info.Queue = params_.queue;
        init_info.DescriptorPool = params_.descriptor_pool;
        init_info.DescriptorPoolSize = params_.descriptor_pool ? 0 : params_.descriptor_pool_size;
        init_info.MinImageCount = params_.min_image_count;
        init_info.ImageCount = params_.image_count;
        init_info.PipelineCache = params_.pipeline_cache;
        init_info.PipelineInfoMain.RenderPass = params_.render_pass;
        init_info.PipelineInfoMain.MSAASamples = params_.samples;
        init_info.UseDynamicRendering = params_.use_dynamic_rendering;
        init_info.Allocator = params_.allocator;
        init_info.CheckVkResultFn = check_vk_result;
        init_info.MinAllocationSize = 1024 * 1024;

#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        if (params_.use_dynamic_rendering) {
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
            init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
                    &params_.color_attachment_format;
        }
#endif

        if (!ImGui_ImplVulkan_Init(&init_info)) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext(context_);
            context_ = nullptr;
            VKZ_THROW("Failed to initialize ImGui Vulkan backend")
        }

        destroyed_ = false;
    }

    impl::~impl() {
        destroy();
    }

    impl::impl(impl&& other) noexcept
        : params_{other.params_}
        , context_{std::exchange(other.context_, nullptr)}
        , destroyed_{std::exchange(other.destroyed_, true)} {
    }

    impl& impl::operator=(impl&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        destroy();
        params_ = other.params_;
        context_ = std::exchange(other.context_, nullptr);
        destroyed_ = std::exchange(other.destroyed_, true);
        return *this;
    }

    void impl::new_frame() {
        ImGui::SetCurrentContext(context_);
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void impl::render(VkCommandBuffer command_buffer) {
        ImGui::SetCurrentContext(context_);
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
    }

    void impl::destroy() {
        if (destroyed_) {
            return;
        }

        ImGui::SetCurrentContext(context_);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(context_);
        context_ = nullptr;
        destroyed_ = true;
    }
    }

    void init(const params& params) {
        instance = std::make_unique<impl>(params);
    }

    void new_frame() {
        if (!instance) {
            VKZ_THROW("vkz::imgui has not been initialized")
        }

        instance->new_frame();
    }

    void render(VkCommandBuffer command_buffer) {
        if (!instance) {
            VKZ_THROW("vkz::imgui has not been initialized")
        }

        instance->render(command_buffer);
    }

    void destroy() {
        instance.reset();
    }
}
