#include "vulkanizer/primitives.hpp"
#include "vulkanizer/detail/teapot.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>

namespace vkz::prim {
    static constexpr auto PI = glm::pi<float>();
    
    primitive cube(const glm::vec4 &color) {
        primitive primitive{};

        primitive.vertices = {

                // FRONT FACE
                {color, {-0.5, -0.5, 0.5},  {0.0f,  0.0f,  1.0f},  {1,  0, 0},  {0, 1, 0},  {0, 0}},
                {color, {0.5,  -0.5, 0.5},  {0.0f,  0.0f,  1.0f},  {1,  0, 0},  {0, 1, 0},  {1, 0}},
                {color, {0.5,  0.5,  0.5},  {0.0f,  0.0f,  1.0f},  {1,  0, 0},  {0, 1, 0},  {1, 1}},
                {color, {-0.5, 0.5,  0.5},  {0.0f,  0.0f,  1.0f},  {1,  0, 0},  {0, 1, 0},  {0, 1}},

                // RIGHT FACE
                {color, {0.5,  -0.5, 0.5},  {1.0f,  0.0f,  0.0f},  {0,  0, -1}, {0, 1, 0},  {0, 0}},
                {color, {0.5,  -0.5, -0.5}, {1.0f,  0.0f,  0.0f},  {0,  0, -1}, {0, 1, 0},  {1, 0}},
                {color, {0.5,  0.5,  -0.5}, {1.0f,  0.0f,  0.0f},  {0,  0, -1}, {0, 1, 0},  {1, 1}},
                {color, {0.5,  0.5,  0.5},  {1.0f,  0.0f,  0.0f},  {0,  0, -1}, {0, 1, 0},  {0, 1}},

                // BACK FAC1
                {color, {-0.5, -0.5, -0.5}, {0.0f,  0.0f,  -1.0f}, {-1, 0, 0},  {0, 1, 0},  {1, 0}},
                {color, {-0.5, 0.5,  -0.5}, {0.0f,  0.0f,  -1.0f}, {-1, 0, 0},  {0, 1, 0},  {1, 1}},
                {color, {0.5,  0.5,  -0.5}, {0.0f,  0.0f,  -1.0f}, {-1, 0, 0},  {0, 1, 0},  {0, 1}},
                {color, {0.5,  -0.5, -0.5}, {0.0f,  0.0f,  -1.0f}, {-1, 0, 0},  {0, 1, 0},  {0, 0}},

                // Left face
                {color, {-0.5, -0.5, 0.5},  {-1.0f, 0.0f,  0.0f},  {0,  0, 1},  {0, 1, 0},  {1, 0}},
                {color, {-0.5, 0.5,  0.5},  {-1.0f, 0.0f,  0.0f},  {0,  0, 1},  {0, 1, 0},  {1, 1}},
                {color, {-0.5, 0.5,  -0.5}, {-1.0f, 0.0f,  0.0f},  {0,  0, 1},  {0, 1, 0},  {0, 1}},
                {color, {-0.5, -0.5, -0.5}, {-1.0f, 0.0f,  0.0f},  {0,  0, 1},  {0, 1, 0},  {0, 0}},

                // bottom face
                {color, {-0.5, -0.5, 0.5},  {0.0f,  -1.0f, 0.0f},  {1,  0, 0},  {0, 0, 1},  {0, 1}},
                {color, {-0.5, -0.5, -0.5}, {0.0f,  -1.0f, 0.0f},  {1,  0, 0},  {0, 0, 1},  {0, 0}},
                {color, {0.5,  -0.5, -0.5}, {0.0f,  -1.0f, 0.0f},  {1,  0, 0},  {0, 0, 1},  {1, 0}},
                {color, {0.5,  -0.5, 0.5},  {0.0f,  -1.0f, 0.0f},  {1,  0, 0},  {0, 0, 1},  {1, 1}},

                // top face
                {color, {-0.5, 0.5,  0.5},  {0.0f,  1.0f,  0.0f},  {1,  0, 0},  {0, 0, -1}, {0, 0}},
                {color, {0.5,  0.5,  0.5},  {0.0f,  1.0f,  0.0f},  {1,  0, 0},  {0, 0, -1}, {1, 0}},
                {color, {0.5,  0.5,  -0.5}, {0.0f,  1.0f,  0.0f},  {1,  0, 0},  {0, 0, -1}, {1, 1}},
                {color, {-0.5, 0.5,  -0.5}, {0.0f,  1.0f,  0.0f},  {1,  0, 0},  {0, 0, -1}, {0, 1}},
        };

        primitive.indices = {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7,
                8, 9, 10, 8, 10, 11,
                12, 13, 14, 12, 14, 15,
                16, 17, 18, 16, 18, 19,
                20, 21, 22, 20, 22, 23
        };
        return primitive;
    }

    primitive teapot(int resolution, glm::mat4 xform, glm::mat4 lidXform, const glm::vec4 &color) {
        const int grid = std::max(1, resolution);
        const int num_vertices = 32 * (grid + 1) * (grid + 1);
        const int num_indices = grid * grid * 32;

        auto positions = std::vector<glm::vec3>(num_vertices);
        auto normals = std::vector<glm::vec3>(num_vertices);
        auto uvs = std::vector<glm::vec2>(num_vertices);
        auto indices = std::vector<uint32_t>(num_indices * 6);

        teapot::generatePatches(
            reinterpret_cast<float*>(positions.data()),
            reinterpret_cast<float*>(normals.data()),
            reinterpret_cast<float*>(uvs.data()),
            indices.data(),
            grid);

        const auto rotMat = glm::mat3(glm::rotate(glm::mat4{1}, glm::radians(-90.0f), glm::vec3{1, 0, 0}));
        for (int i = 0; i < num_vertices; ++i) {
            positions[i] = rotMat * positions[i];
            normals[i] = glm::inverseTranspose(rotMat) * normals[i];
        }

        teapot::moveLid(grid, reinterpret_cast<float*>(positions.data()), lidXform);

        const auto nXform = glm::inverseTranspose(glm::mat3{xform});
        for(auto i = 0; i < num_vertices; ++i) {
            positions[i] = (xform * glm::vec4(positions[i], 1)).xyz();
            normals[i] = glm::normalize(nXform * normals[i]);
        }

        primitive rtVal{};
        rtVal.topology = topology::TRIANGLES;
        for(auto i = 0; i < num_vertices; ++i) {
            rtVal.vertices.emplace_back(color, positions[i], normals[i], glm::vec3{1, 0, 0}, glm::vec3{0, 0, 1}, uvs[i]);
        }
        rtVal.indices = indices;

        return rtVal;
    }

    primitive sphere(int rows, int columns, float radius, const glm::mat4 &xform, const glm::vec4 &color,
                     topology topology) {
        const auto p = columns;
        const auto q = rows;
        const auto r = radius;

        auto f = [&](float i, float j) {
            const auto u = 2 * i / static_cast<float>(p) * PI;
            const auto v = j / static_cast<float>(q) * PI;

            const auto nx = std::cos(u) * std::sin(v);
            const auto x = r * nx;

            const auto ny = std::cos(v);
            const auto y = r * ny;

            const auto nz = std::sin(u) * std::sin(v);
            const auto z = r * nz;

            return std::make_tuple(glm::vec3(x, y, z), glm::vec3(nx, ny, nz));
        };

        return surface(p, q, f, color, xform, topology);
    }

    primitive hemisphere(int rows, int columns, float radius, const glm::vec4 &color, topology topology) {
        auto p = columns;
        auto q = rows;

        auto f = [&](float i, float j) {
            float u = 2 * i / static_cast<float>(p) * PI;
            float v = j / static_cast<float>(q) * PI / 2.0f;

            float nx = std::cos(u) * std::sin(v);
            float x = radius * nx;

            float ny = std::cos(v);
            float y = radius * ny;

            float nz = std::sin(u) * std::sin(v);
            float z = radius * nz;

            return std::make_tuple(glm::vec3(x, y, z), glm::vec3(nx, ny, nz));
        };

        return surface(p, q, f, color, glm::mat4{1}, topology);
    }

    primitive cone(int rows, int columns, float radius, float height, const glm::vec4 &color,
                              topology topology) {
        const auto p = columns;
        const auto q = rows;
        const auto h = height;

        auto f = [&](float i, float j) {
            float u = 2 * i / static_cast<float>(p) * PI;
            float v = j / static_cast<float>(q) * h;

            float nx = std::cos(u);
            float x = radius * v * std::cos(u);

            float ny = std::sin(u);
            float y = radius * v * std::sin(u);

            float nz = 0;
            float z = v - h * 0.5f;

            return std::make_tuple(glm::vec3(x, y, z), glm::vec3(nx, ny, nz));
        };

        return surface(p, q, f, color, glm::mat4{1}, topology);
    }

    primitive cylinder(int rows, int columns, float radius, float height, const glm::vec4 &color,
                                  topology topology) {
        const auto p = columns;
        const auto q = rows;
        const auto h = height;

        auto f = [&](float i, float j) {
            float u = (-1.f + 2.f * i / static_cast<float>(p)) * PI;
            float v = j / static_cast<float>(q) * h;
            float nx = std::sin(u);
            float x = radius * height * std::sin(u);

            float ny = 0;
            float y = v - h * 0.5f;

            float nz = std::cos(u);
            float z = radius * height * std::cos(u);

            return std::make_tuple(glm::vec3(x, y, z), glm::vec3(nx, ny, nz));
        };

        return surface(p, q, f, color, glm::mat4{1}, topology);
    }

    primitive torus(int rows, int columns, float innerRadius, float outerRadius, const glm::mat4 &xform,
                               const glm::vec4 &color, topology topology) {
        auto p = columns;
        auto q = rows;
        auto R = innerRadius;
        auto r = outerRadius;

        auto f = [&](float i, float j) {
            float u = (-1.f + 2.f * i / static_cast<float>(p)) * PI;
            float v = (-1.f + 2.f * j / static_cast<float>(q)) * PI;

            float x = (R + r * std::cos(v)) * std::cos(u);
            float nx = std::cos(v) * std::cos(u);

            float y = (R + r * std::cos(v)) * std::sin(u);
            float ny = std::cos(v) * sin(u);

            float z = r * std::sin(v);
            float nz = std::sin(v);

            return std::make_tuple(glm::vec3(x, y, z), glm::vec3(nx, ny, nz));
        };


        return surface(p, q, f, color, xform, topology);
    }


    primitive
    plane(int rows, int columns, float width, float height, const glm::mat4 &xform, const glm::vec4 &color,
                      topology topology) {

        const auto p = columns;
        const auto q = rows;
        const auto halfWidth = width * 0.5f;
        const auto halfHeight = height * 0.5f;

        auto f = [&](float i, float j) {
            float u = i / static_cast<float>(p) * width - halfWidth;
            float v = j / static_cast<float>(q) * height - halfHeight;

            float x = u;
            float nx = 0;

            float y = v;
            float ny = 0;

            float z = 0;
            float nz = 1;


            return std::make_tuple(glm::vec3(x, y, z), glm::vec3(nx, ny, nz));
        };

        return surface(p, q, f, color, xform, topology);
    }


    template<typename implicit_func>
    primitive implicit(int rows, int columns, float width, float height, implicit_func func, const glm::mat4 &xform, const glm::vec4 &color,
        topology topology) {

        const auto p = columns;
        const auto q = rows;
        const auto halfWidth = width * 0.5f;
        const auto halfHeight = height * 0.5f;
        const auto dx = width / static_cast<float>(p);
        const auto dy = height / static_cast<float>(q);

        auto f = [&](float i, float j) {
            float u = i / static_cast<float>(p) * width - halfWidth;
            float v = j / static_cast<float>(q) * height - halfHeight;

            float x = u;
            float y = v;
            float z = func(x, y);

            const auto dfdx = (func(x + dx, y) - func(x - dx, y)) / (2.0f * dx);
            const auto dfdy = (func(x, y + dy) - func(x, y - dy)) / (2.0f * dy);
            auto normal = glm::normalize(glm::vec3{-dfdx, -dfdy, 1.0f});

            return std::make_tuple(glm::vec3(x, y, z), normal);
        };

        return surface(p, q, f, color, xform, topology);
    }

    template<typename SurfaceFunction>
    primitive surface(int p, int q, SurfaceFunction &&func, const glm::vec4 &color, const glm::mat4 &xform,
                                 topology topology) {
        primitive vertices;
        vertices.topology = topology;
        auto nXform = glm::inverseTranspose(glm::mat3(xform));
        for (int j = 0; j <= q; j++) {
            for (int i = 0; i <= p; i++) {
                auto [position, normal] = func(i, j);
                vertex vertex{};
                vertex.position = xform * glm::vec4(position, 1.0);
                vertex.normal = nXform * normal;
                // TODO construct tangents
                vertex.color = color;
                vertex.uv = {static_cast<float>(p - i) / static_cast<float>(p), static_cast<float>(q - j) / static_cast<float>(q)};
                vertices.vertices.push_back(vertex);
            }
        }

        if (topology == topology::TRIANGLES) {
            for (int j = 0; j < q; j++) {
                for (int i = 0; i < p; i++) {
                    vertices.indices.push_back((j + 1) * (p + 1) + i);
                    vertices.indices.push_back(j * (p + 1) + i);
                    vertices.indices.push_back((j + 1) * (p + 1) + i + 1);

                    vertices.indices.push_back((j + 1) * (p + 1) + i + 1);
                    vertices.indices.push_back(j * (p + 1) + i);
                    vertices.indices.push_back(j * (p + 1) + i + 1);

                }
            }
        } else {
            for (int j = 0; j < q; j++) {
                for (int i = 0; i <= p; i++) {
                    vertices.indices.push_back((j + 1) * (p + 1) + i);
                    vertices.indices.push_back(j * (p + 1) + i);
                }
                vertices.indices.push_back(RESTART_PRIMITIVE);
            }
        }

        return vertices;
    }

    primitive triangleStripToTriangleList(const primitive &vertices) {
        if (vertices.topology == topology::TRIANGLES) return vertices;

        primitive new_primitive{};
        new_primitive.vertices = vertices.vertices;
        new_primitive.topology = topology::TRIANGLES;

        if (vertices.indices.empty()) {
            assert(vertices.vertices.size() > 2);
            const auto numFaces = static_cast<int>(vertices.vertices.size()) - 2;

            int i = 0;
            for (; i < numFaces; i++) {
                int v0, v1, v2;
                if (i % 2 == 0) {
                    v0 = i;
                    v1 = i + 1;
                    v2 = i + 2;
                } else {
                    v0 = i + 1;
                    v1 = i;
                    v2 = i + 2;
                }
                new_primitive.indices.push_back(v0);
                new_primitive.indices.push_back(v1);
                new_primitive.indices.push_back(v2);
            }
            return new_primitive;
        }
        assert(vertices.indices.size() > 2);
        const auto numFaces = static_cast<int>(vertices.indices.size()) - 2; // we are assuming there are N faces defined by N+2 vertices

        for (auto i = 0, restarts = 0; i < numFaces; i++) {
            int v0, v1, v2;
            if ((i + restarts) % 2 == 0) {
                v0 = i;
                v1 = i + 1;
                v2 = i + 2;
            } else {
                v0 = i + 1;
                v1 = i;
                v2 = i + 2;
            }
            auto i0 = vertices.indices[v0];
            auto i1 = vertices.indices[v1];
            auto i2 = vertices.indices[v2];

            if (i0 == RESTART_PRIMITIVE || i1 == RESTART_PRIMITIVE || i2 == RESTART_PRIMITIVE) {
                restarts++;
                continue;
            }

            new_primitive.indices.push_back(i0);
            new_primitive.indices.push_back(i1);
            new_primitive.indices.push_back(i2);
        }

        return new_primitive;
    }

    primitive calculateTangents(primitive &vertices, bool smooth /* TODO implement smooth tangents */) {
        auto &indices = vertices.indices;

        for (int i = 0; i < indices.size(); i += 3) {
            auto &v0 = vertices.vertices[indices[i]];
            auto &v1 = vertices.vertices[indices[i + 1]];
            auto &v2 = vertices.vertices[indices[i + 2]];

            auto e1 = v1.position.xyz() - v0.position.xyz();
            auto e2 = v2.position.xyz() - v0.position.xyz();

            auto du1 = v1.uv.x - v0.uv.x;
            auto dv1 = v1.uv.y - v0.uv.y;
            auto du2 = v2.uv.x - v0.uv.x;
            auto dv2 = v2.uv.y - v0.uv.y;

            auto d = 1.f / (du1 * dv2 - dv1 * du2);

            glm::vec3 tn{0};
            tn.x = d * (dv2 * e1.x - dv1 * e2.x);
            tn.y = d * (dv2 * e1.y - dv1 * e2.y);
            tn.z = d * (dv2 * e1.z - dv1 * e2.z);

            glm::vec3 bn{0};
            bn.x = d * (du1 * e2.x - du2 * e1.x);
            bn.y = d * (du1 * e2.y - du2 * e1.y);
            bn.z = d * (du1 * e2.z - du2 * e1.z);

            v0.tangent = tn;
            v1.tangent = tn;
            v2.tangent = tn;

            v0.bitangent = bn;
            v1.bitangent = bn;
            v2.bitangent = bn;
        }

        return vertices;
    }

    template primitive implicit<float (*)(float, float)>(
        int rows,
        int columns,
        float width,
        float height,
        float (*func)(float, float),
        const glm::mat4 &xform,
        const glm::vec4 &color,
        topology topology);

    std::vector<primitive> cornellBox() {
        auto white = glm::vec4{0.73, 0.71, 0.68, 1};
        auto red = glm::vec4{0.63, 0.064, 0.005, 1};
        auto green = glm::vec4{0.14, 0.45, 0.09, 1};
        auto w = 55.f;

        glm::mat4 xform = glm::translate(glm::mat4(1), {0, 0, -w * 0.5f});
        auto backWall = plane(1, 1, w, w, xform, white, topology::TRIANGLES);


        xform = glm::translate(glm::mat4(1), {0, -w * 0.5f, 0});
        xform = glm::rotate(xform, -glm::half_pi<float>(), {1, 0, 0});
        auto floor = plane(1, 1, w, w, xform, white, topology::TRIANGLES);

        xform = glm::translate(glm::mat4(1), {0, w * 0.5f, 0});
        xform = glm::rotate(xform, glm::half_pi<float>(), {1, 0, 0});
        auto ceiling = plane(1, 1, w, w, xform, white, topology::TRIANGLES);

        xform = glm::translate(glm::mat4(1), {-w * 0.5f, 0, 0});
        xform = glm::rotate(xform, glm::half_pi<float>(), {0, 1, 0});
        auto rightWall = plane(1, 1, w, w, xform, red, topology::TRIANGLES);

        xform = glm::translate(glm::mat4(1), {w * 0.5f, 0, 0});
        xform = glm::rotate(xform, -glm::half_pi<float>(), {0, 1, 0});
        auto leftWall = plane(1, 1, w, w, xform, green, topology::TRIANGLES);

        xform = glm::translate(glm::mat4(1), {0, w * 0.5f - 0.1, 0});
        xform = glm::rotate(xform, glm::half_pi<float>(), {1, 0, 0});
        auto light = plane(1, 1, 13, 10.5, xform, glm::vec4(0), topology::TRIANGLES);


        xform = glm::translate(glm::mat4(1), glm::vec3(10, (16.5 - w) * 0.5, 12));
        xform = glm::rotate(xform, glm::radians(-18.f), {0, 1, 0});
        xform = glm::scale(xform, glm::vec3(16.5));
        auto shortBox = cube(white);

        auto nxForm = glm::inverseTranspose(glm::mat3(xform));
        for (auto &vertex: shortBox.vertices) {
            vertex.position = (xform * glm::vec4(vertex.position, 1)).xyz();;
            vertex.normal = nxForm * vertex.normal;
            vertex.tangent = nxForm * vertex.tangent;
            vertex.bitangent = nxForm * vertex.bitangent;
        }

//    xform = glm::translate(glm::mat4(1), glm::vec3(-26.5, (33 - w) * 0.5, -29.5));
        xform = glm::translate(glm::mat4(1), glm::vec3(-10.5, (33 - w) * 0.5, -5));
        xform = glm::rotate(xform, glm::radians(15.f), {0, 1, 0});
        xform = glm::scale(xform, glm::vec3(16.5, 33, 16.5));
        auto tallBox = cube(white);

        nxForm = glm::inverseTranspose(glm::mat3(xform));
        for (auto &vertex: tallBox.vertices) {
            vertex.position = (xform * glm::vec4(vertex.position, 1)).xyz();;
            vertex.normal = nxForm * vertex.normal;
            vertex.tangent = nxForm * vertex.tangent;
            vertex.bitangent = nxForm * vertex.bitangent;
        }

        return {
                light, ceiling, rightWall, floor, leftWall, tallBox, shortBox, backWall
        };
    }
}
