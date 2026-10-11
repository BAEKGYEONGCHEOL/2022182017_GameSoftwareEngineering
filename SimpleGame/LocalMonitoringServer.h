#pragma once

#include <string>

class LocalMonitoringServer
{
  public:
	LocalMonitoringServer();
	~LocalMonitoringServer();

	bool Start(unsigned short port);
	void Stop();
	void SetStateJson(const std::string& stateJson);
	bool IsRunning() const;

  private:
	class Implementation;
	Implementation* m_Implementation;
};
