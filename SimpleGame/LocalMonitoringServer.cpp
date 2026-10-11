#include "stdafx.h"
#include "LocalMonitoringServer.h"

#include <atomic>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
const char* FallbackDashboard =
	"<!doctype html><html lang='ko'><meta charset='utf-8'><title>NOCTIS Monitor</title>"
	"<body style='background:#071019;color:#dcebf2;font-family:sans-serif;padding:24px'>"
	"<h1>NOCTIS Monitoring</h1><p>dashboard.html을 불러오지 못했습니다.</p>"
	"<pre id='state'>연결 중...</pre><script>setInterval(async()=>{try{const r=await fetch('/api/state');"
	"document.querySelector('#state').textContent=JSON.stringify(await r.json(),null,2)}catch(e){}},500)</script>"
	"</body></html>";

std::string LoadDashboardHtml()
{
	const char* paths[] = {"Monitoring/dashboard.html", "SimpleGame/Monitoring/dashboard.html"};
	for (int pathIndex = 0; pathIndex < 2; ++pathIndex)
	{
		std::ifstream input(paths[pathIndex], std::ios::binary);
		if (!input)
			continue;
		std::ostringstream contents;
		contents << input.rdbuf();
		return contents.str();
	}
	return FallbackDashboard;
}

bool SendAll(SOCKET client, const std::string& response)
{
	size_t sentTotal = 0;
	while (sentTotal < response.size())
	{
		const int sent = send(client,
			response.data() + sentTotal,
			static_cast<int>(response.size() - sentTotal),
			0);
		if (sent <= 0)
			return false;
		sentTotal += static_cast<size_t>(sent);
	}
	return true;
}

std::string HttpResponse(const char* status,
	const char* contentType,
	const std::string& body,
	bool noCache)
{
	std::ostringstream response;
	response << "HTTP/1.1 " << status << "\r\n"
			 << "Content-Type: " << contentType << "\r\n"
			 << "Content-Length: " << body.size() << "\r\n"
			 << "Connection: close\r\n";
	if (noCache)
		response << "Cache-Control: no-store\r\n";
	response << "\r\n" << body;
	return response.str();
}
} // namespace

class LocalMonitoringServer::Implementation
{
  public:
	Implementation()
		: running(false), listenSocket(INVALID_SOCKET), winsockStarted(false), dashboardHtml(LoadDashboardHtml())
	{
		stateJson = "{\"schemaVersion\":1,\"status\":\"starting\",\"regions\":[],\"npcs\":[]}";
	}

	~Implementation()
	{
		Stop();
	}

	bool Start(unsigned short port)
	{
		if (running)
			return true;

		WSADATA data;
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			return false;
		winsockStarted = true;

		listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listenSocket == INVALID_SOCKET)
		{
			Stop();
			return false;
		}

		sockaddr_in address;
		std::memset(&address, 0, sizeof(address));
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
		if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
			listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			Stop();
			return false;
		}

		running = true;
		serverThread = std::thread(&Implementation::Run, this);
		std::cout << "MONITOR_URL http://127.0.0.1:" << port << "/\n";
		return true;
	}

	void Stop()
	{
		running = false;
		if (listenSocket != INVALID_SOCKET)
			shutdown(listenSocket, SD_BOTH);
		if (serverThread.joinable())
			serverThread.join();
		if (listenSocket != INVALID_SOCKET)
		{
			closesocket(listenSocket);
			listenSocket = INVALID_SOCKET;
		}
		if (winsockStarted)
		{
			WSACleanup();
			winsockStarted = false;
		}
	}

	void Run()
	{
		while (running)
		{
			fd_set readSet;
			FD_ZERO(&readSet);
			const SOCKET socketSnapshot = listenSocket;
			if (socketSnapshot == INVALID_SOCKET)
				break;
			FD_SET(socketSnapshot, &readSet);
			timeval timeout = {0, 200000};
			const int selected = select(0, &readSet, NULL, NULL, &timeout);
			if (!running)
				break;
			if (selected <= 0 || !FD_ISSET(socketSnapshot, &readSet))
				continue;

			SOCKET client = accept(socketSnapshot, NULL, NULL);
			if (client == INVALID_SOCKET)
				continue;
			HandleClient(client);
			shutdown(client, SD_BOTH);
			closesocket(client);
		}
	}

	void HandleClient(SOCKET client)
	{
		char requestBuffer[2048] = {};
		const int received = recv(client, requestBuffer, sizeof(requestBuffer) - 1, 0);
		if (received <= 0)
			return;
		const std::string request(requestBuffer, static_cast<size_t>(received));
		if (request.find("GET /api/state ") == 0)
		{
			std::string json;
			{
				std::lock_guard<std::mutex> lock(stateMutex);
				json = stateJson;
			}
			SendAll(client, HttpResponse("200 OK", "application/json; charset=utf-8", json, true));
			return;
		}
		if (request.find("GET / ") == 0 || request.find("GET /index.html ") == 0)
		{
			SendAll(client, HttpResponse("200 OK", "text/html; charset=utf-8", dashboardHtml, true));
			return;
		}
		SendAll(client, HttpResponse("404 Not Found", "application/json; charset=utf-8",
			"{\"error\":\"not_found\"}", true));
	}

	std::atomic<bool> running;
	SOCKET listenSocket;
	bool winsockStarted;
	std::thread serverThread;
	std::mutex stateMutex;
	std::string stateJson;
	std::string dashboardHtml;
};

LocalMonitoringServer::LocalMonitoringServer()
	: m_Implementation(new Implementation())
{
}

LocalMonitoringServer::~LocalMonitoringServer()
{
	delete m_Implementation;
}

bool LocalMonitoringServer::Start(unsigned short port)
{
	return m_Implementation->Start(port);
}

void LocalMonitoringServer::Stop()
{
	m_Implementation->Stop();
}

void LocalMonitoringServer::SetStateJson(const std::string& stateJson)
{
	std::lock_guard<std::mutex> lock(m_Implementation->stateMutex);
	m_Implementation->stateJson = stateJson;
}

bool LocalMonitoringServer::IsRunning() const
{
	return m_Implementation->running.load();
}
