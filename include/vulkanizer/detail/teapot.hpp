#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "teapotdata.hpp"

namespace vkz::teapot {

    glm::vec3 evaluate(int gridU, int gridV, float *B, glm::vec3 patch[][4])
    {
        glm::vec3 p(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                p += patch[i][j] * B[gridU * 4 + i] * B[gridV * 4 + j];
            }
        }
        return p;
    }

    glm::vec3 evaluateNormal(int gridU, int gridV, float *B, float *dB, glm::vec3 patch[][4])
    {
        glm::vec3 du(0.0f, 0.0f, 0.0f);
        glm::vec3 dv(0.0f, 0.0f, 0.0f);

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                du += patch[i][j] * dB[gridU * 4 + i] * B[gridV * 4 + j];
                dv += patch[i][j] * B[gridU * 4 + i] * dB[gridV * 4 + j];
            }
        }

        glm::vec3 norm = glm::cross(du, dv);
        if (glm::length(norm) != 0.0f) {
            norm = glm::normalize(norm);
        }

        return norm;
    }

    void getPatch(int patchNum, glm::vec3 patch[][4], bool reverseV)
    {
        for (int u = 0; u < 4; u++) {          // Loop in u direction
            for (int v = 0; v < 4; v++) {     // Loop in v direction
                if (reverseV) {
                    patch[u][v] = glm::vec3(
                            cpdata[patchdata[patchNum][u * 4 + (3 - v)]][0],
                            cpdata[patchdata[patchNum][u * 4 + (3 - v)]][1],
                            cpdata[patchdata[patchNum][u * 4 + (3 - v)]][2]
                    );
                }
                else {
                    patch[u][v] = glm::vec3(
                            cpdata[patchdata[patchNum][u * 4 + v]][0],
                            cpdata[patchdata[patchNum][u * 4 + v]][1],
                            cpdata[patchdata[patchNum][u * 4 + v]][2]
                    );
                }
            }
        }
    }

    void buildPatch(glm::vec3 patch[][4],
                    float *B, float *dB,
                    float *v, float *n, float *tc,
                    unsigned int *el,
                    int &index, int &elIndex, int &tcIndex, int grid, glm::mat3 reflect,
                    bool invertNormal)
    {
        int startIndex = index / 3;
        float tcFactor = 1.0f / grid;

        for (int i = 0; i <= grid; i++)
        {
            for (int j = 0; j <= grid; j++)
            {
                glm::vec3 pt = reflect * evaluate(i, j, B, patch);
                glm::vec3 norm = reflect * evaluateNormal(i, j, B, dB, patch);
                if (invertNormal)
                    norm = -norm;

                v[index] = pt.x;
                v[index + 1] = pt.y;
                v[index + 2] = pt.z;

                n[index] = norm.x;
                n[index + 1] = norm.y;
                n[index + 2] = norm.z;

                tc[tcIndex] = i * tcFactor;
                tc[tcIndex + 1] = j * tcFactor;

                index += 3;
                tcIndex += 2;
            }
        }

        for (int i = 0; i < grid; i++)
        {
            int iStart = i * (grid + 1) + startIndex;
            int nextiStart = (i + 1) * (grid + 1) + startIndex;
            for (int j = 0; j < grid; j++)
            {
                el[elIndex] = iStart + j;
                el[elIndex + 1] = nextiStart + j + 1;
                el[elIndex + 2] = nextiStart + j;

                el[elIndex + 3] = iStart + j;
                el[elIndex + 4] = iStart + j + 1;
                el[elIndex + 5] = nextiStart + j + 1;

                elIndex += 6;
            }
        }
    }

    void buildPatchReflect(int patchNum,
                           float *B, float *dB,
                           float *v, float *n,
                           float *tc, unsigned int *el,
                           int &index, int &elIndex, int &tcIndex, int grid,
                           bool reflectX, bool reflectY)
    {
        glm::vec3 patch[4][4];
        glm::vec3 patchRevV[4][4];
        getPatch(patchNum, patch, false);
        getPatch(patchNum, patchRevV, true);

        // Patch without modification
        buildPatch(patch, B, dB, v, n, tc, el,
                   index, elIndex, tcIndex, grid, glm::mat3(1.0f), true);

        // Patch reflected in x
        if (reflectX) {
            buildPatch(patchRevV, B, dB, v, n, tc, el,
                       index, elIndex, tcIndex, grid, glm::mat3(glm::vec3(-1.0f, 0.0f, 0.0f),
                                                                glm::vec3(0.0f, 1.0f, 0.0f),
                                                                glm::vec3(0.0f, 0.0f, 1.0f)), false);
        }

        // Patch reflected in y
        if (reflectY) {
            buildPatch(patchRevV, B, dB, v, n, tc, el,
                       index, elIndex, tcIndex, grid, glm::mat3(glm::vec3(1.0f, 0.0f, 0.0f),
                                                                glm::vec3(0.0f, -1.0f, 0.0f),
                                                                glm::vec3(0.0f, 0.0f, 1.0f)), false);
        }

        // Patch reflected in x and y
        if (reflectX && reflectY) {
            buildPatch(patch, B, dB, v, n, tc, el,
                       index, elIndex, tcIndex, grid, glm::mat3(glm::vec3(-1.0f, 0.0f, 0.0f),
                                                                glm::vec3(0.0f, -1.0f, 0.0f),
                                                                glm::vec3(0.0f, 0.0f, 1.0f)), true);
        }
    }


    void computeBasisFunctions(float * B, float * dB, int grid) {
        float inc = 1.0f / grid;
        for (int i = 0; i <= grid; i++)
        {
            float t = i * inc;
            float tSqr = t * t;
            float oneMinusT = (1.0f - t);
            float oneMinusT2 = oneMinusT * oneMinusT;

            B[i * 4 + 0] = oneMinusT * oneMinusT2;
            B[i * 4 + 1] = 3.0f * oneMinusT2 * t;
            B[i * 4 + 2] = 3.0f * oneMinusT * tSqr;
            B[i * 4 + 3] = t * tSqr;

            dB[i * 4 + 0] = -3.0f * oneMinusT2;
            dB[i * 4 + 1] = -6.0f * t * oneMinusT + 3.0f * oneMinusT2;
            dB[i * 4 + 2] = -3.0f * tSqr + 6.0f * t * oneMinusT;
            dB[i * 4 + 3] = 3.0f * tSqr;
        }
    }

    inline void generatePatches(float * v, float * n, float * tc, unsigned int* el, int grid) {
        float * B = new float[4 * (grid + 1)];  // Pre-computed Bernstein basis functions
        float * dB = new float[4 * (grid + 1)]; // Pre-computed derivitives of basis functions

        int idx = 0, elIndex = 0, tcIndex = 0;

        // Pre-compute the basis functions  (Bernstein polynomials)
        // and their derivatives
        computeBasisFunctions(B, dB, grid);

        // Build each patch
        // The rim
        buildPatchReflect(0, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, true, true);
        // The body
        buildPatchReflect(1, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, true, true);
        buildPatchReflect(2, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, true, true);
        // The lid
        buildPatchReflect(3, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, true, true);
        buildPatchReflect(4, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, true, true);
        // The bottom
        buildPatchReflect(5, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, true, true);
        // The handle
        buildPatchReflect(6, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, false, true);
        buildPatchReflect(7, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, false, true);
        // The spout
        buildPatchReflect(8, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, false, true);
        buildPatchReflect(9, B, dB, v, n, tc, el, idx, elIndex, tcIndex, grid, false, true);

        delete[] B;
        delete[] dB;
    }

    inline void moveLid(int grid, float *v, const glm::mat4 & lidTransform) {

        int start = 3 * 12 * (grid + 1) * (grid + 1);
        int end = 3 * 20 * (grid + 1) * (grid + 1);

        for (int i = start; i < end; i += 3)
        {
            glm::vec4 vert = glm::vec4(v[i], v[i + 1], v[i + 2], 1.0f);
            vert = lidTransform * vert;
            v[i] = vert.x;
            v[i + 1] = vert.y;
            v[i + 2] = vert.z;
        }
    }

}