#include "text.h"
#include "stb_truetype.h"
#include <SDL3/SDL.h>
#include <string.h>

#define ATLAS_W 512
#define ATLAS_H 512
#define FIRST_CHAR 32
#define NUM_CHARS 96

static stbtt_bakedchar cdata[NUM_CHARS];
static GLuint tex;
static GLuint prog, vao, vbo;

#ifdef __ANDROID__
#define GLSL_VERSION "#version 300 es\nprecision mediump float;\n"
#else
#define GLSL_VERSION "#version 330\n"
#endif

static const char *text_vert =
    GLSL_VERSION
    "layout(location=0) in vec4 vert;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(vert.xy, 0.0, 1.0);\n"
    "  uv = vert.zw;\n"
    "}\n";

static const char *text_frag =
    GLSL_VERSION
    "in vec2 uv;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D tex;\n"
    "uniform vec3 textColor;\n"
    "void main() {\n"
    "  float a = texture(tex, uv).r;\n"
    "  fragColor = vec4(textColor, a);\n"
    "}\n";

static GLuint mk_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

void text_init(const unsigned char *ttf_data, float pixel_height) {
    unsigned char *bitmap = SDL_malloc(ATLAS_W * ATLAS_H);
    stbtt_BakeFontBitmap(ttf_data, 0, pixel_height,
                         bitmap, ATLAS_W, ATLAS_H,
                         FIRST_CHAR, NUM_CHARS, cdata);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SDL_free(bitmap);

    GLuint vs = mk_shader(GL_VERTEX_SHADER, text_vert);
    GLuint fs = mk_shader(GL_FRAGMENT_SHADER, text_frag);
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 256, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
}

void text_draw(const char *str, float x, float y, float sx, float sy,
               float r, float g, float b) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(prog);
    glUniform3f(glGetUniformLocation(prog, "textColor"), r, g, b);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    float cx = x, cy = y;
    int len = (int)strlen(str);
    if (len > 256) len = 256;
    float verts[256 * 6 * 4];
    int n = 0;

    for (int i = 0; i < len; i++) {
        int c = (unsigned char)str[i] - FIRST_CHAR;
        if (c < 0 || c >= NUM_CHARS) { cx += 10.0f; continue; }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, ATLAS_W, ATLAS_H, c, &cx, &cy, &q, 1);

        float x0 = q.x0 * sx - 1.0f, y0 = 1.0f - q.y0 * sy;
        float x1 = q.x1 * sx - 1.0f, y1 = 1.0f - q.y1 * sy;

        float quad[] = {
            x0, y1, q.s0, q.t1,
            x0, y0, q.s0, q.t0,
            x1, y0, q.s1, q.t0,
            x0, y1, q.s0, q.t1,
            x1, y0, q.s1, q.t0,
            x1, y1, q.s1, q.t1,
        };
        SDL_memcpy(verts + n, quad, sizeof(quad));
        n += 24;
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, n * sizeof(float), verts);
    glDrawArrays(GL_TRIANGLES, 0, n / 4);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void text_cleanup(void) {
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
}
