#include "gl_resources.h"

namespace cesium_poc {
namespace {

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

} // namespace

GLuint createProgram() {
    constexpr const char* vertexShader = R"(
        attribute vec3 a_position;
        attribute vec2 a_texcoord;
        uniform mat4 u_projection;
        uniform vec3 u_originEye;
        uniform vec3 u_right;
        uniform vec3 u_up;
        uniform vec3 u_backward;
        varying vec2 v_texcoord;
        void main() {
            vec3 rel = a_position + u_originEye;
            vec3 camera = vec3(dot(rel, u_right), dot(rel, u_up), dot(rel, u_backward));
            v_texcoord = a_texcoord;
            gl_Position = u_projection * vec4(camera, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(
        precision mediump float;
        uniform sampler2D u_texture;
        varying vec2 v_texcoord;
        void main() {
            if (v_texcoord.x < 0.0 || v_texcoord.x > 1.0 || v_texcoord.y < 0.0 || v_texcoord.y > 1.0) {
                discard;
            }
            gl_FragColor = texture2D(u_texture, v_texcoord);
        }
    )";

    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_texcoord");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLuint createBatchProgram() {
    constexpr const char* vertexShader = R"(#version 300 es
        in vec3 a_position;
        in vec2 a_texcoord;
        in float a_layer;
        uniform mat4 u_projection;
        uniform vec3 u_originEye;
        uniform vec3 u_right;
        uniform vec3 u_up;
        uniform vec3 u_backward;
        out vec2 v_texcoord;
        flat out int v_layer;
        void main() {
            vec3 rel = a_position + u_originEye;
            vec3 camera = vec3(dot(rel, u_right), dot(rel, u_up), dot(rel, u_backward));
            v_texcoord = a_texcoord;
            v_layer = int(a_layer + 0.5);
            gl_Position = u_projection * vec4(camera, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(#version 300 es
        precision mediump float;
        precision mediump sampler2DArray;
        uniform sampler2DArray u_textureArray;
        in vec2 v_texcoord;
        flat in int v_layer;
        out vec4 fragColor;
        void main() {
            if (v_texcoord.x < 0.0 || v_texcoord.x > 1.0 || v_texcoord.y < 0.0 || v_texcoord.y > 1.0) {
                discard;
            }
            fragColor = texture(u_textureArray, vec3(v_texcoord, float(v_layer)));
        }
    )";

    const GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShader);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_texcoord");
    glBindAttribLocation(program, 2, "a_layer");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

} // namespace cesium_poc
