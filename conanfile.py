from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.scm import Git


class VulkanizerConan(ConanFile):
    name = "vulkanizer"
    version = "2.2.0"
    package_type = "static-library"

    license = "MIT"
    url = "https://github.com/ejosiah/vulkanizer"
    description = "Small Vulkan helper library with GLFW, ImGui, VMA, GLM, and Volk integration."
    topics = ("vulkan", "imgui", "glfw")

    settings = "os", "compiler", "build_type", "arch"
    source_url = "https://github.com/ejosiah/vulkanizer.git"
    source_tag = f"v{version}"

    def requirements(self):
        self.requires("volk/1.3.296.0")
        self.requires("glfw/3.4")
        self.requires("imgui/1.92.7")
        self.requires("glm/1.0.1")
        self.requires("vulkan-memory-allocator/3.3.0")
        self.requires("glslang/11.7.0")

    def source(self):
        git = Git(self)
        git.clone(url=self.source_url, target=".")
        git.checkout(self.source_tag)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        toolchain = CMakeToolchain(self)
        toolchain.variables["VULKANIZER_BUILD_TESTS"] = False
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "vulkanizer")
        self.cpp_info.set_property("cmake_target_name", "vulkanizer::vulkanizer")
        self.cpp_info.libs = ["vulkanizer"]
