#include "stdafx.h"
#include <Windows.h>
#include "PrototypeRenderer.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include "Dependencies\\freeglut.h"

PrototypeRenderer::PrototypeRenderer(int width, int height)
	: m_Initialized(false)
	, m_Width(width)
	, m_Height(height)
	, m_Program(0)
	, m_PostProgram(0)
	, m_BloomProgram(0)
	, m_SpaceProgram(0)
	, m_VertexBuffer(0)
	, m_SceneFramebuffer(0)
	, m_SceneTexture(0)
	, m_PositionAttribute(-1)
	, m_ScreenSizeUniform(-1)
	, m_ColorUniform(-1)
	, m_FontHandle(NULL)
	, m_DrawCallCount(0)
	, m_FrameNumber(0)
{
	m_BloomFramebuffers[0] = m_BloomFramebuffers[1] = 0;
	m_BloomTextures[0] = m_BloomTextures[1] = 0;
	m_Program = CreateProgram();
	m_PostProgram = CreatePostProgram();
	m_BloomProgram = CreateBloomProgram();
	m_SpaceProgram = CreateSpaceProgram();
	if (m_Program == 0 || m_PostProgram == 0 || m_BloomProgram == 0 || m_SpaceProgram == 0)
		return;

	glGenBuffers(1, &m_VertexBuffer);
	m_PositionAttribute = glGetAttribLocation(m_Program, "a_Position");
	m_ScreenSizeUniform = glGetUniformLocation(m_Program, "u_ScreenSize");
	m_ColorUniform = glGetUniformLocation(m_Program, "u_Color");
	if (m_VertexBuffer == 0 || m_PositionAttribute < 0 || m_ScreenSizeUniform < 0 ||
		m_ColorUniform < 0 || !CreateSceneTarget())
	{
		std::cerr << "Prototype renderer resource creation failed.\n";
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	InitializeFont();
	m_Initialized = true;
}

PrototypeRenderer::~PrototypeRenderer()
{
	for (std::unordered_map<wchar_t, GLuint>::const_iterator glyph = m_Glyphs.begin();
		glyph != m_Glyphs.end();
		++glyph)
		glDeleteLists(glyph->second, 1);
	if (m_FontHandle != NULL)
		DeleteObject(static_cast<HFONT>(m_FontHandle));
	DestroySceneTarget();
	if (m_VertexBuffer != 0)
		glDeleteBuffers(1, &m_VertexBuffer);
	if (m_Program != 0)
		glDeleteProgram(m_Program);
	if (m_PostProgram != 0)
		glDeleteProgram(m_PostProgram);
	if (m_BloomProgram != 0)
		glDeleteProgram(m_BloomProgram);
	if (m_SpaceProgram != 0)
		glDeleteProgram(m_SpaceProgram);
}

bool PrototypeRenderer::IsInitialized() const
{
	return m_Initialized;
}
int PrototypeRenderer::Width() const
{
	return m_Width;
}
int PrototypeRenderer::Height() const
{
	return m_Height;
}

unsigned int PrototypeRenderer::DrawCallCount() const
{
	return m_DrawCallCount;
}

void PrototypeRenderer::Resize(int width, int height)
{
	m_Width = width > 1 ? width : 1;
	m_Height = height > 1 ? height : 1;
	glViewport(0, 0, m_Width, m_Height);
	DestroySceneTarget();
	CreateSceneTarget();
}

void PrototypeRenderer::BeginFrame(float r, float g, float b, float a)
{
	m_DrawCallCount = 0;
	++m_FrameNumber;
	glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFramebuffer);
	glViewport(0, 0, m_Width, m_Height);
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PrototypeRenderer::Present(float timeSeconds)
{
	if (!m_Initialized)
		return;
	glDisable(GL_BLEND);
	const float vertices[] = {
		-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
	glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

	glUseProgram(m_BloomProgram);
	GLint bloomPosition = glGetAttribLocation(m_BloomProgram, "a_Position");
	glEnableVertexAttribArray(bloomPosition);
	glVertexAttribPointer(bloomPosition, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
	glUniform1i(glGetUniformLocation(m_BloomProgram, "u_Image"), 0);
	glUniform2f(glGetUniformLocation(m_BloomProgram, "u_Resolution"),
		static_cast<float>(m_Width),
		static_cast<float>(m_Height));
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffers[0]);
	glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
	glUniform2f(glGetUniformLocation(m_BloomProgram, "u_Direction"), 1.0f, 0.0f);
	glUniform1i(glGetUniformLocation(m_BloomProgram, "u_ExtractBright"), 1);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	++m_DrawCallCount;

	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffers[1]);
	glBindTexture(GL_TEXTURE_2D, m_BloomTextures[0]);
	glUniform2f(glGetUniformLocation(m_BloomProgram, "u_Direction"), 0.0f, 1.0f);
	glUniform1i(glGetUniformLocation(m_BloomProgram, "u_ExtractBright"), 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	++m_DrawCallCount;

	// A third, wider horizontal pass removes the cross-shaped two-pass bloom around small console
	// lights.
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffers[0]);
	glBindTexture(GL_TEXTURE_2D, m_BloomTextures[1]);
	glUniform2f(glGetUniformLocation(m_BloomProgram, "u_Direction"), 1.65f, 0.0f);
	glUniform1i(glGetUniformLocation(m_BloomProgram, "u_ExtractBright"), 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	++m_DrawCallCount;
	glDisableVertexAttribArray(bloomPosition);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_Width, m_Height);
	glUseProgram(m_PostProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
	glUniform1i(glGetUniformLocation(m_PostProgram, "u_Scene"), 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_BloomTextures[0]);
	glUniform1i(glGetUniformLocation(m_PostProgram, "u_Bloom"), 1);
	glUniform2f(glGetUniformLocation(m_PostProgram, "u_Resolution"),
		static_cast<float>(m_Width),
		static_cast<float>(m_Height));
	glUniform1f(glGetUniformLocation(m_PostProgram, "u_Time"), timeSeconds);

	GLint position = glGetAttribLocation(m_PostProgram, "a_Position");
	glEnableVertexAttribArray(position);
	glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	++m_DrawCallCount;
	glDisableVertexAttribArray(position);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_BLEND);

	std::cout << "\r[Frame " << m_FrameNumber << "] OpenGL draw calls: " << m_DrawCallCount
			  << "        " << std::flush;
}

void PrototypeRenderer::DrawQuad(const ScreenPoint& a,
	const ScreenPoint& b,
	const ScreenPoint& c,
	const ScreenPoint& d,
	float r,
	float g,
	float bColor,
	float alpha)
{
	const float vertices[] = {a.x, a.y, b.x, b.y, c.x, c.y, a.x, a.y, c.x, c.y, d.x, d.y};
	DrawVertices(vertices, 6, r, g, bColor, alpha);
}

void PrototypeRenderer::DrawRect(
	float x, float y, float width, float height, float r, float g, float b, float a)
{
	ScreenPoint p0 = {x - width * 0.5f, y - height * 0.5f};
	ScreenPoint p1 = {x + width * 0.5f, y - height * 0.5f};
	ScreenPoint p2 = {x + width * 0.5f, y + height * 0.5f};
	ScreenPoint p3 = {x - width * 0.5f, y + height * 0.5f};
	DrawQuad(p0, p1, p2, p3, r, g, b, a);
}

void PrototypeRenderer::DrawDiamond(
	float x, float y, float width, float height, float r, float g, float b, float a)
{
	ScreenPoint p0 = {x, y - height * 0.5f};
	ScreenPoint p1 = {x + width * 0.5f, y};
	ScreenPoint p2 = {x, y + height * 0.5f};
	ScreenPoint p3 = {x - width * 0.5f, y};
	DrawQuad(p0, p1, p2, p3, r, g, b, a);
}

void PrototypeRenderer::DrawEllipse(
	float x, float y, float width, float height, float r, float g, float b, float a)
{
	const int SegmentCount = 28;
	float vertices[SegmentCount * 6];
	int vertexOffset = 0;

	for (int segment = 0; segment < SegmentCount; ++segment)
	{
		float angleA = static_cast<float>(segment) / SegmentCount * 6.283185307f;
		float angleB = static_cast<float>(segment + 1) / SegmentCount * 6.283185307f;

		vertices[vertexOffset++] = x;
		vertices[vertexOffset++] = y;
		vertices[vertexOffset++] = x + std::cos(angleA) * width * 0.5f;
		vertices[vertexOffset++] = y + std::sin(angleA) * height * 0.5f;
		vertices[vertexOffset++] = x + std::cos(angleB) * width * 0.5f;
		vertices[vertexOffset++] = y + std::sin(angleB) * height * 0.5f;
	}

	DrawVertices(vertices, SegmentCount * 3, r, g, b, a);
}

void PrototypeRenderer::DrawSpaceBackground(float timeSeconds, float travelX, float travelY)
{
	if (!m_Initialized)
		return;

	const float vertices[] = {
		-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

	glDisable(GL_BLEND);
	glUseProgram(m_SpaceProgram);
	glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);

	GLint position = glGetAttribLocation(m_SpaceProgram, "a_Position");
	glEnableVertexAttribArray(position);
	glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
	glUniform2f(glGetUniformLocation(m_SpaceProgram, "u_Resolution"),
		static_cast<float>(m_Width),
		static_cast<float>(m_Height));
	glUniform1f(glGetUniformLocation(m_SpaceProgram, "u_Time"), timeSeconds);
	glUniform2f(glGetUniformLocation(m_SpaceProgram, "u_Travel"), travelX, travelY);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	++m_DrawCallCount;
	glDisableVertexAttribArray(position);
	glEnable(GL_BLEND);
}

void PrototypeRenderer::DrawSoftShadow(float x, float y, float width, float height, float strength)
{
	// More closely spaced layers create a smoother penumbra; the offset implies a shared overhead
	// light.
	for (int layer = 12; layer >= 1; --layer)
	{
		float normalized = (13.0f - static_cast<float>(layer)) / 12.0f;
		float spread = static_cast<float>(layer) * 1.65f;
		float alpha = strength * normalized * normalized * 0.075f;
		DrawDiamond(x - 8.0f - layer * 0.55f,
			y - 5.0f - layer * 0.12f,
			width + spread * 2.5f,
			height + spread,
			0.0f,
			0.0f,
			0.010f,
			alpha);
	}
	DrawDiamond(
		x - 5.0f, y - 2.0f, width * 0.78f, height * 0.68f, 0.0f, 0.0f, 0.006f, strength * 0.27f);
	DrawDiamond(
		x - 2.0f, y - 1.0f, width * 0.54f, height * 0.46f, 0.0f, 0.0f, 0.004f, strength * 0.20f);
}

void PrototypeRenderer::DrawVertices(
	const float* vertices, int count, float r, float g, float b, float a)
{
	if (!m_Initialized)
		return;
	glUseProgram(m_Program);
	glUniform2f(m_ScreenSizeUniform, static_cast<float>(m_Width), static_cast<float>(m_Height));
	glUniform4f(m_ColorUniform, r, g, b, a);
	glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2 * count, vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray(m_PositionAttribute);
	glVertexAttribPointer(m_PositionAttribute, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
	glDrawArrays(GL_TRIANGLES, 0, count);
	++m_DrawCallCount;
	glDisableVertexAttribArray(m_PositionAttribute);
}

void PrototypeRenderer::DrawString(
	float x, float y, const std::string& text, float r, float g, float b, float a)
{
	std::wstring wideText(text.begin(), text.end());
	DrawString(x, y, wideText, r, g, b, a);
}

void PrototypeRenderer::DrawString(
	float x, float y, const std::wstring& text, float r, float g, float b, float a)
{
	if (m_FontHandle == NULL)
		return;
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-m_Width * 0.5, m_Width * 0.5, -m_Height * 0.5, m_Height * 0.5, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glColor4f(r, g, b, a);
	glRasterPos2f(x, y);
	for (std::wstring::const_iterator character = text.begin(); character != text.end();
		++character)
	{
		GLuint glyph = GetGlyph(*character);
		if (glyph != 0)
		{
			glCallList(glyph);
			++m_DrawCallCount;
		}
	}
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

bool PrototypeRenderer::InitializeFont()
{
	HFONT font = CreateFontW(-18,
		0,
		0,
		0,
		FW_MEDIUM,
		FALSE,
		FALSE,
		FALSE,
		HANGUL_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY,
		FIXED_PITCH | FF_MODERN,
		L"Malgun Gothic");
	if (font == NULL)
		font = CreateFontW(-18,
			0,
			0,
			0,
			FW_MEDIUM,
			FALSE,
			FALSE,
			FALSE,
			DEFAULT_CHARSET,
			OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY,
			DEFAULT_PITCH,
			L"Arial");
	m_FontHandle = font;
	return font != NULL;
}

GLuint PrototypeRenderer::GetGlyph(wchar_t character)
{
	std::unordered_map<wchar_t, GLuint>::const_iterator existing = m_Glyphs.find(character);
	if (existing != m_Glyphs.end())
		return existing->second;
	if (m_FontHandle == NULL)
		return 0;

	HDC deviceContext = wglGetCurrentDC();
	if (deviceContext == NULL)
		return 0;
	HGDIOBJ previousFont = SelectObject(deviceContext, static_cast<HFONT>(m_FontHandle));
	GLuint list = glGenLists(1);
	if (list == 0 || !wglUseFontBitmapsW(deviceContext, static_cast<DWORD>(character), 1, list))
	{
		if (list != 0)
			glDeleteLists(list, 1);
		SelectObject(deviceContext, previousFont);
		return 0;
	}
	SelectObject(deviceContext, previousFont);
	m_Glyphs[character] = list;
	return list;
}

bool PrototypeRenderer::ReadFile(const char* filename, std::string& result) const
{
	std::ifstream file(filename);
	if (!file.is_open())
		return false;
	std::string line;
	while (std::getline(file, line))
	{
		result += line;
		result += "\n";
	}
	return true;
}

GLuint PrototypeRenderer::CompileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	if (shader == 0)
		return 0;
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint success = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLchar log[1024] = {0};
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		std::cerr << "Shader compilation failed: " << log << "\n";
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint PrototypeRenderer::CreateProgram()
{
	const char* vertexPaths[] = {"./Shaders/Prototype.vs",
		"./SimpleGame/Shaders/Prototype.vs",
		"../../SimpleGame/Shaders/Prototype.vs"};
	const char* fragmentPaths[] = {"./Shaders/Prototype.fs",
		"./SimpleGame/Shaders/Prototype.fs",
		"../../SimpleGame/Shaders/Prototype.fs"};
	std::string vertexSource, fragmentSource;
	for (int i = 0; i < 3 && vertexSource.empty(); ++i)
	{
		std::string candidateVertex, candidateFragment;
		if (ReadFile(vertexPaths[i], candidateVertex) &&
			ReadFile(fragmentPaths[i], candidateFragment))
		{
			vertexSource = candidateVertex;
			fragmentSource = candidateFragment;
		}
	}
	if (vertexSource.empty())
	{
		std::cerr << "Prototype shaders were not found.\n";
		return 0;
	}

	GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource.c_str());
	GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource.c_str());
	if (vertex == 0 || fragment == 0)
	{
		if (vertex != 0)
			glDeleteShader(vertex);
		if (fragment != 0)
			glDeleteShader(fragment);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLchar log[1024] = {0};
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cerr << "Shader linking failed: " << log << "\n";
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint PrototypeRenderer::CreatePostProgram()
{
	const char* vertexPaths[] = {"./Shaders/PrototypePost.vs",
		"./SimpleGame/Shaders/PrototypePost.vs",
		"../../SimpleGame/Shaders/PrototypePost.vs"};
	const char* fragmentPaths[] = {"./Shaders/PrototypePost.fs",
		"./SimpleGame/Shaders/PrototypePost.fs",
		"../../SimpleGame/Shaders/PrototypePost.fs"};
	std::string vertexSource, fragmentSource;
	for (int i = 0; i < 3 && vertexSource.empty(); ++i)
	{
		std::string candidateVertex, candidateFragment;
		if (ReadFile(vertexPaths[i], candidateVertex) &&
			ReadFile(fragmentPaths[i], candidateFragment))
		{
			vertexSource = candidateVertex;
			fragmentSource = candidateFragment;
		}
	}
	if (vertexSource.empty())
	{
		std::cerr << "Post-processing shaders were not found.\n";
		return 0;
	}

	GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource.c_str());
	GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource.c_str());
	if (vertex == 0 || fragment == 0)
	{
		if (vertex != 0)
			glDeleteShader(vertex);
		if (fragment != 0)
			glDeleteShader(fragment);
		return 0;
	}
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_FALSE)
	{
		GLchar log[1024] = {0};
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cerr << "Post-processing shader linking failed: " << log << "\n";
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint PrototypeRenderer::CreateBloomProgram()
{
	const char* vertexPaths[] = {"./Shaders/PrototypePost.vs",
		"./SimpleGame/Shaders/PrototypePost.vs",
		"../../SimpleGame/Shaders/PrototypePost.vs"};
	const char* fragmentPaths[] = {"./Shaders/PrototypeBloom.fs",
		"./SimpleGame/Shaders/PrototypeBloom.fs",
		"../../SimpleGame/Shaders/PrototypeBloom.fs"};
	std::string vertexSource, fragmentSource;
	for (int i = 0; i < 3 && vertexSource.empty(); ++i)
	{
		std::string candidateVertex, candidateFragment;
		if (ReadFile(vertexPaths[i], candidateVertex) &&
			ReadFile(fragmentPaths[i], candidateFragment))
		{
			vertexSource = candidateVertex;
			fragmentSource = candidateFragment;
		}
	}
	if (vertexSource.empty())
		return 0;
	GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource.c_str());
	GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource.c_str());
	if (vertex == 0 || fragment == 0)
	{
		if (vertex != 0)
			glDeleteShader(vertex);
		if (fragment != 0)
			glDeleteShader(fragment);
		return 0;
	}
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_FALSE)
	{
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLuint PrototypeRenderer::CreateSpaceProgram()
{
	const char* vertexPaths[] = {"./Shaders/PrototypeSpace.vs",
		"./SimpleGame/Shaders/PrototypeSpace.vs",
		"../../SimpleGame/Shaders/PrototypeSpace.vs"};
	const char* fragmentPaths[] = {"./Shaders/PrototypeSpace.fs",
		"./SimpleGame/Shaders/PrototypeSpace.fs",
		"../../SimpleGame/Shaders/PrototypeSpace.fs"};
	std::string vertexSource;
	std::string fragmentSource;

	for (int index = 0; index < 3 && vertexSource.empty(); ++index)
	{
		std::string candidateVertex;
		std::string candidateFragment;

		if (ReadFile(vertexPaths[index], candidateVertex) &&
			ReadFile(fragmentPaths[index], candidateFragment))
		{
			vertexSource = candidateVertex;
			fragmentSource = candidateFragment;
		}
	}

	if (vertexSource.empty())
		return 0;

	GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexSource.c_str());
	GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource.c_str());

	if (vertex == 0 || fragment == 0)
	{
		if (vertex != 0)
			glDeleteShader(vertex);
		if (fragment != 0)
			glDeleteShader(fragment);
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	GLint success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_FALSE)
	{
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

bool PrototypeRenderer::CreateSceneTarget()
{
	glGenFramebuffers(1, &m_SceneFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFramebuffer);
	glGenTextures(1, &m_SceneTexture);
	glBindTexture(GL_TEXTURE_2D, m_SceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneTexture, 0);
	bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	for (int i = 0; i < 2 && complete; ++i)
	{
		glGenFramebuffers(1, &m_BloomFramebuffers[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFramebuffers[i]);
		glGenTextures(1, &m_BloomTextures[i]);
		glBindTexture(GL_TEXTURE_2D, m_BloomTextures[i]);
		glTexImage2D(
			GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BloomTextures[i], 0);
		complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (!complete)
	{
		std::cerr << "Scene framebuffer creation failed.\n";
		DestroySceneTarget();
	}
	return complete;
}

void PrototypeRenderer::DestroySceneTarget()
{
	for (int i = 0; i < 2; ++i)
	{
		if (m_BloomTextures[i] != 0)
			glDeleteTextures(1, &m_BloomTextures[i]);
		if (m_BloomFramebuffers[i] != 0)
			glDeleteFramebuffers(1, &m_BloomFramebuffers[i]);
		m_BloomTextures[i] = 0;
		m_BloomFramebuffers[i] = 0;
	}
	if (m_SceneTexture != 0)
	{
		glDeleteTextures(1, &m_SceneTexture);
		m_SceneTexture = 0;
	}
	if (m_SceneFramebuffer != 0)
	{
		glDeleteFramebuffers(1, &m_SceneFramebuffer);
		m_SceneFramebuffer = 0;
	}
}
