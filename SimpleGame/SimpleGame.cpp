#include "stdafx.h"

#include <chrono>
#include <iostream>

#include "Dependencies\\glew.h"
#define GLFW_INCLUDE_NONE
#include "Dependencies\\GLFW\\include\\GLFW\\glfw3.h"
#include "Game.h"
#include "PrototypeRenderer.h"

namespace
{
PrototypeRenderer* g_Renderer = NULL;
Game* g_Game = NULL;
GLFWwindow* g_Window = NULL;
double g_PreviousTime = 0.0;
float g_Accumulator = 0.0f;
const float FixedStep = 1.0f / 60.0f;
const unsigned int MaximumFixedUpdatesPerFrame = 4;

void ErrorCallback(int errorCode, const char* description)
{
	std::cerr << "GLFW error " << errorCode << ": "
		<< (description != NULL ? description : "unknown error") << "\n";
}

int TranslateSpecialKey(int key)
{
	switch (key)
	{
		case GLFW_KEY_LEFT: return GameKeyLeft;
		case GLFW_KEY_UP: return GameKeyUp;
		case GLFW_KEY_RIGHT: return GameKeyRight;
		case GLFW_KEY_DOWN: return GameKeyDown;
		case GLFW_KEY_LEFT_SHIFT: return GameKeyLeftShift;
		case GLFW_KEY_RIGHT_SHIFT: return GameKeyRightShift;
		default: return -1;
	}
}

int TranslateCharacterKey(int key)
{
	if ((key >= GLFW_KEY_0 && key <= GLFW_KEY_9) ||
		(key >= GLFW_KEY_A && key <= GLFW_KEY_Z))
	{
		return key;
	}

	switch (key)
	{
		case GLFW_KEY_ESCAPE: return 27;
		case GLFW_KEY_SPACE: return ' ';
		case GLFW_KEY_TAB: return '\t';
		default: return -1;
	}
}

void KeyCallback(GLFWwindow*, int key, int, int action, int)
{
	if (g_Game == NULL || action == GLFW_REPEAT)
		return;

	const int specialKey = TranslateSpecialKey(key);
	if (specialKey >= 0)
	{
		if (action == GLFW_PRESS)
			g_Game->SpecialDown(specialKey);
		else if (action == GLFW_RELEASE)
			g_Game->SpecialUp(specialKey);
		return;
	}

	const int characterKey = TranslateCharacterKey(key);
	if (characterKey < 0)
		return;
	if (action == GLFW_PRESS)
		g_Game->KeyDown(static_cast<unsigned char>(characterKey));
	else if (action == GLFW_RELEASE)
		g_Game->KeyUp(static_cast<unsigned char>(characterKey));
}

void CursorPositionCallback(GLFWwindow* window, double x, double y)
{
	if (g_Game == NULL)
		return;

	int windowWidth = 1;
	int windowHeight = 1;
	int framebufferWidth = 1;
	int framebufferHeight = 1;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);
	glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
	const double scaleX = windowWidth > 0 ?
		static_cast<double>(framebufferWidth) / static_cast<double>(windowWidth) : 1.0;
	const double scaleY = windowHeight > 0 ?
		static_cast<double>(framebufferHeight) / static_cast<double>(windowHeight) : 1.0;
	g_Game->MouseMove(static_cast<int>(x * scaleX), static_cast<int>(y * scaleY));
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int)
{
	if (g_Game == NULL || button != GLFW_MOUSE_BUTTON_LEFT)
		return;

	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(window, &x, &y);
	CursorPositionCallback(window, x, y);
	g_Game->MouseButton(GameMouseLeftButton,
		action == GLFW_PRESS ? GameMouseButtonDown : GameMouseButtonUp);
}

void FramebufferSizeCallback(GLFWwindow*, int width, int height)
{
	if (g_Game != NULL && width > 0 && height > 0)
		g_Game->Resize(width, height);
}

void UpdateGame()
{
	const double currentTime = glfwGetTime();
	float elapsedSeconds = static_cast<float>(currentTime - g_PreviousTime);
	g_PreviousTime = currentTime;
	if (elapsedSeconds > 0.25f)
		elapsedSeconds = 0.25f;
	g_Accumulator += elapsedSeconds;

	const std::chrono::steady_clock::time_point updateStart =
		std::chrono::steady_clock::now();
	unsigned int fixedUpdateCount = 0;
	while (g_Accumulator >= FixedStep && fixedUpdateCount < MaximumFixedUpdatesPerFrame)
	{
		if (g_Game != NULL)
			g_Game->Update(FixedStep);
		g_Accumulator -= FixedStep;
		++fixedUpdateCount;
	}
	if (fixedUpdateCount == MaximumFixedUpdatesPerFrame && g_Accumulator >= FixedStep)
		g_Accumulator = 0.0f;
	const std::chrono::steady_clock::time_point updateEnd =
		std::chrono::steady_clock::now();
	if (g_Renderer != NULL)
	{
		const double updateTimeMs =
			std::chrono::duration<double, std::milli>(updateEnd - updateStart).count();
		g_Renderer->SetUpdateTimeMs(updateTimeMs);
		g_Renderer->SetFixedUpdateCount(fixedUpdateCount);
	}
}

void RenderGame()
{
	if (g_Game != NULL)
		g_Game->Render();
	const std::chrono::steady_clock::time_point swapStart =
		std::chrono::steady_clock::now();
	glfwSwapBuffers(g_Window);
	const std::chrono::steady_clock::time_point swapEnd =
		std::chrono::steady_clock::now();
	if (g_Renderer != NULL)
	{
		const double swapTimeMs =
			std::chrono::duration<double, std::milli>(swapEnd - swapStart).count();
		g_Renderer->SetSwapTimeMs(swapTimeMs);
	}
}

void CloseGame()
{
	delete g_Game;
	g_Game = NULL;
	delete g_Renderer;
	g_Renderer = NULL;
}
} // namespace

int main()
{
	glfwSetErrorCallback(ErrorCallback);
	if (glfwInit() != GLFW_TRUE)
	{
		std::cerr << "GLFW initialization failed.\n";
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	g_Window = glfwCreateWindow(
		1100, 700, "노크티스 - 튜토리얼 프로토타입", NULL, NULL);
	if (g_Window == NULL)
	{
		std::cerr << "GLFW window creation failed.\n";
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(g_Window, 80, 50);
	glfwMakeContextCurrent(g_Window);
	glfwSwapInterval(1);

	glewExperimental = GL_TRUE;
	const GLenum glewResult = glewInit();
	if (glewResult != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(glewResult) << "\n";
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		return 1;
	}
	while (glGetError() != GL_NO_ERROR)
	{
	}
	if (!GLEW_VERSION_3_3)
	{
		std::cerr << "OpenGL 3.3 is required.\n";
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		return 1;
	}

	int framebufferWidth = 0;
	int framebufferHeight = 0;
	glfwGetFramebufferSize(g_Window, &framebufferWidth, &framebufferHeight);
	g_Renderer = new PrototypeRenderer(framebufferWidth, framebufferHeight);
	if (!g_Renderer->IsInitialized())
	{
		std::cerr << "Renderer could not be initialized.\n";
		CloseGame();
		glfwDestroyWindow(g_Window);
		glfwTerminate();
		return 1;
	}

	g_Game = new Game(g_Renderer);
	g_Game->Resize(framebufferWidth, framebufferHeight);
	glfwSetKeyCallback(g_Window, KeyCallback);
	glfwSetCursorPosCallback(g_Window, CursorPositionCallback);
	glfwSetMouseButtonCallback(g_Window, MouseButtonCallback);
	glfwSetFramebufferSizeCallback(g_Window, FramebufferSizeCallback);
	g_PreviousTime = glfwGetTime();

	while (glfwWindowShouldClose(g_Window) == GLFW_FALSE)
	{
		glfwPollEvents();
		UpdateGame();
		RenderGame();
	}

	CloseGame();
	glfwDestroyWindow(g_Window);
	g_Window = NULL;
	glfwTerminate();
	return 0;
}
