#pragma once

#include <iostream>
#include <immintrin.h>
#include <thread>
#include <vector>
#include <functional>

// The `vec4` class represents a 4D vector and provides operations such as scaling, addition, subtraction, 
// normalization, and vector products (dot and cross).
class vec4 {
    union {
        struct {
            float x, y, z, w; // Components of the vector
        };
        float v[4];           // Array representation of the vector components
    };

public:

    const float* getV() const { return v; }
    float* getValues() { return v; }
    inline const float* getValues() const { return v; }

    // Constructor to initialize the vector with specified values.
    // Default values: x = 0, y = 0, z = 0, w = 1.
    // Input Variables:
    // - _x: X component of the vector
    // - _y: Y component of the vector
    // - _z: Z component of the vector
    // - _w: W component of the vector (default is 1.0)
    vec4(float _x = 0.f, float _y = 0.f, float _z = 0.f, float _w = 1.f)
        : x(_x), y(_y), z(_z), w(_w) {}

    // Displays the components of the vector in a readable format.
    void display() {
        std::cout << x << '\t' << y << '\t' << z << '\t' << w << std::endl;
    }

    // Scales the vector by a scalar value.
    // Input Variables:
    // - scalar: Value to scale the vector by
    // Returns a new scaled `vec4`.
    vec4 operator*(float scalar) const {
        return { x * scalar, y * scalar, z * scalar, w * scalar };
    }

    // Divides the vector by its W component and sets W to 1.
    // Useful for normalizing the W component after transformations.
    void divideW() {
        x /= w;
        y /= w;
        z /= w;
        w = 1.f;
    }

    // Accesses a vector component by index.
    // Input Variables:
    // - index: Index of the component (0 for x, 1 for y, 2 for z, 3 for w)
    // Returns a reference to the specified component.
    float& operator[](const unsigned int index) {
        return v[index];
    }

    // Accesses a vector component by index (const version).
    // Input Variables:
    // - index: Index of the component (0 for x, 1 for y, 2 for z, 3 for w)
    // Returns the specified component value.
    float operator[](const unsigned int index) const {
        return v[index];
    }

    // Subtracts another vector from this vector.
    // Input Variables:
    // - other: The vector to subtract
    // Returns a new `vec4` resulting from the subtraction.
    //vec4 operator-(const vec4& other) const {
    //    return vec4(x - other.x, y - other.y, z - other.z, 0.0f);
    //}

    vec4 operator-(const vec4& other) const {
        __m128 a = _mm_loadu_ps(this->v);
        __m128 b = _mm_loadu_ps(other.v);
        __m128 res = _mm_sub_ps(a, b);
        vec4 result;
        _mm_storeu_ps(result.v, res);
        return result;
    }


    // Adds another vector to this vector.
    // Input Variables:
    // - other: The vector to add
    // Returns a new `vec4` resulting from the addition.
    //vec4 operator+(const vec4& other) const {
    //    return vec4(x + other.x, y + other.y, z + other.z, 0.0f);
    //}

    vec4 operator+(const vec4& other) const {
        __m128 a = _mm_loadu_ps(this->v);
        __m128 b = _mm_loadu_ps(other.v);
        __m128 res = _mm_add_ps(a, b);
        vec4 result;
        _mm_storeu_ps(result.v, res);
        return result;
    }


    // Computes the cross product of two vectors.
    // Input Variables:
    // - v1: The first vector
    // - v2: The second vector
    // Returns a new `vec4` representing the cross product.
    //static vec4 cross(const vec4& v1, const vec4& v2) {
    //    return vec4(
    //        v1.y * v2.z - v1.z * v2.y,
    //        v1.z * v2.x - v1.x * v2.z,
    //        v1.x * v2.y - v1.y * v2.x,
    //        0.0f // The W component is set to 0 for cross products
    //    );
    //}

    static vec4 cross(const vec4& v1, const vec4& v2) {
        __m128 a = _mm_loadu_ps(v1.v); // 加载 v1
        __m128 b = _mm_loadu_ps(v2.v); // 加载 v2

        __m128 a_yzxw = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 b_yzxw = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 a_zxyw = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
        __m128 b_zxyw = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));

        __m128 result = _mm_sub_ps(_mm_mul_ps(a_yzxw, b_zxyw), _mm_mul_ps(a_zxyw, b_yzxw));

        vec4 res;
        _mm_storeu_ps(res.v, result); // 存回 res
        res.w = 0.0f; // 叉积 w 分量为 0
        return res;
    }




    // Computes the dot product of two vectors.
    // Input Variables:
    // - v1: The first vector
    // - v2: The second vector
    // Returns the dot product as a float.
    //static float dot(const vec4& v1, const vec4& v2) {
    //    return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
    //}


    static float dot(const vec4& v1, const vec4& v2) {
        __m128 a = _mm_loadu_ps(v1.v); // 加载 v1 向量
        __m128 b = _mm_loadu_ps(v2.v); // 加载 v2 向量
        __m128 res = _mm_dp_ps(a, b, 0x7F); // 计算点积（只加前三个）
        return _mm_cvtss_f32(res); // 取出结果
    }


    // Normalizes the vector to make its length equal to 1.
    // This operation does not affect the W component.
    //void normalise() {
    //    float length = std::sqrt(x * x + y * y + z * z);
    //    x /= length;
    //    y /= length;
    //    z /= length;
    //}

    void normalise() {
        __m128 vReg = _mm_loadu_ps(this->v); // 加载 this->v 到寄存器
        __m128 len = _mm_sqrt_ps(_mm_dp_ps(vReg, vReg, 0x7F)); // 计算向量长度
        vReg = _mm_div_ps(vReg, len); // 除以长度归一化
        _mm_storeu_ps(this->v, vReg); // 存回 this->v
    }


    // 批量归一化函数
    void normalizeVectorsParallel(std::vector<vec4>& vectors) {
        int numThreads = std::thread::hardware_concurrency(); // 获取可用线程数
        int vectorsPerThread = vectors.size() / numThreads;

        auto worker = [&](int start, int end) {
            for (int i = start; i < end; ++i) {
                vectors[i].normalise(); // 调用 vec4 的 normalise
            }
            };

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; ++t) {
            int start = t * vectorsPerThread;
            int end = (t == numThreads - 1) ? vectors.size() : start + vectorsPerThread;
            threads.emplace_back(worker, start, end); // 创建线程
        }

        for (auto& thread : threads) {
            thread.join(); // 等待线程完成
        }
    }




    // 批量点积函数
    std::vector<float> dotProductParallel(const std::vector<vec4>& v1, const std::vector<vec4>& v2) {
        int numThreads = std::thread::hardware_concurrency();
        int vectorsPerThread = v1.size() / numThreads;

        std::vector<float> results(v1.size(), 0.0f);
        auto worker = [&](int start, int end) {
            for (int i = start; i < end; ++i) {
                results[i] = vec4::dot(v1[i], v2[i]); // 调用 vec4 的点积
            }
            };

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; ++t) {
            int start = t * vectorsPerThread;
            int end = (t == numThreads - 1) ? v1.size() : start + vectorsPerThread;
            threads.emplace_back(worker, start, end);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return results; // 返回结果
    }


    // 批量叉积函数
    std::vector<vec4> crossProductParallel(const std::vector<vec4>& v1, const std::vector<vec4>& v2) {
        int numThreads = std::thread::hardware_concurrency();
        int vectorsPerThread = v1.size() / numThreads;

        std::vector<vec4> results(v1.size());
        auto worker = [&](int start, int end) {
            for (int i = start; i < end; ++i) {
                results[i] = vec4::cross(v1[i], v2[i]); // 调用 vec4 的叉积
            }
            };

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; ++t) {
            int start = t * vectorsPerThread;
            int end = (t == numThreads - 1) ? v1.size() : start + vectorsPerThread;
            threads.emplace_back(worker, start, end);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        return results; // 返回结果
    }



};
