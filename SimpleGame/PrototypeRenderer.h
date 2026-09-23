#pragma once

#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "Dependencies\\glew.h"

struct ScreenPoint
{
	float x;
	float y;
};

class PrototypeRenderer
{
  public:
	PrototypeRenderer(int width, int height);
	~PrototypeRenderer();

	bool IsInitialized() const;
	void Resize(int width, int height);
	int Width() const;
	int Height() const;
	unsigned int DrawCallCount() const;
	void SetUpdateTimeMs(double updateTimeMs);
	void SetSwapTimeMs(double swapTimeMs);
	void SetSceneMetrics(unsigned int visibleActors,
		unsigned int culledActors,
		unsigned int actorPoolSize,
		double transformTimeMs,
		double sortTimeMs,
		double actorRenderTimeMs);
	void BeginFrame(float r, float g, float b, float a);
	void Present(float timeSeconds);
	void DrawQuad(const ScreenPoint& a,
		const ScreenPoint& b,
		const ScreenPoint& c,
		const ScreenPoint& d,
		float r,
		float g,
		float bColor,
		float alpha);
	void DrawRect(float x, float y, float width, float height, float r, float g, float b, float a);
	void DrawDiamond(
		float x, float y, float width, float height, float r, float g, float b, float a);
	void DrawEllipse(
		float x, float y, float width, float height, float r, float g, float b, float a);
	void DrawSpaceBackground(float timeSeconds, float travelX, float travelY);
	void DrawString(float x, float y, const std::string& text, float r, float g, float b, float a);
	void DrawString(float x, float y, const std::wstring& text, float r, float g, float b, float a);
	void DrawSoftShadow(float x, float y, float width, float height, float strength);

  private:
	bool ReadFile(const char* filename, std::string& result) const;
	GLuint CompileShader(GLenum type, const char* source);
	GLuint CreateProgram();
	GLuint CreatePostProgram();
	GLuint CreateBloomProgram();
	GLuint CreateSpaceProgram();
	bool CreateSceneTarget();
	void DestroySceneTarget();
	bool InitializeFont();
	GLuint GetGlyph(wchar_t character);
	void DrawVertices(const float* vertices, int count, float r, float g, float b, float a);
	void FlushGeometryBatch();
	void FlushTextBatch();
	void BeginGpuTimer();
	void EndGpuTimer();
	void LogPerformance(double frameTimeMs, double renderBuildMs, double submitTimeMs);

	struct TextCommand
	{
		float x;
		float y;
		float r;
		float g;
		float b;
		float a;
		std::wstring text;
	};

	bool m_Initialized;
	int m_Width;
	int m_Height;
	GLuint m_Program;
	GLuint m_PostProgram;
	GLuint m_BloomProgram;
	GLuint m_SpaceProgram;
	GLuint m_VertexBuffer;
	GLuint m_SceneFramebuffer;
	GLuint m_SceneTexture;
	GLuint m_BloomFramebuffers[2];
	GLuint m_BloomTextures[2];
	GLint m_PositionAttribute;
	GLint m_ColorAttribute;
	GLint m_EffectCoordinateAttribute;
	GLint m_EffectTypeAttribute;
	GLint m_ScreenSizeUniform;
	void* m_FontHandle;
	std::unordered_map<wchar_t, GLuint> m_Glyphs;
	std::vector<float> m_BatchedVertices;
	std::vector<TextCommand> m_TextCommands;
	unsigned int m_DrawCallCount;
	unsigned int m_BatchCount;
	unsigned int m_VertexCount;
	unsigned int m_PrimitiveCount;
	unsigned int m_TextBatchCount;
	unsigned int m_VisibleActorCount;
	unsigned int m_CulledActorCount;
	unsigned int m_ActorPoolSize;
	unsigned long long m_FrameNumber;
	unsigned long long m_LastLogFrame;
	double m_UpdateTimeMs;
	double m_SwapTimeMs;
	double m_LastGpuTimeMs;
	double m_SceneTransformTimeMs;
	double m_SceneSortTimeMs;
	double m_ActorRenderTimeMs;
	GLuint m_GpuQueries[2];
	int m_GpuQueryIndex;
	bool m_GpuQueryPending[2];
	bool m_GpuQueryActive;
	long long m_FrameStartTicks;
	long long m_LastLogTicks;
	std::ofstream m_ProfileLog;
};
