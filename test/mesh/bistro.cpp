#include "mesh_viewer.hpp"

int main(int argc, char **argv) {
    vkz::path mesh = VKZ_BISTRO_MESH;
    const char *name = "Bistro exterior";
    bool interior = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--interior") {
            mesh = VKZ_BISTRO_INTERIOR_MESH;
            name = "Bistro interior";
            interior = true;
        } else if (std::string_view(argv[i]) == "--mesh") {
            if (++i == argc) {
                std::cerr << "--mesh requires a .vkmesh path\n";
                return 1;
            }
            mesh = std::filesystem::u8path(argv[i]);
            name = "Bistro";
        }
    }
    reference_test::scene_options options{mesh, name, false, VKZ_BISTRO_ENVIRONMENT};
    options.eye = interior ? glm::vec3{6, 2, -3} : glm::vec3{0, 2, 8};
    options.target = interior ? glm::vec3{6, 2, -10} : glm::vec3{0, 2, -8};
    return reference_test::run(argc, argv, options);
}
