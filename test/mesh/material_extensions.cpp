#include "mesh_viewer.hpp"

int main(int argc, char **argv) {
    std::string name = "TextureTransformTest";
    vkz::path mesh;
    bool orbit = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--fps-camera") orbit = false;
        if (std::string_view(argv[i]) == "--mesh") {
            if (++i == argc) {
                std::cerr << "--mesh requires a .vkmesh path\n";
                return 1;
            }
            mesh = std::filesystem::u8path(argv[i]);
            name = mesh.stem().string();
        } else if (std::string_view(argv[i]) == "--scene") {
            if (++i == argc) {
                std::cerr << "--scene requires a sample model name\n";
                return 1;
            }
            name = argv[i];
        }
    }
    if (mesh.empty()) mesh = vkz::path(VKZ_EXTENSION_SCENES) / name / (name + ".vkmesh");
    return reference_test::run(argc, argv, {mesh, name.c_str(), orbit, VKZ_EXTENSION_ENVIRONMENT});
}
