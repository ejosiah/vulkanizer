#include "mesh_viewer.hpp"

int main(int argc, char **argv) {
    return reference_test::run(argc, argv, {VKZ_MULTI_UV_MESH, "MultiUVTest", true, VKZ_MULTI_UV_ENVIRONMENT});
}
