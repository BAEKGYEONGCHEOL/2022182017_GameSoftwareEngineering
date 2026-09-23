#include "stdafx.h"

#include <chrono>
#include <iostream>
#include <Windows.h>
#include "Dependencies\\glew.h"
#include "Dependencies\\freeglut.h"
#include "Game.h"
#include "PrototypeRenderer.h"

namespace
{
PrototypeRenderer* g_Renderer = NULL;
Game* g_Game = NULL;
int g_PreviousTime = 0;
float g_Accumulator = 0.0f;
const float FixedStep = 1.0f / 60.0f;
} // namespace

void RenderScene()
{
	if (g_Game != NULL)
		g_Game->Render();
	const std::chrono::steady_clock::time_point swapStart = std::chrono::steady_clock::now();
	glutSwapBuffers();
	const std::chrono::steady_clock::time_point swapEnd = std::chrono::steady_clock::now();
	if (g_Renderer != NULL)
	{
		double swapTimeMs = std::chrono::duration<double, std::milli>(swapEnd - swapStart).count();
		g_Renderer->SetSwapTimeMs(swapTimeMs);
	}
}

void Idle()
{
	int currentTime = glutGet(GLUT_ELAPSED_TIME);
	float elapsedSeconds = static_cast<float>(currentTime - g_PreviousTime) / 1000.0f;
	g_PreviousTime = currentTime;
	if (elapsedSeconds > 0.25f)
		elapsedSeconds = 0.25f;
	g_Accumulator += elapsedSeconds;

	const std::chrono::steady_clock::time_point updateStart = std::chrono::steady_clock::now();
	while (g_Accumulator >= FixedStep)
	{
		if (g_Game != NULL)
			g_Game->Update(FixedStep);
		g_Accumulator -= FixedStep;
	}
	const std::chrono::steady_clock::time_point updateEnd = std::chrono::steady_clock::now();
	if (g_Renderer != NULL)
	{
		double updateTimeMs =
			std::chrono::duration<double, std::milli>(updateEnd - updateStart).count();
		g_Renderer->SetUpdateTimeMs(updateTimeMs);
	}
	glutPostRedisplay();
}

void Reshape(int width, int height)
{
	if (g_Game != NULL)
		g_Game->Resize(width, height);
}

void KeyDown(unsigned char key, int, int)
{
	if (g_Game != NULL)
		g_Game->KeyDown(key);
}

void KeyUp(unsigned char key, int, int)
{
	if (g_Game != NULL)
		g_Game->KeyUp(key);
}

void SpecialDown(int key, int, int)
{
	if (g_Game != NULL)
		g_Game->SpecialDown(key);
}

void SpecialUp(int key, int, int)
{
	if (g_Game != NULL)
		g_Game->SpecialUp(key);
}

void MouseMove(int x, int y)
{
	if (g_Game != NULL)
		g_Game->MouseMove(x, y);
}

void MouseButton(int button, int state, int x, int y)
{
	if (g_Game != NULL)
	{
		g_Game->MouseMove(x, y);
		g_Game->MouseButton(button, state);
	}
}

void CloseGame()
{
	delete g_Game;
	g_Game = NULL;
	delete g_Renderer;
	g_Renderer = NULL;
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(80, 50);
	glutInitWindowSize(1100, 700);
	glutCreateWindow("NOCTIS - Tutorial Prototype");
	HWND gameWindow = GetForegroundWindow();
	if (gameWindow != NULL)
		SetWindowTextW(gameWindow, L"노크티스 - 튜토리얼 프로토타입");

	GLenum glewResult = glewInit();
	if (glewResult != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed: " << glewGetErrorString(glewResult) << "\n";
		return 1;
	}
	if (!GLEW_VERSION_3_3)
	{
		std::cerr << "OpenGL 3.3 is required.\n";
		return 1;
	}

	g_Renderer = new PrototypeRenderer(1100, 700);
	if (!g_Renderer->IsInitialized())
	{
		std::cerr << "Renderer could not be initialized.\n";
		delete g_Renderer;
		g_Renderer = NULL;
		return 1;
	}
	g_Game = new Game(g_Renderer);
	g_PreviousTime = glutGet(GLUT_ELAPSED_TIME);

	glutIgnoreKeyRepeat(1);
	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyDown);
	glutKeyboardUpFunc(KeyUp);
	glutSpecialFunc(SpecialDown);
	glutSpecialUpFunc(SpecialUp);
	glutPassiveMotionFunc(MouseMove);
	glutMotionFunc(MouseMove);
	glutMouseFunc(MouseButton);
	glutCloseFunc(CloseGame);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutMainLoop();
	CloseGame();
	return 0;
}
