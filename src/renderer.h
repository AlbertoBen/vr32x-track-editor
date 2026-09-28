// 3D view of a track: flat-shaded polygons rendered into an off-screen texture.
#pragma once
#include "rom.h"
#include <cstdint>
#include <vector>

struct Vec3 { float x, y, z; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
Vec3 normalize(Vec3 a);

struct Camera {
    Vec3 target{0, 0, 0};
    float yaw = 0.6f, pitch = 0.9f, distance = 80.0f, fovY = 50.0f;
    Vec3 eye() const;
    void frame(Vec3 lo, Vec3 hi);                 // fit a box in view
};

struct RenderOptions {
    bool lighting = true;
    bool wireframe = false;
    bool cellGrid = true;
    float background[3] = {0.35f, 0.55f, 0.85f};
};

struct PickResult { bool hit = false; int blockId = -1; Vec3 point{}; };

class TrackRenderer {
public:
    ~TrackRenderer();
    bool init(const char **error);
    // Rebuilds GPU buffers. blockIds follow the order of track.blocks.
    void setTrack(const vr::Track &track, const std::array<uint32_t, 256> &palette);
    void render(int width, int height, const Camera &cam, const RenderOptions &opt, int selectedBlock, int hoverBlock);
    unsigned texture() const { return colorTex_; }
    PickResult pick(const Camera &cam, int width, int height, float mx, float my) const;
    Vec3 boundsMin() const { return lo_; }
    Vec3 boundsMax() const { return hi_; }
    Vec3 blockCenter(int id) const;
    std::vector<uint8_t> readPixels(int &w, int &h) const;   // RGBA, top row first

private:
    struct Tri { Vec3 p[3]; int block; };
    std::vector<Tri> tris_;
    std::vector<Vec3> blockLo_, blockHi_;
    Vec3 lo_{}, hi_{};
    unsigned prog_ = 0, vao_ = 0, vbo_ = 0, lineProg_ = 0, lineVao_ = 0, lineVbo_ = 0;
    int vertexCount_ = 0, lineCount_ = 0;
    unsigned fbo_ = 0, colorTex_ = 0, depthRb_ = 0;
    int fbW_ = 0, fbH_ = 0;
    void ensureTarget(int w, int h);
};
