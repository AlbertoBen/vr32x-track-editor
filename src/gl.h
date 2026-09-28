// Minimal OpenGL 3.3 loader (no system GL headers needed).
#pragma once
#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#define VRGL_APIENTRY __stdcall
#else
#define VRGL_APIENTRY
#endif

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean, GLubyte;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr, GLintptr;

enum : GLenum {
    GL_DEPTH_BUFFER_BIT = 0x0100, GL_COLOR_BUFFER_BIT = 0x4000,
    GL_TRIANGLES = 0x0004, GL_LINES = 0x0001,
    GL_DEPTH_TEST = 0x0B71, GL_CULL_FACE = 0x0B44, GL_BLEND = 0x0BE2, GL_SCISSOR_TEST = 0x0C11,
    GL_POLYGON_OFFSET_FILL = 0x8037, GL_POLYGON_OFFSET_LINE = 0x2A02,
    GL_LEQUAL = 0x0203, GL_LESS = 0x0201,
    GL_FRONT_AND_BACK = 0x0408, GL_LINE = 0x1B01, GL_FILL = 0x1B02,
    GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303,
    GL_FLOAT = 0x1406, GL_UNSIGNED_BYTE = 0x1401, GL_UNSIGNED_INT = 0x1405, GL_INT = 0x1404,
    GL_ARRAY_BUFFER = 0x8892, GL_STATIC_DRAW = 0x88E4, GL_DYNAMIC_DRAW = 0x88E8,
    GL_VERTEX_SHADER = 0x8B31, GL_FRAGMENT_SHADER = 0x8B30,
    GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82,
    GL_TEXTURE_2D = 0x0DE1, GL_RGBA = 0x1908, GL_RGBA8 = 0x8058, GL_RGB = 0x1907,
    GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_MAG_FILTER = 0x2800, GL_LINEAR = 0x2601, GL_NEAREST = 0x2600,
    GL_FRAMEBUFFER = 0x8D40, GL_RENDERBUFFER = 0x8D41, GL_COLOR_ATTACHMENT0 = 0x8CE0,
    GL_DEPTH_ATTACHMENT = 0x8D00, GL_DEPTH_COMPONENT24 = 0x81A6, GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
    GL_PACK_ALIGNMENT = 0x0D05, GL_VERSION = 0x1F02, GL_RENDERER = 0x1F01,
};

#define VRGL_FUNCS(X) \
    X(void, glEnable, GLenum) X(void, glDisable, GLenum) X(void, glDepthFunc, GLenum) \
    X(void, glClear, GLbitfield) X(void, glClearColor, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, glViewport, GLint, GLint, GLsizei, GLsizei) X(void, glPolygonMode, GLenum, GLenum) \
    X(void, glPolygonOffset, GLfloat, GLfloat) X(void, glLineWidth, GLfloat) \
    X(void, glBlendFunc, GLenum, GLenum) X(void, glDepthMask, GLboolean) \
    X(void, glDrawArrays, GLenum, GLint, GLsizei) \
    X(void, glReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *) \
    X(void, glPixelStorei, GLenum, GLint) X(const GLubyte *, glGetString, GLenum) \
    X(void, glGenTextures, GLsizei, GLuint *) X(void, glBindTexture, GLenum, GLuint) \
    X(void, glDeleteTextures, GLsizei, const GLuint *) \
    X(void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *) \
    X(void, glTexParameteri, GLenum, GLenum, GLint) \
    X(GLuint, glCreateShader, GLenum) X(void, glShaderSource, GLuint, GLsizei, const GLchar *const *, const GLint *) \
    X(void, glCompileShader, GLuint) X(void, glGetShaderiv, GLuint, GLenum, GLint *) \
    X(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei *, GLchar *) X(void, glDeleteShader, GLuint) \
    X(GLuint, glCreateProgram) X(void, glAttachShader, GLuint, GLuint) X(void, glLinkProgram, GLuint) \
    X(void, glGetProgramiv, GLuint, GLenum, GLint *) X(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei *, GLchar *) \
    X(void, glUseProgram, GLuint) X(GLint, glGetUniformLocation, GLuint, const GLchar *) \
    X(void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat *) \
    X(void, glUniform1f, GLint, GLfloat) X(void, glUniform1i, GLint, GLint) \
    X(void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat) X(void, glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat) \
    X(void, glGenVertexArrays, GLsizei, GLuint *) X(void, glBindVertexArray, GLuint) \
    X(void, glDeleteVertexArrays, GLsizei, const GLuint *) \
    X(void, glGenBuffers, GLsizei, GLuint *) X(void, glBindBuffer, GLenum, GLuint) \
    X(void, glDeleteBuffers, GLsizei, const GLuint *) \
    X(void, glBufferData, GLenum, GLsizeiptr, const void *, GLenum) \
    X(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *) \
    X(void, glVertexAttribIPointer, GLuint, GLint, GLenum, GLsizei, const void *) \
    X(void, glEnableVertexAttribArray, GLuint) \
    X(void, glGenFramebuffers, GLsizei, GLuint *) X(void, glBindFramebuffer, GLenum, GLuint) \
    X(void, glDeleteFramebuffers, GLsizei, const GLuint *) \
    X(void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint) \
    X(GLenum, glCheckFramebufferStatus, GLenum) \
    X(void, glGenRenderbuffers, GLsizei, GLuint *) X(void, glBindRenderbuffer, GLenum, GLuint) \
    X(void, glDeleteRenderbuffers, GLsizei, const GLuint *) \
    X(void, glRenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei) \
    X(void, glFramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint)

#define VRGL_DECL(ret, name, ...) typedef ret(VRGL_APIENTRY *PFN_##name)(__VA_ARGS__); extern PFN_##name name;
VRGL_FUNCS(VRGL_DECL)
#undef VRGL_DECL

// getProc: platform lookup (wglGetProcAddress + opengl32.dll, or glXGetProcAddress).
bool vrglLoad(void *(*getProc)(const char *name), const char **missing);
