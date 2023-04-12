#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GLES2/gl2.h>

#include "AndroidRenderer.h"

GLuint GLInternalLoadShader(const char *vertex_shader, const char *fragment_shader)
{
    GLuint fragment_shader_object = 0;
    GLuint vertex_shader_object;
    GLuint program = 0;
    int ret;

    vertex_shader_object = glCreateShader(GL_VERTEX_SHADER);
    if (!vertex_shader_object)
    {
        fprintf(stderr, "Error: glCreateShader(GL_VERTEX_SHADER) "
                        "failed: 0x%08X\n", glGetError());
        goto fail;
    }

    glShaderSource(vertex_shader_object, 1, &vertex_shader, NULL);
    glCompileShader(vertex_shader_object);

    glGetShaderiv(vertex_shader_object, GL_COMPILE_STATUS, &ret);
    if (!ret)
    {
        fprintf(stderr, "Error: vertex shader compilation failed!\n");
        glGetShaderiv(vertex_shader_object, GL_INFO_LOG_LENGTH, &ret);

        if (ret > 1)
        {
            char *log = malloc(ret);
            glGetShaderInfoLog(vertex_shader_object, ret, NULL, log);
            fprintf(stderr, "%s", log);
            free(log);
        }
        goto fail;
    }

    fragment_shader_object = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fragment_shader_object)
    {
        fprintf(stderr, "Error: glCreateShader(GL_FRAGMENT_SHADER) "
                        "failed: 0x%08X\n", glGetError());
        goto fail;
    }

    glShaderSource(fragment_shader_object, 1, &fragment_shader, NULL);
    glCompileShader(fragment_shader_object);

    glGetShaderiv(fragment_shader_object, GL_COMPILE_STATUS, &ret);
    if (!ret)
    {
        fprintf(stderr, "Error: fragment shader compilation failed!\n");
        glGetShaderiv(fragment_shader_object, GL_INFO_LOG_LENGTH, &ret);

        if (ret > 1)
        {
            char *log = malloc(ret);
            glGetShaderInfoLog(fragment_shader_object, ret, NULL, log);
            fprintf(stderr, "%s", log);
            free(log);
        }
        goto fail;
    }

    program = glCreateProgram();
    if (!program)
    {
        fprintf(stderr, "Error: failed to create program!\n");
        goto fail;
    }

    glAttachShader(program, vertex_shader_object);
    glAttachShader(program, fragment_shader_object);

    glBindAttribLocation(program, 0, "a0");
    glBindAttribLocation(program, 1, "a1");

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &ret);
    if (!ret)
    {
        fprintf(stderr, "Error: program linking failed!\n");
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

        if (ret > 1)
        {
            char *log = malloc(ret);
            glGetProgramInfoLog(program, ret, NULL, log);
            fprintf(stderr, "%s", log);
            free(log);
        }
        goto fail;
    }
    return program;
fail:
    if (!vertex_shader_object) glDeleteShader(vertex_shader_object);
    if (!fragment_shader_object) glDeleteShader(fragment_shader_object);
    if (!program) glDeleteShader(program);
    return -1;
}

GLuint imageProgram = 0;
GLint imageProgramUX = 0;
GLint imageProgramUT = 0;
GLuint imageProgramTex = 0;
GLuint circleProgram = -1;
GLint circleProgramUX = -1;
GLint circleProgramScrn = -1;
GLint circleProgramLoc = -1;
GLint circleProgramRad = -1;
GLuint lastWidthResize;
GLuint lastHeightResize;

void SetupBatchInternal(void)
{
    circleProgram = GLInternalLoadShader(
            "#version 100\n"
            "uniform vec4 xfrm;"
            "attribute vec3 a0;"
            "attribute vec4 a1;"
            "varying lowp vec4 vc;"
            "void main() { gl_Position = vec4(a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5); vc = a1; }",

            "#version 100\n"
            "precision mediump float;"
            "varying lowp vec4 vc;"
            "uniform vec2 uRes;"
            "uniform vec2 uLoc;"
            "uniform float uRad;"
            "void main() {"
            "    vec2 uv = (gl_FragCoord.xy / uRes - vec2(0.0, 1.0) + uLoc) * 2.0;\n"
            "    float aspect = uRes.x / uRes.y;"
            "    if (max(uRes.x, uRes.y) == uRes.y)"
            "        uv.y /= aspect;"
            "    else { uv.x *= aspect; }"
            "    float distance = uRad/545.0 - length(uv);"
            "    float alphaColour;"
            "    if (distance > 0.0) {"
            "        distance = 1.0;"
            "        alphaColour = 1.0;"
            "    } else { alphaColour = 0.0; }"
            "    gl_FragColor = vec4(vec3(distance), alphaColour);"
            "    gl_FragColor.rgb = vc.abg;"
            "}"
    );

    glUseProgram(circleProgram);
    circleProgramUX = glGetUniformLocation(circleProgram, "xfrm");
    circleProgramScrn = glGetUniformLocation(circleProgram, "uRes");
    circleProgramLoc = glGetUniformLocation(circleProgram, "uLoc");
    circleProgramRad = glGetUniformLocation(circleProgram, "uRad");

    imageProgram = GLInternalLoadShader(
            "uniform vec4 xfrm;"
            "attribute vec3 a0;"
            "attribute vec4 a1;"
            "varying mediump vec2 tc;"
            "void main() { gl_Position = vec4(a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5); tc = a1.xy; }",

            "varying mediump vec2 tc;"
            "uniform sampler2D tex;"
            "void main() { gl_FragColor = vec4(texture2D(tex, tc).zyx, 1.0); }"
            );

    glUseProgram(imageProgram);
    imageProgramUX = glGetUniformLocation(imageProgram, "xfrm");
    imageProgramUT = glGetUniformLocation(imageProgram, "tex");
    glGenTextures(1, &imageProgramTex);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void InternalResize(int x, int y)
{
    glViewport(0, 0, x, y);
    lastWidthResize = x;
    lastHeightResize = y;
}

void RenderCircle(int x, int y, float radius, uint32_t color)
{
    glUseProgram(circleProgram);
    glUniform4f(circleProgramUX, 1.0f / lastWidthResize, -1.0f / lastHeightResize, -0.5f, 0.5f);
    glUniform2f(circleProgramScrn, (float) lastWidthResize, (float) lastHeightResize);
    float ux = (float) x+radius;
    float uy = (float) y+radius;
    glUniform2f(circleProgramLoc, -ux / lastWidthResize, uy / lastHeightResize);
    glUniform1f(circleProgramRad, radius);

    float x1 = (float) x, y1 = (float) y;
    float x2 = (float) x+radius*2, y2 = (float) y+radius*2;

    float fv[18];
    for (int i = 0; i < 18; ++i) fv[i] = 0.0f; // we need to fill with 0 because it causes errors
    fv[0] = x1; fv[1] = y1;
    fv[3] = x2; fv[4] = y1;
    fv[6] = x1; fv[7] = y2;
    fv[9] = x1; fv[10] = y2;
    fv[12] = x2; fv[13] = y1;
    fv[15] = x2; fv[16] = y2;

    uint32_t col[6];
    for (int i = 0; i < 6; ++i) col[i] = color;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, fv);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, col);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void RenderImage(uint32_t *data, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;

    glUseProgram(imageProgram);
    glUniform4f(imageProgramUX, 1.0f / lastWidthResize, -1.0f / lastHeightResize,
        -0.5f+x/(float)lastWidthResize, 0.5f - y / (float)lastHeightResize);
    glUniform1i(imageProgramUT, 0);

    glBindTexture(GL_TEXTURE_2D, imageProgramTex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    const float verts[] = {
            0, 0, w, 0, w, h,
            0, 0, w, h, 0, h
    };
    const uint8_t colors[] = {
            0, 0, 255, 0, 255, 255,
            0, 0, 255, 255, 0, 255
    };

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_TRUE, 0, colors);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void ClearFrame(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
