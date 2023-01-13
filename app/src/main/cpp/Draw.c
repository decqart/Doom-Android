#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GLES2/gl2.h>

#include "Draw.h"

uint32_t *AScreenBuffer = 0;
size_t scrnX;
size_t scrnY;

void DrawRectangle(int x1, int y1, short x2, short y2, uint32_t color)
{
	int min_x = (int) fmin(x1, x2);
	int max_x = (int) fmax(x1, x2);
	int min_y = (int) fmin(y1, y2);
	int max_y = (int) fmax(y1, y2);

	if (min_x < 0) min_x = 0;
	if (min_y < 0) min_y = 0;
	if (max_x >= scrnX) max_x = scrnX - 1;
	if (max_y >= scrnY) max_y = scrnY - 1;

	int reset_x = min_x;

	while (min_y <= max_y)
	{
		unsigned int *bufferStart = &AScreenBuffer[min_y * scrnX + min_x];
		while (min_x <= max_x)
		{
			*bufferStart++ = color >> 8;
			min_x++;
		}
		min_x = reset_x;
		min_y++;
	}
}

void DrawCircle(int x, int y, int radius, uint32_t color)
{
    int xs = x;
	int x2 = x + 2*radius;
	int y2 = y + 2*radius;
    if (x < 0) xs = 0;

	for (int i = y; i <= y2; i++)
	{
        if (i < 0) continue;
		if (i >= scrnY) break;
		unsigned int *bufferStart = &AScreenBuffer[i * scrnX + xs];
		for (int j = x; j <= x2; j++)
		{
            if (j < 0) continue;
			if (j >= scrnX) break;
			double distance = sqrt((i-(radius+y))*(i-(radius+y)) + (j-(radius+x))*(j-(radius+x)));
			if (distance < radius+0.5)
				*bufferStart++ = color >> 8;
			else
				bufferStart++;
		}
	}
}

GLuint gRDBlitProg = 0;
GLuint gRDBlitProgUX = 0;
GLuint gRDBlitProgUT = 0;
GLuint gRDBlitProgTex = 0;
GLuint gRDLastResizeW;
GLuint gRDLastResizeH;

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

void SetupBatchInternal(void)
{
	gRDBlitProg = GLInternalLoadShader(
		"uniform vec4 xfrm;"
		"attribute vec3 a0;"
		"attribute vec4 a1;"
		"varying mediump vec2 tc;"
		"void main() { gl_Position = vec4(a0.xy*xfrm.xy+xfrm.zw, a0.z, 0.5); tc = a1.xy; }",

		"varying mediump vec2 tc;"
		"uniform sampler2D tex;"
		"void main() { gl_FragColor = texture2D(tex, tc).zyxw; }"
	);

	glUseProgram(gRDBlitProg);
	gRDBlitProgUX = glGetUniformLocation(gRDBlitProg, "xfrm");
	gRDBlitProgUT = glGetUniformLocation(gRDBlitProg, "tex");
	glGenTextures(1, &gRDBlitProgTex);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
}

void InternalResize(int x, int y)
{
	glViewport(0, 0, x, y);
	gRDLastResizeW = x;
	gRDLastResizeH = y;

	scrnX = x;
	scrnY = y;

	if (AScreenBuffer) free(AScreenBuffer);
	AScreenBuffer = malloc(scrnX * scrnY * 4);
	for(int i = 0; i < x * y; i++)
		AScreenBuffer[i] = 0x000000ff >> 8;
}

void DrawImage(uint32_t *data, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0) return;

	glUseProgram(gRDBlitProg);
	glUniform4f(gRDBlitProgUX, 1.0f/gRDLastResizeW,-1.0f/gRDLastResizeH,
		-0.5f+x/(float)gRDLastResizeW, 0.5f-y/(float)gRDLastResizeH);
	glUniform1i(gRDBlitProgUT, 0);

	glBindTexture(GL_TEXTURE_2D, gRDBlitProgTex);

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
	DrawRectangle(0, 0, scrnX, scrnY, 0x000000ff);
	glClearColor(0.0f, 0.0f, 0.0f, 255.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

int lastButtonX = -1;
int lastButtonY = -1;
int buttonDown = 0;
int lastMotionX = -1;
int lastMotionY = -1;
int lastButtonId = 0;
int lastMask = 0;

void HandleButton(int x, int y, int button, int bDown)
{
	lastButtonId = button;
	if (bDown)
	{
		lastButtonX = x; lastButtonY = y;
		lastMotionX = x; lastMotionY = y;
	}
	else
	{
		lastButtonX = -1;
		lastButtonY = -1;
	}
	buttonDown = bDown;
}

void HandleMotion(int x, int y, int mask)
{
	lastMask = mask;
	lastMotionX = x;
	lastMotionY = y;
}
