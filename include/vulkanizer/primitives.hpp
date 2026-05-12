#pragma once

#include "types.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <functional>

namespace vkz::prim {

    static const glm::vec4 GRAY{0.6, 0.6,0.6, 1};
    constexpr uint RESTART_PRIMITIVE = UINT32_MAX;

    enum class topology {
        POINTS,
        LINES,
        LINE_STRIPS,
        TRIANGLES,
        TRIANGLE_FAN,
        TRIANGLE_STRIPS,
        LISTS_WITH_ADJACENCY,
        LINE_STRIPS_WITH_ADJACENCY,
        TRIANGLES_WITH_ADJACENCY,
        TRIANGLE_STRIP_WITH_ADJACENCY,
        PATCHES
    };

    struct vertex {
        glm::vec4 color;
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv;
    };

    struct primitive {
        std::vector<vertex> vertices;
        std::vector<uint> indices;
        topology topology;
    };

     /**
     * Generates vertices for cube
     * @param color cube color
     * @return vertices defining a cube
     */
    [[maybe_unused]]
    primitive cube(const glm::vec4& color = GRAY);

    [[maybe_unused]]
    primitive teapot(int resolution = 12, glm::mat4 xform = glm::mat4{1}, glm::mat4 lidXform = glm::mat4{1}, const glm::vec4& color = GRAY);

    /**
     * Generates mesh for a sphere
     * @param rows number of rows on the sphere
     * @param columns number of columns on the sphere
     * @param radius radius of sphere
     * @param color color of sphere
     * @return vertices defining a sphere
     */
    [[maybe_unused]]
    primitive sphere(int rows, int columns, float radius = 1.0f, const glm::mat4 &xform = glm::mat4{1}, const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);

    /**
     *
     * Generates mesh for a hemisphere
     * @param rows number of rows on the hemisphere
     * @param columns number of columns on the hemisphere
     * @param radius radius of hemisphere
     * @param color color of hemisphere
     * @return vertices defining a hemisphere
     */
    [[maybe_unused]]
    primitive hemisphere(int rows, int columns, float radius = 1.0f, const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);

    /**
     * Generates mesh for a cone
     * @param rows number of rows on the cone
     * @param columns number of columns on the cone
     * @param radius radius of the cone
     * @param height height of the cone
     * @param color color of the cone
     * @return vertices defining a cone
     */
    [[maybe_unused]]
    primitive cone(int rows, int columns, float radius = 1.0f, float height = 1.0f, const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);

    /**
     * @brief Generates mesh for a cylinder
     * @param rows number of rows on the cylinder
     * @param columns number of columns on the cylinder
     * @param radius radius of the cylinder
     * @param height height of the cylinder
     * @param color color of the cylinder
     * @return vertices defining a cylinder
     */
    [[maybe_unused]]
    primitive cylinder(int rows, int columns, float radius = 1.0f, float height = 1.0f,  const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);

    [[maybe_unused]]
    primitive plane(int rows, int columns, float width, float height, const glm::mat4& xform = glm::mat4(1), const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);

    /**
     * @brief Generates mesh for a torus
     * @param rows number of rows on the torus
     * @param columns number of columns on the torus
     * @param innerRadius inner radius of the torus
     * @param outerRadius outer radius of the torus
     * @param color color of the torus
     * @return vertices defining a torus
     */
    [[maybe_unused]]
    primitive torus(int rows, int columns, float innerRadius = 0.5f, float outerRadius = 1.0f, const glm::mat4 &xform = glm::mat4{1}, const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);


    /**
     *
     * @param rows number of rows on the torus
     * @param columns number of columns on the torus
     * @param xform primitive transformation
     * @param color
     * @param topology
     * @param func
     * @return
     */
    template<typename implicit_func>
    primitive implicit(int rows, int columns, float width, float height, implicit_func func, const glm::mat4 &xform = glm::mat4{1}, const glm::vec4& color = GRAY, topology topology = topology::TRIANGLE_STRIPS);

    /**
     * Generates a parametric surface
     * @tparam SurfaceFunction giving an i,j pair generates a position and normal vector i.e f(i, j) -> std::tuple<glm::vec3, glm::vec3>
     * @param p horizontal sample points
     * @param q vertical sample points
     * @param f giving an i,j pair generates a position and normal vector i.e f(i, j) -> std::tuple<glm::vec3, glm::vec3>
     * @param color surface color
     * @return  returns a surface defined by the parametric function f
     */
    template<typename SurfaceFunction>
    primitive surface(int p,
                     int q,
                     SurfaceFunction&& f,
                     const glm::vec4& color,
                     const glm::mat4& xform = glm::mat4(1),
                     topology topology = topology::TRIANGLE_STRIPS);
}
