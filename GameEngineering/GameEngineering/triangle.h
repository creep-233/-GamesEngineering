#pragma once

#include "mesh.h"
#include "colour.h"
#include "renderer.h"
#include "light.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include "ThreadPool.h"



// Simple support class for a 2D vector
class vec2D {
public:
    float x, y;

    // Default constructor initializes both components to 0
    vec2D() { x = y = 0.f; };

    // Constructor initializes components with given values
    vec2D(float _x, float _y) : x(_x), y(_y) {}

    // Constructor initializes components from a vec4
    vec2D(vec4 v) {
        x = v[0];
        y = v[1];
    }

    // Display the vector components
    void display() { std::cout << x << '\t' << y << std::endl; }

    // Overloaded subtraction operator for vector subtraction
    vec2D operator- (vec2D& v) {
        vec2D q;
        q.x = x - v.x;
        q.y = y - v.y;
        return q;
    }
};

// Class representing a triangle for rendering purposes
class triangle {
    Vertex v[3];       // Vertices of the triangle
    float area;        // Area of the triangle
    colour col[3];     // Colors for each vertex of the triangle

private:
    inline static std::unique_ptr<ThreadPool> pool = nullptr;  // **使用智能指针管理线程池**

public:
    // Constructor initializes the triangle with three vertices
    // Input Variables:
    // - v1, v2, v3: Vertices defining the triangle
    //triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    //    v[0] = v1;
    //    v[1] = v2;
    //    v[2] = v3;

    //    // Calculate the 2D area of the triangle
    //    vec2D e1 = vec2D(v[1].p - v[0].p);
    //    vec2D e2 = vec2D(v[2].p - v[0].p);
    //    area = abs(e1.x * e2.y - e1.y * e2.x);
    //}

    triangle(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
        v[0] = v1;
        v[1] = v2;
        v[2] = v3;

        // 使用 AVX 计算 e1, e2，并计算叉积
        __m128 p0 = _mm_loadu_ps(v[0].p.getValues());
        __m128 p1 = _mm_loadu_ps(v[1].p.getValues());
        __m128 p2 = _mm_loadu_ps(v[2].p.getValues());

        __m128 e1 = _mm_sub_ps(p1, p0);  // e1 = p1 - p0
        __m128 e2 = _mm_sub_ps(p2, p0);  // e2 = p2 - p0

        float e1_x = _mm_cvtss_f32(e1);
        float e1_y = _mm_cvtss_f32(_mm_shuffle_ps(e1, e1, _MM_SHUFFLE(0, 3, 2, 1)));  // 获取 y 分量
        float e2_x = _mm_cvtss_f32(e2);
        float e2_y = _mm_cvtss_f32(_mm_shuffle_ps(e2, e2, _MM_SHUFFLE(0, 3, 2, 1)));

        area = fabs(e1_x * e2_y - e1_y * e2_x);  // 叉积计算
    }


    // Helper function to compute the cross product for barycentric coordinates
    // Input Variables:
    // - v1, v2: Edges defining the vector
    // - p: Point for which coordinates are being calculated
    float getC(vec2D v1, vec2D v2, vec2D p) {
        vec2D e = v2 - v1;
        vec2D q = p - v1;
        return q.y * e.x - q.x * e.y;
    }

    // Compute barycentric coordinates for a given point
    // Input Variables:
    // - p: Point to check within the triangle
    // Output Variables:
    // - alpha, beta, gamma: Barycentric coordinates of the point
    // Returns true if the point is inside the triangle, false otherwise
    bool getCoordinates(vec2D p, float& alpha, float& beta, float& gamma) {
        alpha = getC(vec2D(v[0].p), vec2D(v[1].p), p) / area;
        beta = getC(vec2D(v[1].p), vec2D(v[2].p), p) / area;
        gamma = getC(vec2D(v[2].p), vec2D(v[0].p), p) / area;

        if (alpha < 0.f || beta < 0.f || gamma < 0.f) return false;
        return true;
    }

    static void initializePool(int numThreads) {
        pool = std::make_unique<ThreadPool>(numThreads);
    }


    void computeBarycentricParallel(std::vector<vec2D>& points, std::vector<float>& alphas, std::vector<float>& betas, std::vector<float>& gammas, int numThreads) {
        std::vector<std::thread> threads;
        int pointsPerThread = points.size() / numThreads;
        std::mutex mtx;

        auto worker = [&](int start, int end) {
            for (int i = start; i < end; ++i) {
                float alpha, beta, gamma;
                bool inside = getCoordinates(points[i], alpha, beta, gamma);
                if (inside) {
                    std::lock_guard<std::mutex> lock(mtx);
                    alphas[i] = alpha;
                    betas[i] = beta;
                    gammas[i] = gamma;
                }
            }
            };

        for (int i = 0; i < numThreads; ++i) {
            int start = i * pointsPerThread;
            int end = (i == numThreads - 1) ? points.size() : start + pointsPerThread;
            threads.emplace_back(worker, start, end);
        }

        for (auto& t : threads) {
            t.join();
        }
    }







    // Template function to interpolate values using barycentric coordinates
    // Input Variables:
    // - alpha, beta, gamma: Barycentric coordinates
    // - a1, a2, a3: Values to interpolate
    // Returns the interpolated value
    template <typename T>
    T interpolate(float alpha, float beta, float gamma, T a1, T a2, T a3) {
        return (a1 * alpha) + (a2 * beta) + (a3 * gamma);
    }

    vec4 interpolate(float alpha, float beta, float gamma, vec4 a1, vec4 a2, vec4 a3) {
        __m128 v_alpha = _mm_set1_ps(alpha);
        __m128 v_beta = _mm_set1_ps(beta);
        __m128 v_gamma = _mm_set1_ps(gamma);

        __m128 v1 = _mm_loadu_ps(a1.getValues());
        __m128 v2 = _mm_loadu_ps(a2.getValues());
        __m128 v3 = _mm_loadu_ps(a3.getValues());

        __m128 result = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(v1, v_alpha), _mm_mul_ps(v2, v_beta)),
            _mm_mul_ps(v3, v_gamma)
        );

        vec4 output;
        _mm_storeu_ps(output.getValues(), result);
        return output;
    }

    float interpolate_float(float alpha, float beta, float gamma, float a1, float a2, float a3) {
        return (a1 * alpha) + (a2 * beta) + (a3 * gamma);
    }

    colour interpolate_colour(float alpha, float beta, float gamma, colour a1, colour a2, colour a3) {
        return (a1 * alpha) + (a2 * beta) + (a3 * gamma);
    }






    // Draw the triangle on the canvas
    // Input Variables:
    // - renderer: Renderer object for drawing
    // - L: Light object for shading calculations
    // - ka, kd: Ambient and diffuse lighting coefficients
    void draw(Renderer& renderer, Light& L, float ka, float kd) {
        vec2D minV, maxV;

        // Get the screen-space bounds of the triangle
        getBoundsWindow(renderer.canvas, minV, maxV);

        // Skip very small triangles
        if (area < 1.f) return;

        // Iterate over the bounding box and check each pixel
        for (int y = (int)(minV.y); y < (int)ceil(maxV.y); y++) {
            for (int x = (int)(minV.x); x < (int)ceil(maxV.x); x++) {
                float alpha, beta, gamma;

                // Check if the pixel lies inside the triangle
                if (getCoordinates(vec2D((float)x, (float)y), alpha, beta, gamma)) {
                    // Interpolate color, depth, and normals
                    //colour c = interpolate(beta, gamma, alpha, v[0].rgb, v[1].rgb, v[2].rgb);
                    colour c = interpolate_colour(beta, gamma, alpha, v[0].rgb, v[1].rgb, v[2].rgb);
                    c.clampColour();

                    //float depth = interpolate(beta, gamma, alpha, v[0].p[2], v[1].p[2], v[2].p[2]);
                    float depth = interpolate_float(beta, gamma, alpha, v[0].p[2], v[1].p[2], v[2].p[2]);

                    vec4 normal = interpolate(beta, gamma, alpha, v[0].normal, v[1].normal, v[2].normal);
                    normal.normalise();

                    // Perform Z-buffer test and apply shading
                    if (renderer.zbuffer(x, y) > depth && depth > 0.01f) {
                        // typical shader begin
                        L.omega_i.normalise();
                        float dot = max(vec4::dot(L.omega_i, normal), 0.0f);
                        colour a = (c * kd) * (L.L * dot + (L.ambient * kd));
                        // typical shader end
                        unsigned char r, g, b;
                        a.toRGB(r, g, b);
                        renderer.canvas.draw(x, y, r, g, b);
                        renderer.zbuffer(x, y) = depth;
                    }
                }
            }
        }
    }

    //加了之后测试场景更慢
    //void draw(Renderer& renderer, Light& L, float ka, float kd) {
    //    vec2D minV, maxV;
    //    getBoundsWindow(renderer.canvas, minV, maxV);

    //    if (area < 1.f) return;

    //    int numThreads = std::thread::hardware_concurrency();
    //    int rowsPerThread = (int)(ceil(maxV.y) - minV.y) / numThreads;

    //    auto worker = [&](int startRow, int endRow) {
    //        for (int y = startRow; y < endRow; y++) {
    //            for (int x = (int)minV.x; x < (int)ceil(maxV.x); x++) {
    //                float alpha, beta, gamma;

    //                if (getCoordinates(vec2D((float)x, (float)y), alpha, beta, gamma)) {
    //                    colour c = interpolate_colour(beta, gamma, alpha, v[0].rgb, v[1].rgb, v[2].rgb);
    //                    c.clampColour();
    //                    float depth = interpolate_float(beta, gamma, alpha, v[0].p[2], v[1].p[2], v[2].p[2]);

    //                    vec4 normal = interpolate(beta, gamma, alpha, v[0].normal, v[1].normal, v[2].normal);
    //                    normal.normalise();

    //                    if (renderer.zbuffer(x, y) > depth && depth > 0.01f) {
    //                        L.omega_i.normalise();
    //                        float dot = max(vec4::dot(L.omega_i, normal), 0.0f);
    //                        colour a = (c * kd) * (L.L * dot + (L.ambient * kd));
    //                        unsigned char r, g, b;
    //                        a.toRGB(r, g, b);
    //                        renderer.canvas.draw(x, y, r, g, b);
    //                        renderer.zbuffer(x, y) = depth;
    //                    }
    //                }
    //            }
    //        }
    //        };

    //    std::vector<std::thread> threads;
    //    for (int t = 0; t < numThreads; ++t) {
    //        int startRow = (int)minV.y + t * rowsPerThread;
    //        int endRow = (t == numThreads - 1) ? (int)ceil(maxV.y) : startRow + rowsPerThread;
    //        threads.emplace_back(worker, startRow, endRow);
    //    }

    //    for (auto& t : threads) {
    //        t.join();
    //    }
    //}







    // Compute the 2D bounds of the triangle
    // Output Variables:
    // - minV, maxV: Minimum and maximum bounds in 2D space
    void getBounds(vec2D& minV, vec2D& maxV) {
        minV = vec2D(v[0].p);
        maxV = vec2D(v[0].p);
        for (unsigned int i = 1; i < 3; i++) {
            minV.x = min(minV.x, v[i].p[0]);
            minV.y = min(minV.y, v[i].p[1]);
            maxV.x = max(maxV.x, v[i].p[0]);
            maxV.y = max(maxV.y, v[i].p[1]);
        }
    }

    // Compute the 2D bounds of the triangle, clipped to the canvas
    // Input Variables:
    // - canvas: Reference to the rendering canvas
    // Output Variables:
    // - minV, maxV: Clipped minimum and maximum bounds
    void getBoundsWindow(GamesEngineeringBase::Window& canvas, vec2D& minV, vec2D& maxV) {
        getBounds(minV, maxV);
        minV.x = max(minV.x, 0);
        minV.y = max(minV.y, 0);
        maxV.x = min(maxV.x, canvas.getWidth());
        maxV.y = min(maxV.y, canvas.getHeight());
    }

    // Debugging utility to display the triangle bounds on the canvas
    // Input Variables:
    // - canvas: Reference to the rendering canvas
    void drawBounds(GamesEngineeringBase::Window& canvas) {
        vec2D minV, maxV;
        getBounds(minV, maxV);

        for (int y = (int)minV.y; y < (int)maxV.y; y++) {
            for (int x = (int)minV.x; x < (int)maxV.x; x++) {
                canvas.draw(x, y, 255, 0, 0);
            }
        }
    }

    // Debugging utility to display the coordinates of the triangle vertices
    void display() {
        for (unsigned int i = 0; i < 3; i++) {
            v[i].p.display();
        }
        std::cout << std::endl;
    }
};
