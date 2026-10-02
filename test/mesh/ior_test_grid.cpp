#include "mesh_viewer.hpp"

int main(int argc, char **argv) {
    return reference_test::run(argc, argv, {VKZ_IOR_TEST_GRID_MESH, "IORTestGrid", true, VKZ_IOR_ENVIRONMENT});
}
