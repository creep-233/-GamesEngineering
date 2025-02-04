#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include "GamesEngineeringBase.h"
#include "zbuffer.h"
#include "matrix.h"

// The `Renderer` class handles rendering operations, including managing the
// Z-buffer, canvas, and perspective transformations for a 3D scene.
class Renderer {
    float fov = 90.0f * M_PI / 180.0f; // Field of view in radians (converted from degrees)
    float aspect = 4.0f / 3.0f;        // Aspect ratio of the canvas (width/height)
    float n = 0.1f;                    // Near clipping plane distance
    float f = 100.0f;                  // Far clipping plane distance
public:
    Zbuffer<float> zbuffer;                  // Z-buffer for depth management
    GamesEngineeringBase::Window canvas;     // Canvas for rendering the scene
    matrix perspective;                      // Perspective projection matrix

    // Constructor initializes the canvas, Z-buffer, and perspective projection matrix.
    Renderer() {
        canvas.create(1024, 768, "Raster");  // Create a canvas with specified dimensions and title
        zbuffer.create(1024, 768);           // Initialize the Z-buffer with the same dimensions
        perspective = matrix::makePerspective(fov, aspect, n, f); // Set up the perspective matrix
    }

    // Clears the canvas and resets the Z-buffer.
    //void clear() {
    //    canvas.clear();  // Clear the canvas (sets all pixels to the background color)
    //    zbuffer.clear(); // Reset the Z-buffer to the farthest depth
    //}

    void clear() {
        canvas.clear();  // �������
        const int totalPixels = 1024 * 768;
        float* buffer = zbuffer.getBuffer();  // ��ȡ Z-buffer ����ָ��
        __m256 farDepth = _mm256_set1_ps(1.0f); // 1.0f ��ʾ��Զ����

        int i = 0;
        for (; i <= totalPixels - 8; i += 8) {
            _mm256_storeu_ps(&buffer[i], farDepth);
        }
        // ����ʣ��Ĳ��� 8 ��Ԫ��
        for (; i < totalPixels; ++i) {
            buffer[i] = 1.0f;
        }
    }



    // Presents the current canvas frame to the display.
    void present() {
        canvas.present(); // Display the rendered frame
    }
};
