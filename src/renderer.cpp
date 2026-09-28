#include "renderer.h"
#include "gl.h"
#include "objio.h"
#include <cmath>
#include <cstring>

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 normalize(Vec3 a) { float l = std::sqrt(dot(a, a)); return l > 0 ? a * (1.0f / l) : a; }

Vec3 Camera::eye() const {
    return target + Vec3{std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)} * distance;
}

void Camera::frame(Vec3 lo, Vec3 hi) {
    target = (lo + hi) * 0.5f;
    Vec3 d = hi - lo;
    float r = 0.5f * std::sqrt(dot(d, d));
    distance = std::max(1.0f, r / std::tan(fovY * 0.5f * 3.14159265f / 180.0f) * 1.05f);
}

namespace {
struct Mat4 { float m[16]; };
Mat4 perspective(float fovY, float aspect, float n, float f) {
    float t = 1.0f / std::tan(fovY * 0.5f * 3.14159265f / 180.0f);
    Mat4 r{}; r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
Mat4 lookAt(Vec3 eye, Vec3 at) {
    Vec3 f = normalize(at - eye);
    Vec3 upRef = std::fabs(f.y) > 0.999f ? Vec3{0, 0, -1} : Vec3{0, 1, 0};
    Vec3 s = normalize(cross(f, upRef)), u = cross(s, f);
    Mat4 r{};
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye); r.m[15] = 1;
    return r;
}
Mat4 mul(const Mat4 &a, const Mat4 &b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
float nearFar(const Camera &cam, float &farOut) {
    farOut = std::max(2000.0f, cam.distance * 20.0f);
    return std::max(0.05f, cam.distance * 0.002f);
}

GLuint compile(const char *vs, const char *fs, const char **error) {
    auto one = [&](GLenum type, const char *src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { static char log[1024]; glGetShaderInfoLog(s, sizeof log, nullptr, log); *error = log; return 0; }
        return s;
    };
    GLuint v = one(GL_VERTEX_SHADER, vs), f = one(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(v); glDeleteShader(f);
    if (!ok) { static char log[1024]; glGetProgramInfoLog(p, sizeof log, nullptr, log); *error = log; return 0; }
    return p;
}

const char *kVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(location=3) in int aBlock;
uniform mat4 uMVP;
out vec3 vNormal; out vec3 vColor; flat out int vBlock;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); vNormal = aNormal; vColor = aColor; vBlock = aBlock; }
)";
const char *kFS = R"(#version 330 core
in vec3 vNormal; in vec3 vColor; flat in int vBlock;
uniform int uLighting; uniform int uSelected; uniform int uHover; uniform vec4 uOverride;
out vec4 frag;
void main() {
    vec3 c = vColor;
    if (uLighting != 0) {
        vec3 L = normalize(vec3(0.35, 0.85, 0.4));
        c *= 0.62 + 0.45 * abs(dot(normalize(vNormal), L));
    }
    if (vBlock == uSelected) c = mix(c, vec3(1.0, 0.85, 0.1), 0.45);
    else if (vBlock == uHover) c = mix(c, vec3(1.0), 0.25);
    frag = uOverride.a > 0.0 ? uOverride : vec4(c, 1.0);
}
)";
const char *kLineVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";
const char *kLineFS = R"(#version 330 core
uniform vec4 uColor; out vec4 frag;
void main() { frag = uColor; }
)";

struct GpuVertex { float pos[3], normal[3], color[3]; int32_t block; };
} // namespace

TrackRenderer::~TrackRenderer() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (colorTex_) glDeleteTextures(1, &colorTex_);
    if (depthRb_) glDeleteRenderbuffers(1, &depthRb_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (lineVbo_) glDeleteBuffers(1, &lineVbo_);
    if (lineVao_) glDeleteVertexArrays(1, &lineVao_);
}

bool TrackRenderer::init(const char **error) {
    prog_ = compile(kVS, kFS, error);
    lineProg_ = prog_ ? compile(kLineVS, kLineFS, error) : 0;
    if (!prog_ || !lineProg_) return false;
    glGenVertexArrays(1, &vao_); glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(GpuVertex), (void *)offsetof(GpuVertex, pos));
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, sizeof(GpuVertex), (void *)offsetof(GpuVertex, normal));
    glVertexAttribPointer(2, 3, GL_FLOAT, 0, sizeof(GpuVertex), (void *)offsetof(GpuVertex, color));
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(GpuVertex), (void *)offsetof(GpuVertex, block));
    for (int i = 0; i < 4; ++i) glEnableVertexAttribArray(i);
    glGenVertexArrays(1, &lineVao_); glGenBuffers(1, &lineVbo_);
    glBindVertexArray(lineVao_); glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, sizeof(float) * 3, nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return true;
}

static Vec3 toView(const vr::Vec3s &v) {
    return {float(v.x / vr::kObjScale), float(v.y / vr::kObjScale), float(-v.z / vr::kObjScale)};
}

void TrackRenderer::setTrack(const vr::Track &track, const std::array<uint32_t, 256> &pal) {
    std::vector<GpuVertex> verts;
    tris_.clear(); blockLo_.clear(); blockHi_.clear();
    lo_ = {1e9f, 1e9f, 1e9f}; hi_ = {-1e9f, -1e9f, -1e9f};
    int id = 0;
    for (auto &kv : track.blocks) {
        const vr::Block &b = kv.second;
        Vec3 blo{1e9f, 1e9f, 1e9f}, bhi{-1e9f, -1e9f, -1e9f};
        for (auto &v : b.verts) {
            Vec3 p = toView(v);
            blo = {std::min(blo.x, p.x), std::min(blo.y, p.y), std::min(blo.z, p.z)};
            bhi = {std::max(bhi.x, p.x), std::max(bhi.y, p.y), std::max(bhi.z, p.z)};
        }
        lo_ = {std::min(lo_.x, blo.x), std::min(lo_.y, blo.y), std::min(lo_.z, blo.z)};
        hi_ = {std::max(hi_.x, bhi.x), std::max(hi_.y, bhi.y), std::max(hi_.z, bhi.z)};
        blockLo_.push_back(blo); blockHi_.push_back(bhi);
        for (auto &f : b.faces) {
            uint32_t c = pal[f.paletteIndex()];
            float col[3] = {((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f};
            Vec3 p[4];
            for (int k = 0; k < f.n; ++k) p[k] = toView(b.verts[f.idx[k]]);
            Vec3 n = normalize(cross(p[1] - p[0], p[2] - p[0]));
            if (f.n == 4 && dot(n, n) < 0.5f) n = normalize(cross(p[2] - p[0], p[3] - p[0]));
            int triIdx[2][3] = {{0, 1, 2}, {0, 2, 3}};
            for (int t = 0; t < (f.n == 4 ? 2 : 1); ++t) {
                Tri tri{{p[triIdx[t][0]], p[triIdx[t][1]], p[triIdx[t][2]]}, id};
                tris_.push_back(tri);
                for (auto &q : tri.p) {
                    GpuVertex gv{{q.x, q.y, q.z}, {n.x, n.y, n.z}, {col[0], col[1], col[2]}, id};
                    verts.push_back(gv);
                }
            }
        }
        ++id;
    }
    vertexCount_ = int(verts.size());
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(GpuVertex)), verts.data(), GL_STATIC_DRAW);
    // 32x32 grid of cells on a plane just under the track. Cell size 1024 world = 2048 ROM units.
    std::vector<float> lines;
    float y = lo_.y - 0.05f, cell = float(2048.0 / vr::kObjScale), half = 16 * cell;
    for (int i = 0; i <= 32; ++i) {
        float a = -half + i * cell;
        float seg[12] = {a, y, -half, a, y, half, -half, y, a, half, y, a};
        lines.insert(lines.end(), seg, seg + 12);
    }
    lineCount_ = int(lines.size() / 3);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(lines.size() * sizeof(float)), lines.data(), GL_STATIC_DRAW);
}

void TrackRenderer::ensureTarget(int w, int h) {
    if (w == fbW_ && h == fbH_ && fbo_) return;
    if (!fbo_) { glGenFramebuffers(1, &fbo_); glGenTextures(1, &colorTex_); glGenRenderbuffers(1, &depthRb_); }
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fbW_ = w; fbH_ = h;
}

void TrackRenderer::render(int w, int h, const Camera &cam, const RenderOptions &opt, int selected, int hover) {
    if (w < 1 || h < 1) return;
    ensureTarget(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(1);
    glClearColor(opt.background[0], opt.background[1], opt.background[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float farP, nearP = nearFar(cam, farP);
    Mat4 mvp = mul(perspective(cam.fovY, float(w) / float(h), nearP, farP), lookAt(cam.eye(), cam.target));

    glUseProgram(prog_);
    glUniformMatrix4fv(glGetUniformLocation(prog_, "uMVP"), 1, 0, mvp.m);
    glUniform1i(glGetUniformLocation(prog_, "uLighting"), opt.lighting ? 1 : 0);
    glUniform1i(glGetUniformLocation(prog_, "uSelected"), selected);
    glUniform1i(glGetUniformLocation(prog_, "uHover"), hover);
    glUniform4f(glGetUniformLocation(prog_, "uOverride"), 0, 0, 0, 0);
    glBindVertexArray(vao_);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1, 1);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (opt.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUniform4f(glGetUniformLocation(prog_, "uOverride"), 0.05f, 0.05f, 0.05f, 1);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount_);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (opt.cellGrid) {
        glUseProgram(lineProg_);
        glUniformMatrix4fv(glGetUniformLocation(lineProg_, "uMVP"), 1, 0, mvp.m);
        glUniform4f(glGetUniformLocation(lineProg_, "uColor"), 1, 1, 1, 0.35f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(lineVao_);
        glDrawArrays(GL_LINES, 0, lineCount_);
        glDisable(GL_BLEND);
    }
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

PickResult TrackRenderer::pick(const Camera &cam, int w, int h, float mx, float my) const {
    PickResult r;
    Vec3 eye = cam.eye(), f = normalize(cam.target - eye);
    Vec3 upRef = std::fabs(f.y) > 0.999f ? Vec3{0, 0, -1} : Vec3{0, 1, 0};
    Vec3 s = normalize(cross(f, upRef)), u = cross(s, f);
    float t = std::tan(cam.fovY * 0.5f * 3.14159265f / 180.0f);
    float nx = (2.0f * mx / w - 1.0f) * t * float(w) / float(h), ny = (1.0f - 2.0f * my / h) * t;
    Vec3 dir = normalize(f + s * nx + u * ny);
    float best = 1e30f;
    for (auto &tri : tris_) {                       // Moller-Trumbore
        Vec3 e1 = tri.p[1] - tri.p[0], e2 = tri.p[2] - tri.p[0], pv = cross(dir, e2);
        float det = dot(e1, pv);
        if (std::fabs(det) < 1e-9f) continue;
        float inv = 1.0f / det;
        Vec3 tv = eye - tri.p[0];
        float uu = dot(tv, pv) * inv;
        if (uu < 0 || uu > 1) continue;
        Vec3 qv = cross(tv, e1);
        float vv = dot(dir, qv) * inv;
        if (vv < 0 || uu + vv > 1) continue;
        float d = dot(e2, qv) * inv;
        if (d > 0 && d < best) { best = d; r.hit = true; r.blockId = tri.block; r.point = eye + dir * d; }
    }
    return r;
}

Vec3 TrackRenderer::blockCenter(int id) const {
    if (id < 0 || id >= int(blockLo_.size())) return (lo_ + hi_) * 0.5f;
    return (blockLo_[id] + blockHi_[id]) * 0.5f;
}

std::vector<uint8_t> TrackRenderer::readPixels(int &w, int &h) const {
    w = fbW_; h = fbH_;
    std::vector<uint8_t> px(size_t(w) * h * 4), out(px.size());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (int y = 0; y < h; ++y) std::memcpy(&out[size_t(y) * w * 4], &px[size_t(h - 1 - y) * w * 4], size_t(w) * 4);
    return out;
}
