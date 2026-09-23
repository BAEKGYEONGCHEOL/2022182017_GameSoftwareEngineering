#include "stdafx.h"
#include <Windows.h>
#include "PrototypeRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "Dependencies\\freeglut.h"

namespace
{
long long PerformanceTicks()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

double TicksToMilliseconds(long long ticks)
{
	return static_cast<double>(ticks) / 1000000.0;
}
} // namespace

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
	, m_ColorAttribute(-1)
	, m_EffectCoordinateAttribute(-1)
	, m_EffectTypeAttribute(-1)
	, m_ScreenSizeUniform(-1)
	, m_FontHandle(NULL)
	, m_DrawCallCount(0)
	, m_BatchCount(0)
	, m_VertexCount(0)
	, m_PrimitiveCount(0)
	, m_TextBatchCount(0)
	, m_VisibleActorCount(0)
	, m_CulledActorCount(0)
	, m_ActorPoolSize(0)
	, m_FrameNumber(0)
	, m_LastLogFrame(0)
	, m_UpdateTimeMs(0.0)
	, m_SwapTimeMs(0.0)
	, m_LastGpuTimeMs(0.0)
	, m_SceneTransformTimeMs(0.0)
	, m_SceneSortTimeMs(0.0)
	, m_ActorRenderTimeMs(0.0)
	, m_GpuQueryIndex(0)
	, m_GpuQueryActive(false)
	, m_FrameStartTicks(0)
	, m_LastLogTicks(0)
{
	m_BatchedVertices.reserve(262144);
	m_TextCommands.reserve(64);
	m_ProfileLog.open("PerformanceProfile.log", std::ios::out | std::ios::trunc);
	if (m_ProfileLog.is_open())
	{
		m_ProfileLog << "PERF_SCHEMA version=1 units=time_ms,count,bytes "
						"keys=frame,fps,frame_ms,cpu_update_ms,cpu_render_build_ms,cpu_swap_ms,"
						"scene_transform_ms,scene_sort_ms,actor_render_ms,cpu_gpu_submit_ms,"
						"gpu_frame_ms,draw_calls,batches,primitives,vertices,"
						"text_batches,buffer_upload_bytes,visible_actors,culled_actors,"
						"actor_pool_size,bottleneck_hint\n";
	}
	m_BloomFramebuffers[0] = m_BloomFramebuffers[1] = 0;
	m_BloomTextures[0] = m_BloomTextures[1] = 0;
	m_GpuQueries[0] = m_GpuQueries[1] = 0;
	m_GpuQueryPending[0] = m_GpuQueryPending[1] = false;
	m_Program = CreateProgram();
	m_PostProgram = CreatePostProgram();
	m_BloomProgram = CreateBloomProgram();
	m_SpaceProgram = CreateSpaceProgram();
	if (m_Program == 0 || m_PostProgram == 0 || m_BloomProgram == 0 || m_SpaceProgram == 0)
		return;

	glGenBuffers(1, &m_VertexBuffer);
	m_PositionAttribute = glGetAttribLocation(m_Program, "a_Position");
	m_ColorAttribute = glGetAttribLocation(m_Program, "a_Color");
	m_EffectCoordinateAttribute = glGetAttribLocation(m_Program, "a_EffectCoordinate");
	m_EffectTypeAttribute = glGetAttribLocation(m_Program, "a_EffectType");
	m_ScreenSizeUniform = glGetUniformLocation(m_Program, "u_ScreenSize");
	if (m_VertexBuffer == 0 || m_PositionAttribute < 0 || m_ColorAttribute < 0 ||
		m_EffectCoordinateAttribute < 0 || m_EffectTypeAttribute < 0 || m_ScreenSizeUniform < 0 ||
		!CreateSceneTarget())
	{
		std::cerr << "Prototype renderer resource creation failed.\n";
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glGenQueries(2, m_GpuQueries);
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
	if (m_GpuQueries[0] != 0 || m_GpuQueries[1] != 0)
		glDeleteQueries(2, m_GpuQueries);
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

void PrototypeRenderer::SetUpdateTimeMs(double updateTimeMs)
{
	m_UpdateTimeMs = updateTimeMs;
}

void PrototypeRenderer::SetSwapTimeMs(double swapTimeMs)
{
	m_SwapTimeMs = swapTimeMs;
}

void PrototypeRenderer::SetSceneMetrics(unsigned int visibleActors,
	unsigned int culledActors,
	unsigned int actorPoolSize,
	double transformTimeMs,
	double sortTimeMs,
	double actorRenderTimeMs)
{
	m_VisibleActorCount += visibleActors;
	m_CulledActorCount += culledActors;
	m_ActorPoolSize = m_ActorPoolSize > actorPoolSize ? m_ActorPoolSize : actorPoolSize;
	m_SceneTransformTimeMs += transformTimeMs;
	m_SceneSortTimeMs += sortTimeMs;
	m_ActorRenderTimeMs += actorRenderTimeMs;
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
	m_FrameStartTicks = PerformanceTicks();
	m_DrawCallCount = 0;
	m_BatchCount = 0;
	m_VertexCount = 0;
	m_PrimitiveCount = 0;
	m_TextBatchCount = 0;
	m_VisibleActorCount = 0;
	m_CulledActorCount = 0;
	m_ActorPoolSize = 0;
	m_SceneTransformTimeMs = 0.0;
	m_SceneSortTimeMs = 0.0;
	m_ActorRenderTimeMs = 0.0;
	m_BatchedVertices.clear();
	m_TextCommands.clear();
	++m_FrameNumber;
	glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFramebuffer);
	glViewport(0, 0, m_Width, m_Height);
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	BeginGpuTimer();
}

void PrototypeRenderer::Present(float timeSeconds)
{
	if (!m_Initialized)
		return;
	const long long submitStart = PerformanceTicks();
	const double renderBuildMs = TicksToMilliseconds(submitStart - m_FrameStartTicks);
	FlushGeometryBatch();
	FlushTextBatch();
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
	const int bloomWidth = m_Width / 2 > 1 ? m_Width / 2 : 1;
	const int bloomHeight = m_Height / 2 > 1 ? m_Height / 2 : 1;
	glUniform2f(glGetUniformLocation(m_BloomProgram, "u_Resolution"),
		static_cast<float>(bloomWidth),
		static_cast<float>(bloomHeight));
	glActiveTexture(GL_TEXTURE0);
	glViewport(0, 0, bloomWidth, bloomHeight);

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
	EndGpuTimer();

	const long long frameEnd = PerformanceTicks();
	const double submitTimeMs = TicksToMilliseconds(frameEnd - submitStart);
	const double frameTimeMs = TicksToMilliseconds(frameEnd - m_FrameStartTicks);
	LogPerformance(frameTimeMs, renderBuildMs, submitTimeMs);
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

	FlushGeometryBatch();
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
	const float halfWidth = (width + 16.0f) * 0.5f;
	const float halfHeight = (height + 9.0f) * 0.5f;
	const float centerX = x - 5.0f;
	const float centerY = y - 3.0f;
	const float positions[] = {centerX - halfWidth,
		centerY - halfHeight,
		centerX + halfWidth,
		centerY - halfHeight,
		centerX + halfWidth,
		centerY + halfHeight,
		centerX - halfWidth,
		centerY - halfHeight,
		centerX + halfWidth,
		centerY + halfHeight,
		centerX - halfWidth,
		centerY + halfHeight};
	const float effectCoordinates[] = {
		-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
	m_BatchedVertices.reserve(m_BatchedVertices.size() + 54);
	for (int index = 0; index < 6; ++index)
	{
		m_BatchedVertices.push_back(positions[index * 2]);
		m_BatchedVertices.push_back(positions[index * 2 + 1]);
		m_BatchedVertices.push_back(0.0f);
		m_BatchedVertices.push_back(0.0f);
		m_BatchedVertices.push_back(0.008f);
		m_BatchedVertices.push_back(strength * 0.34f);
		m_BatchedVertices.push_back(effectCoordinates[index * 2]);
		m_BatchedVertices.push_back(effectCoordinates[index * 2 + 1]);
		m_BatchedVertices.push_back(1.0f);
	}
	++m_PrimitiveCount;
}

void PrototypeRenderer::DrawVertices(
	const float* vertices, int count, float r, float g, float b, float a)
{
	if (!m_Initialized)
		return;
	m_BatchedVertices.reserve(m_BatchedVertices.size() + static_cast<size_t>(count) * 9);
	for (int index = 0; index < count; ++index)
	{
		m_BatchedVertices.push_back(vertices[index * 2]);
		m_BatchedVertices.push_back(vertices[index * 2 + 1]);
		m_BatchedVertices.push_back(r);
		m_BatchedVertices.push_back(g);
		m_BatchedVertices.push_back(b);
		m_BatchedVertices.push_back(a);
		m_BatchedVertices.push_back(0.0f);
		m_BatchedVertices.push_back(0.0f);
		m_BatchedVertices.push_back(0.0f);
	}
	++m_PrimitiveCount;
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
	TextCommand command = {x, y, r, g, b, a, text};
	m_TextCommands.push_back(command);
}

void PrototypeRenderer::FlushGeometryBatch()
{
	if (m_BatchedVertices.empty())
		return;

	const GLsizei stride = static_cast<GLsizei>(sizeof(float) * 9);
	const GLsizei vertexCount = static_cast<GLsizei>(m_BatchedVertices.size() / 9);
	glUseProgram(m_Program);
	glUniform2f(m_ScreenSizeUniform, static_cast<float>(m_Width), static_cast<float>(m_Height));
	glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
	glBufferData(GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(m_BatchedVertices.size() * sizeof(float)),
		&m_BatchedVertices[0],
		GL_STREAM_DRAW);
	glEnableVertexAttribArray(m_PositionAttribute);
	glEnableVertexAttribArray(m_ColorAttribute);
	glEnableVertexAttribArray(m_EffectCoordinateAttribute);
	glEnableVertexAttribArray(m_EffectTypeAttribute);
	glVertexAttribPointer(m_PositionAttribute, 2, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(m_ColorAttribute,
		4,
		GL_FLOAT,
		GL_FALSE,
		stride,
		reinterpret_cast<const void*>(sizeof(float) * 2));
	glVertexAttribPointer(m_EffectCoordinateAttribute,
		2,
		GL_FLOAT,
		GL_FALSE,
		stride,
		reinterpret_cast<const void*>(sizeof(float) * 6));
	glVertexAttribPointer(m_EffectTypeAttribute,
		1,
		GL_FLOAT,
		GL_FALSE,
		stride,
		reinterpret_cast<const void*>(sizeof(float) * 8));
	glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	glDisableVertexAttribArray(m_EffectTypeAttribute);
	glDisableVertexAttribArray(m_EffectCoordinateAttribute);
	glDisableVertexAttribArray(m_ColorAttribute);
	glDisableVertexAttribArray(m_PositionAttribute);
	++m_DrawCallCount;
	++m_BatchCount;
	m_VertexCount += static_cast<unsigned int>(vertexCount);
	m_BatchedVertices.clear();
}

void PrototypeRenderer::FlushTextBatch()
{
	if (m_TextCommands.empty() || m_FontHandle == NULL)
		return;

	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-m_Width * 0.5, m_Width * 0.5, -m_Height * 0.5, m_Height * 0.5, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	for (size_t commandIndex = 0; commandIndex < m_TextCommands.size(); ++commandIndex)
	{
		const TextCommand& command = m_TextCommands[commandIndex];
		std::vector<GLuint> glyphLists;
		glyphLists.reserve(command.text.size());
		for (std::wstring::const_iterator character = command.text.begin();
			character != command.text.end();
			++character)
		{
			GLuint glyph = GetGlyph(*character);
			if (glyph != 0)
				glyphLists.push_back(glyph);
		}
		if (glyphLists.empty())
			continue;

		glColor4f(command.r, command.g, command.b, command.a);
		glRasterPos2f(command.x, command.y);
		glListBase(0);
		glCallLists(static_cast<GLsizei>(glyphLists.size()), GL_UNSIGNED_INT, &glyphLists[0]);
		++m_DrawCallCount;
		++m_BatchCount;
		++m_TextBatchCount;
	}
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	m_TextCommands.clear();
}

void PrototypeRenderer::BeginGpuTimer()
{
	m_GpuQueryActive = false;
	if (m_GpuQueries[m_GpuQueryIndex] != 0 && !m_GpuQueryPending[m_GpuQueryIndex])
	{
		glBeginQuery(GL_TIME_ELAPSED, m_GpuQueries[m_GpuQueryIndex]);
		m_GpuQueryActive = true;
	}
}

void PrototypeRenderer::EndGpuTimer()
{
	if (m_GpuQueryActive)
	{
		glEndQuery(GL_TIME_ELAPSED);
		m_GpuQueryPending[m_GpuQueryIndex] = true;
		m_GpuQueryActive = false;
	}

	for (int queryIndex = 0; queryIndex < 2; ++queryIndex)
	{
		if (m_GpuQueryPending[queryIndex])
		{
			GLint available = GL_FALSE;
			glGetQueryObjectiv(m_GpuQueries[queryIndex], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available == GL_TRUE)
			{
				GLuint64 elapsedNanoseconds = 0;
				glGetQueryObjectui64v(
					m_GpuQueries[queryIndex], GL_QUERY_RESULT, &elapsedNanoseconds);
				m_LastGpuTimeMs = static_cast<double>(elapsedNanoseconds) / 1000000.0;
				m_GpuQueryPending[queryIndex] = false;
			}
		}
	}
	m_GpuQueryIndex = 1 - m_GpuQueryIndex;
}

void PrototypeRenderer::LogPerformance(
	double frameTimeMs, double renderBuildMs, double submitTimeMs)
{
	const long long currentTicks = PerformanceTicks();
	if (m_LastLogTicks == 0)
	{
		m_LastLogTicks = currentTicks;
		m_LastLogFrame = m_FrameNumber;
		return;
	}

	const double logElapsedSeconds =
		static_cast<double>(currentTicks - m_LastLogTicks) / 1000000000.0;
	if (logElapsedSeconds < 0.5)
		return;

	const unsigned long long framesSinceLog = m_FrameNumber - m_LastLogFrame;
	const double framesPerSecond = framesSinceLog / logElapsedSeconds;
	const char* bottleneckHint = "balanced";
	if (m_SwapTimeMs > m_UpdateTimeMs && m_SwapTimeMs > renderBuildMs &&
		m_SwapTimeMs > m_LastGpuTimeMs)
		bottleneckHint = "swap_or_vsync";
	else if (m_UpdateTimeMs > renderBuildMs && m_UpdateTimeMs > m_LastGpuTimeMs)
		bottleneckHint = "cpu_update";
	else if (renderBuildMs > m_LastGpuTimeMs)
		bottleneckHint = "cpu_render_build";
	else if (m_LastGpuTimeMs > 0.0)
		bottleneckHint = "gpu_frame";
	std::ostringstream logLine;
	logLine << std::fixed << std::setprecision(2) << "PERF_FRAME"
			<< " frame=" << m_FrameNumber << " fps=" << framesPerSecond
			<< " frame_ms=" << frameTimeMs << " cpu_update_ms=" << m_UpdateTimeMs
			<< " cpu_render_build_ms=" << renderBuildMs << " cpu_swap_ms=" << m_SwapTimeMs
			<< " scene_transform_ms=" << m_SceneTransformTimeMs
			<< " scene_sort_ms=" << m_SceneSortTimeMs << " actor_render_ms=" << m_ActorRenderTimeMs
			<< " cpu_gpu_submit_ms=" << submitTimeMs << " gpu_frame_ms=" << m_LastGpuTimeMs
			<< " draw_calls=" << m_DrawCallCount << " batches=" << m_BatchCount
			<< " primitives=" << m_PrimitiveCount << " vertices=" << m_VertexCount
			<< " text_batches=" << m_TextBatchCount
			<< " buffer_upload_bytes=" << m_VertexCount * sizeof(float) * 9
			<< " visible_actors=" << m_VisibleActorCount << " culled_actors=" << m_CulledActorCount
			<< " actor_pool_size=" << m_ActorPoolSize << " bottleneck_hint=" << bottleneckHint;
	std::cout << logLine.str() << '\n';
	if (m_ProfileLog.is_open())
	{
		m_ProfileLog << logLine.str() << '\n';
		m_ProfileLog.flush();
	}
	m_LastLogFrame = m_FrameNumber;
	m_LastLogTicks = currentTicks;
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
	const int bloomWidth = m_Width / 2 > 1 ? m_Width / 2 : 1;
	const int bloomHeight = m_Height / 2 > 1 ? m_Height / 2 : 1;
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
		glTexImage2D(GL_TEXTURE_2D,
			0,
			GL_RGBA8,
			bloomWidth,
			bloomHeight,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			NULL);
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
