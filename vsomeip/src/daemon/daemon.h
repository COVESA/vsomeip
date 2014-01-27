//
// daemon.h
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef __VSOMEIP_DAEMON_H__
#define __VSOMEIP_DAEMON_H__

#include <string>
#include <vector>

namespace vsomeip {

class Daemon {

public:
	enum class LogLevel : uint8_t {
			Fatal = 0x0,
			Error = 0x1,
			Warn = 0x2,
			Info = 0x3,
			Debug = 0x4,
			Verbose = 0x5,
			Unknown = 0x6
	};

public:
	static Daemon * getInstance();

	void init(int a_argc, char **a_argv);
	void start();
	void stop();

private:
	Daemon();
	void log(LogLevel a_logLevel, const std::string& a_message);
	int accept();
	void run();

private:
	int m_port;
	bool m_isVirtualMode;
	bool m_isServiceDiscoveryMode;
	LogLevel m_logLevel;
	int m_socket;
};

} // namespace vsomeip

#endif /* DAEMON_H_ */
