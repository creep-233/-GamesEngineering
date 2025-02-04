#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>

#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <algorithm>
#include <chrono>

#include <cmath>
#include "matrix.h"
#include "colour.h"
#include "mesh.h"
#include "zbuffer.h"
#include "renderer.h"
#include "RNG.h"
#include "light.h"
#include "triangle.h"

#include <windows.h> 


// Main rendering function that processes a mesh, transforms its vertices, applies lighting, and draws triangles on the canvas.
// Input Variables:
// - renderer: The Renderer object used for drawing.
// - mesh: Pointer to the Mesh object containing vertices and triangles to render.
// - camera: Matrix representing the camera's transformation.
// - L: Light object representing the lighting parameters.
void render(Renderer& renderer, Mesh* mesh, matrix& camera, Light& L) {
    // Combine perspective, camera, and world transformations for the mesh
    matrix p = renderer.perspective * camera * mesh->world;

    // Iterate through all triangles in the mesh
    for (triIndices& ind : mesh->triangles) {
        Vertex t[3]; // Temporary array to store transformed triangle vertices

        // Transform each vertex of the triangle
        for (unsigned int i = 0; i < 3; i++) {
            t[i].p = p * mesh->vertices[ind.v[i]].p; // Apply transformations
            t[i].p.divideW(); // Perspective division to normalize coordinates

            // Transform normals into world space for accurate lighting
            // no need for perspective correction as no shearing or non-uniform scaling
            t[i].normal = mesh->world * mesh->vertices[ind.v[i]].normal; 
            t[i].normal.normalise();

            // Map normalized device coordinates to screen space
            t[i].p[0] = (t[i].p[0] + 1.f) * 0.5f * static_cast<float>(renderer.canvas.getWidth());
            t[i].p[1] = (t[i].p[1] + 1.f) * 0.5f * static_cast<float>(renderer.canvas.getHeight());
            t[i].p[1] = renderer.canvas.getHeight() - t[i].p[1]; // Invert y-axis

            // Copy vertex colours
            t[i].rgb = mesh->vertices[ind.v[i]].rgb;
        }

        // Clip triangles with Z-values outside [-1, 1]
        if (fabs(t[0].p[2]) > 1.0f || fabs(t[1].p[2]) > 1.0f || fabs(t[2].p[2]) > 1.0f) continue;

        // Create a triangle object and render it
        triangle tri(t[0], t[1], t[2]);
        tri.draw(renderer, L, mesh->ka, mesh->kd);
    }
}

// Test scene function to demonstrate rendering with user-controlled transformations
// No input variables
void sceneTest() {
    Renderer renderer;
    // create light source {direction, diffuse intensity, ambient intensity}
    Light L{ vec4(0.f, 1.f, 1.f, 0.f), colour(1.0f, 1.0f, 1.0f), colour(0.1f, 0.1f, 0.1f) };
    // camera is just a matrix
    matrix camera = matrix::makeIdentity(); // Initialize the camera with identity matrix

    bool running = true; // Main loop control variable

    std::vector<Mesh*> scene; // Vector to store scene objects

    // Create a sphere and a rectangle mesh
    Mesh mesh = Mesh::makeSphere(1.0f, 10, 20);
    //Mesh mesh2 = Mesh::makeRectangle(-2, -1, 2, 1);

    // add meshes to scene
    scene.push_back(&mesh);
   // scene.push_back(&mesh2); 

    float x = 0.0f, y = 0.0f, z = -4.0f; // Initial translation parameters
    mesh.world = matrix::makeTranslation(x, y, z);
    //mesh2.world = matrix::makeTranslation(x, y, z) * matrix::makeRotateX(0.01f);

    // Main rendering loop
    while (running) {
        renderer.canvas.checkInput(); // Handle user input
        renderer.clear(); // Clear the canvas for the next frame

        // Apply transformations to the meshes
     //   mesh2.world = matrix::makeTranslation(x, y, z) * matrix::makeRotateX(0.01f);
        mesh.world = matrix::makeTranslation(x, y, z);

        // Handle user inputs for transformations
        if (renderer.canvas.keyPressed(VK_ESCAPE)) break;
        if (renderer.canvas.keyPressed('A')) x += -0.1f;
        if (renderer.canvas.keyPressed('D')) x += 0.1f;
        if (renderer.canvas.keyPressed('W')) y += 0.1f;
        if (renderer.canvas.keyPressed('S')) y += -0.1f;
        if (renderer.canvas.keyPressed('Q')) z += 0.1f;
        if (renderer.canvas.keyPressed('E')) z += -0.1f;

        // Render each object in the scene
        for (auto& m : scene)
            render(renderer, m, camera, L);

        renderer.present(); // Display the rendered frame
    }
}

// Utility function to generate a random rotation matrix
// No input variables
matrix makeRandomRotation() {
    RandomNumberGenerator& rng = RandomNumberGenerator::getInstance();
    unsigned int r = rng.getRandomInt(0, 3);

    switch (r) {
    case 0: return matrix::makeRotateX(rng.getRandomFloat(0.f, 2.0f * M_PI));
    case 1: return matrix::makeRotateY(rng.getRandomFloat(0.f, 2.0f * M_PI));
    case 2: return matrix::makeRotateZ(rng.getRandomFloat(0.f, 2.0f * M_PI));
    default: return matrix::makeIdentity();
    }
}

// Function to render a scene with multiple objects and dynamic transformations
// No input variables
void scene1() {
    Renderer renderer;
    matrix camera;
    Light L{ vec4(0.f, 1.f, 1.f, 0.f), colour(1.0f, 1.0f, 1.0f), colour(0.1f, 0.1f, 0.1f) };

    bool running = true;

    std::vector<Mesh*> scene;

    // Create a scene of 40 cubes with random rotations
    for (unsigned int i = 0; i < 20; i++) {
        Mesh* m = new Mesh();
        *m = Mesh::makeCube(1.f);
        m->world = matrix::makeTranslation(-2.0f, 0.0f, (-3 * static_cast<float>(i))) * makeRandomRotation();
        scene.push_back(m);
        m = new Mesh();
        *m = Mesh::makeCube(1.f);
        m->world = matrix::makeTranslation(2.0f, 0.0f, (-3 * static_cast<float>(i))) * makeRandomRotation();
        scene.push_back(m);
    }

    float zoffset = 8.0f; // Initial camera Z-offset
    float step = -0.1f;  // Step size for camera movement

    auto start = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> end;
    int cycle = 0;

    // Main rendering loop
    while (running) {
        renderer.canvas.checkInput();
        renderer.clear();

        camera = matrix::makeTranslation(0, 0, -zoffset); // Update camera position

        // Rotate the first two cubes in the scene
        scene[0]->world = scene[0]->world * matrix::makeRotateXYZ(0.1f, 0.1f, 0.0f);
        scene[1]->world = scene[1]->world * matrix::makeRotateXYZ(0.0f, 0.1f, 0.2f);

        if (renderer.canvas.keyPressed(VK_ESCAPE)) break;

        zoffset += step;
        if (zoffset < -60.f || zoffset > 8.f) {
            step *= -1.f;
            if (++cycle % 2 == 0) {
                end = std::chrono::high_resolution_clock::now();
                std::cout << cycle / 2 << " :" << std::chrono::duration<double, std::milli>(end - start).count() << "ms\n";
                start = std::chrono::high_resolution_clock::now();
            }
        }

        for (auto& m : scene)
            render(renderer, m, camera, L);
        renderer.present();
    }

    for (auto& m : scene)
        delete m;
}

// Scene with a grid of cubes and a moving sphere
// No input variables
void scene2() {
    Renderer renderer;
    matrix camera = matrix::makeIdentity();
    Light L{ vec4(0.f, 1.f, 1.f, 0.f), colour(1.0f, 1.0f, 1.0f), colour(0.1f, 0.1f, 0.1f) };

    std::vector<Mesh*> scene;

    struct rRot { float x; float y; float z; }; // Structure to store random rotation parameters
    std::vector<rRot> rotations;

    RandomNumberGenerator& rng = RandomNumberGenerator::getInstance();

    // Create a grid of cubes with random rotations
    for (unsigned int y = 0; y < 6; y++) {
        for (unsigned int x = 0; x < 8; x++) {
            Mesh* m = new Mesh();
            *m = Mesh::makeCube(1.f);
            scene.push_back(m);
            m->world = matrix::makeTranslation(-7.0f + (static_cast<float>(x) * 2.f), 5.0f - (static_cast<float>(y) * 2.f), -8.f);
            rRot r{ rng.getRandomFloat(-.1f, .1f), rng.getRandomFloat(-.1f, .1f), rng.getRandomFloat(-.1f, .1f) };
            rotations.push_back(r);
        }
    }

    // Create a sphere and add it to the scene
    Mesh* sphere = new Mesh();
    *sphere = Mesh::makeSphere(1.0f, 10, 20);
    scene.push_back(sphere);
    float sphereOffset = -6.f;
    float sphereStep = 0.1f;
    sphere->world = matrix::makeTranslation(sphereOffset, 0.f, -6.f);

    auto start = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> end;
    int cycle = 0;

    bool running = true;
    while (running) {
        renderer.canvas.checkInput();
        renderer.clear();

        // Rotate each cube in the grid
        for (unsigned int i = 0; i < rotations.size(); i++)
            scene[i]->world = scene[i]->world * matrix::makeRotateXYZ(rotations[i].x, rotations[i].y, rotations[i].z);

        // Move the sphere back and forth
        sphereOffset += sphereStep;
        sphere->world = matrix::makeTranslation(sphereOffset, 0.f, -6.f);
        if (sphereOffset > 6.0f || sphereOffset < -6.0f) {
            sphereStep *= -1.f;
            if (++cycle % 2 == 0) {
                end = std::chrono::high_resolution_clock::now();
                std::cout << cycle / 2 << " :" << std::chrono::duration<double, std::milli>(end - start).count() << "ms\n";
                start = std::chrono::high_resolution_clock::now();
            }
        }

        if (renderer.canvas.keyPressed(VK_ESCAPE)) break;

        for (auto& m : scene)
            render(renderer, m, camera, L);
        renderer.present();
    }

    for (auto& m : scene)
        delete m;
}


struct DynamicObject {
    Mesh* mesh;     
    float rotationSpeedX;
    float rotationSpeedY;
    float translationSpeed;
    float translationOffset;
    bool isSineWave;
};


void scene3() {
    Renderer renderer;
    matrix camera = matrix::makeIdentity();
    Light L{ vec4(0.f, 1.f, 1.f, 0.f), colour(1.0f, 1.0f, 1.0f), colour(0.1f, 0.1f, 0.1f) };

    std::vector<Mesh*> scene;
    std::vector<DynamicObject> dynamicObjects;

    RandomNumberGenerator& rng = RandomNumberGenerator::getInstance();

    // **创建背景矩形**
    Mesh* background = new Mesh();
    *background = Mesh::makeRectangle(-15, -15, 15, 15);
    background->world = matrix::makeTranslation(0, 0, -30.f);
    scene.push_back(background);

    // **创建旋转立方体网格**
    for (unsigned int y = 0; y < 40; y++) {
        for (unsigned int x = 0; x < 50; x++) {
            Mesh* cube = new Mesh();
            *cube = Mesh::makeCube(1.f);
            cube->world = matrix::makeTranslation(-25.0f + (x * 2.f), 20.0f - (y * 2.f), -20.f);
            float rotationSpeedX = rng.getRandomFloat(-0.05f, 0.05f);
            float rotationSpeedY = rng.getRandomFloat(-0.05f, 0.05f);
            dynamicObjects.push_back({ cube, rotationSpeedX, rotationSpeedY, 0.f, 0.f, false });
            scene.push_back(cube);
        }
    }

    // **创建绕圈运动的球体**
    for (int i = 0; i < 10; i++) {
        Mesh* sphere = new Mesh();
        *sphere = Mesh::makeSphere(1.0f, 10, 20);
        sphere->world = matrix::makeTranslation(5.0f * std::cos(i * 0.5f), 5.0f * std::sin(i * 0.5f), -10.0f);
        float orbitSpeed = rng.getRandomFloat(0.02f, 0.05f);
        dynamicObjects.push_back({ sphere, 0.f, 0.f, orbitSpeed, i * 0.5f, true });
        scene.push_back(sphere);
    }

    // **多线程更新所有动态物体**
    int numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    int objectsPerThread = dynamicObjects.size() / numThreads;

    auto updateObjects = [&](int start, int end) {
        for (int i = start; i < end; i++) {
            auto& obj = dynamicObjects[i];
            if (obj.isSineWave) {
                obj.translationOffset += obj.translationSpeed;
                obj.mesh->world = matrix::makeTranslation(5.0f * std::cos(obj.translationOffset),
                    5.0f * std::sin(obj.translationOffset), -8.0f);
            }
            else {
                obj.mesh->world = obj.mesh->world * matrix::makeRotateXYZ(obj.rotationSpeedX, obj.rotationSpeedY, 0.0f);
            }
        }
        };


    for (int t = 0; t < numThreads; ++t) {
        int start = t * objectsPerThread;
        int end = (t == numThreads - 1) ? dynamicObjects.size() : start + objectsPerThread;
        threads.emplace_back(updateObjects, start, end);
    }

    // **光源旋转**
    float lightAngle = 0.0f;
    float lightSpeed = 0.02f;

    // **计时变量**
    auto start = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> end;
    int cycle = 0;
    float cycleProgress = 0.0f;  // 记录球体的旋转角度进度
    const float fullCycleThreshold = 2.0f * M_PI;  // 完整旋转一周 (360° = 2π)

    bool running = true;
    while (running) {
        renderer.canvas.checkInput();
        renderer.clear();

        // **动态调整光照方向**
        lightAngle += lightSpeed;
        L.omega_i = vec4(std::sin(lightAngle), 1.f, std::cos(lightAngle), 0.f);

        // **更新所有动态物体**
        for (auto& obj : dynamicObjects) {
            if (obj.isSineWave) {
                obj.translationOffset += obj.translationSpeed;
                obj.mesh->world = matrix::makeTranslation(obj.translationOffset, std::sin(obj.translationOffset) * 2.f, -8.0f);
            }
            else {
                obj.mesh->world = obj.mesh->world * matrix::makeRotateXYZ(obj.rotationSpeedX, obj.rotationSpeedY, 0.0f);
            }
        }


        // **处理用户输入**
        if (renderer.canvas.keyPressed(VK_ESCAPE)) break;
        if (renderer.canvas.keyPressed('D'))
            camera = camera * matrix::makeTranslation(-0.1f, 0.0f, 0.0f);
        if (renderer.canvas.keyPressed('A'))
            camera = camera * matrix::makeTranslation(0.1f, 0.0f, 0.0f);
        if (renderer.canvas.keyPressed('W'))
            camera = camera * matrix::makeTranslation(0.0f, -0.1f, 0.0f);
        if (renderer.canvas.keyPressed('S'))
            camera = camera * matrix::makeTranslation(0.0f, 0.1f, 0.0f);

        // **渲染整个场景**
        for (auto& m : scene)
            render(renderer, m, camera, L);

        renderer.present();

        // **每两次反转计时一次**
        if (++cycle % 30 == 0) {
            end = std::chrono::high_resolution_clock::now();
            std::cout /*<< "循环 " << cycle / 30 << " 次用时: "*/
                << std::chrono::duration<double, std::milli>(end - start).count()
                << " ms\n";
            start = std::chrono::high_resolution_clock::now();
        }
    }

    // **清理内存**
    for (auto& m : scene)
        delete m;
}




// Entry point of the application
// No input variables
int main() {

    Renderer renderer;

    triangle::initializePool(std::thread::hardware_concurrency());

    // Uncomment the desired scene function to run
    //scene1();
    //scene2();
    scene3();
    //sceneTest(); 

    return 0;
}