#include "mesh_viewer.hpp"

int main(int argc, char **argv) {
    return reference_test::run(argc, argv, {VKZ_GLASS_VASE_FLOWERS_MESH, "GlassVaseFlowers", true, VKZ_GLASS_VASE_ENVIRONMENT});
}
