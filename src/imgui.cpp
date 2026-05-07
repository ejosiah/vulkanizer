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

            void newFrame();

            void render(VkCommandBuffer commandBuffer);

            void destroy();

        private:
            params params_{};
            ImGuiContext* context_{};
            bool destroyed_{true};
        };

        std::unique_ptr<impl> instance;

        void checkVkResult(VkResult result) {
            VKZ_CHECK_VULKAN(result);
        }

        void validate(const params& params) {
            if (!params.window) {
                VKZ_THROW("vkz::imgui requires a GLFW window")
            }

            if (!params.vulkanContext) {
                VKZ_THROW("vkz::imgui requires a Vulkan context")
            }

            if (!params.vulkanContext->instance || !params.vulkanContext->device.physical || !params.vulkanContext->device.logical) {
                VKZ_THROW("vkz::imgui requires a fully initialized Vulkan context")
            }

            if (!params.queue) {
                VKZ_THROW("vkz::imgui requires a Vulkan queue")
            }

            if (params.minImageCount < 2 || params.imageCount < params.minImageCount) {
                VKZ_THROW("vkz::imgui requires imageCount >= minImageCount >= 2")
            }

            if (params.useDynamicRendering && params.colorAttachmentFormat == VK_FORMAT_UNDEFINED) {
                VKZ_THROW("vkz::imgui dynamic rendering requires a color attachment format")
            }

            if (!params.useDynamicRendering && !params.renderPass) {
                VKZ_THROW("vkz::imgui render pass mode requires a render pass")
            }

            if (!params.descriptorPool && params.descriptorPoolSize < IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE) {
                VKZ_THROW("vkz::imgui descriptorPoolSize is too small")
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
        if (params_.enableKeyboardNavigation) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        }
        if (params_.enableGamepadNavigation) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(params_.window, params_.installCallbacks)) {
            ImGui::DestroyContext(context_);
            context_ = nullptr;
            VKZ_THROW("Failed to initialize ImGui GLFW backend")
        }

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = params_.apiVersion;
        initInfo.Instance = params_.vulkanContext->instance;
        initInfo.PhysicalDevice = params_.vulkanContext->device.physical;
        initInfo.Device = params_.vulkanContext->device.logical;
        initInfo.QueueFamily = params_.queueFamily;
        initInfo.Queue = params_.queue;
        initInfo.DescriptorPool = params_.descriptorPool;
        initInfo.DescriptorPoolSize = params_.descriptorPool ? 0 : params_.descriptorPoolSize;
        initInfo.MinImageCount = params_.minImageCount;
        initInfo.ImageCount = params_.imageCount;
        initInfo.PipelineCache = params_.pipelineCache;
        initInfo.PipelineInfoMain.RenderPass = params_.renderPass;
        initInfo.PipelineInfoMain.MSAASamples = params_.samples;
        initInfo.UseDynamicRendering = params_.useDynamicRendering;
        initInfo.Allocator = params_.allocator;
        initInfo.CheckVkResultFn = checkVkResult;
        initInfo.MinAllocationSize = 1024 * 1024;

#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        if (params_.useDynamicRendering) {
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
                    &params_.colorAttachmentFormat;
        }
#endif

        if (!ImGui_ImplVulkan_Init(&initInfo)) {
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

    void impl::newFrame() {
        ImGui::SetCurrentContext(context_);
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void impl::render(VkCommandBuffer commandBuffer) {
        ImGui::SetCurrentContext(context_);
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
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

    void newFrame() {
        if (!instance) {
            VKZ_THROW("vkz::imgui has not been initialized")
        }

        instance->newFrame();
    }

    void setMinImageCount(uint32_t minImageCount) {
        if (!instance) {
            VKZ_THROW("vkz::imgui has not been initialized")
        }

        ImGui_ImplVulkan_SetMinImageCount(minImageCount);
    }

    void render(VkCommandBuffer commandBuffer) {
        if (!instance) {
            VKZ_THROW("vkz::imgui has not been initialized")
        }

        instance->render(commandBuffer);
    }

    void destroy() {
        instance.reset();
    }
}
