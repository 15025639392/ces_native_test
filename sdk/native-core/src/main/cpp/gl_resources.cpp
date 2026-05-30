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
        uniform vec2 u_uvTranslation;
        uniform vec2 u_uvScale;
        varying vec2 v_texcoord;
        void main() {
            vec3 rel = a_position + u_originEye;
            vec3 camera = vec3(dot(rel, u_right), dot(rel, u_up), dot(rel, u_backward));
            v_texcoord = a_texcoord * u_uvScale + u_uvTranslation;
            gl_Position = u_projection * vec4(camera, 1.0);
        }
    )";
    constexpr const char* fragmentShader = R"(
        precision highp float;
        uniform sampler2D u_texture;
        uniform float u_alpha;
        varying vec2 v_texcoord;
        void main() {
            vec4 color = texture2D(u_texture, clamp(v_texcoord, 0.0, 1.0));
            gl_FragColor = vec4(color.rgb, color.a * u_alpha);
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

GLuint createTextureArrayProgram() {
    constexpr const char* vertexShader = R"(#version 300 es
        layout(location = 0) in vec3 a_position;
        layout(location = 1) in vec2 a_texcoord;
        layout(location = 2) in float a_layer;
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
        precision highp float;
        precision highp sampler2DArray;
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
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

} // namespace cesium_poc
