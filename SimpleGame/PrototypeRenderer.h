#pragma once

#include <string>
#include <unordered_map>
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
	void BeginFrame(float r, float g, float b, float a);
	void Present(float timeSeconds);
	void DrawQuad(const ScreenPoint& a, const ScreenPoint& b, const ScreenPoint& c, const ScreenPoint& d,
		float r, float g, float bColor, float alpha);
	void DrawRect(float x, float y, float width, float height, float r, float g, float b, float a);
	void DrawDiamond(float x, float y, float width, float height, float r, float g, float b, float a);
	void DrawString(float x, float y, const std::string& text, float r, float g, float b, float a);
	void DrawString(float x, float y, const std::wstring& text, float r, float g, float b, float a);
	void DrawSoftShadow(float x, float y, float width, float height, float strength);

private:
	bool ReadFile(const char* filename, std::string& result) const;
	GLuint CompileShader(GLenum type, const char* source);
	GLuint CreateProgram();
	GLuint CreatePostProgram();
	GLuint CreateBloomProgram();
	bool CreateSceneTarget();
	void DestroySceneTarget();
	bool InitializeFont();
	GLuint GetGlyph(wchar_t character);
	void DrawVertices(const float* vertices, int count, float r, float g, float b, float a);

	bool m_Initialized;
	int m_Width;
	int m_Height;
	GLuint m_Program;
	GLuint m_PostProgram;
	GLuint m_BloomProgram;
	GLuint m_VertexBuffer;
	GLuint m_SceneFramebuffer;
	GLuint m_SceneTexture;
	GLuint m_BloomFramebuffers[2];
	GLuint m_BloomTextures[2];
	GLint m_PositionAttribute;
	GLint m_ScreenSizeUniform;
	GLint m_ColorUniform;
	void* m_FontHandle;
	std::unordered_map<wchar_t, GLuint> m_Glyphs;
};
