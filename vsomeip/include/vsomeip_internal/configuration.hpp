//
// configuration.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_CONFIGURATION_HPP
#define VSOMEIP_INTERNAL_CONFIGURATION_HPP

#include <map>

namespace vsomeip {

class configuration {
public:
	static void init(int _option_count, char **_options);
	static configuration * request(const std::string &_name = "");
	static void release(const std::string &_name = "");

	~configuration();

	bool use_console_logger() const;
	bool use_file_logger() const;
	bool use_dlt_logger() const;
	const std::string & get_loglevel() const;
	const std::string & get_logfile_path() const;

	bool is_service_discovery_enabled() const;
	bool is_watchdog_enabled() const;
	bool is_endpoint_manager_enabled() const;

	uint16_t get_client_id() const;
	uint8_t get_slots() const;

	const std::string & get_protocol() const;
	const std::string & get_unicast_address() const;
	const std::string & get_multicast_address() const;
	const std::string & get_netmask() const;
	uint16_t get_port() const;

	uint32_t get_min_initial_delay() const;
	uint32_t get_max_initial_delay() const;
	uint32_t get_repetition_base_delay() const;
	uint8_t get_repetition_max() const;
	uint32_t get_ttl() const;
	uint32_t get_cyclic_offer_delay() const;
	uint32_t get_cyclic_request_delay() const;
	uint32_t get_request_response_delay() const;

private:
	configuration();
	void read_configuration(const std::string &);
	void read_loggers(const std::string &);
	void read_loglevel(const std::string &);

private:
	// Logging
	bool use_console_logger_;
	bool use_file_logger_;
	bool use_dlt_logger_;
	std::string loglevel_;
	std::string logfile_path_;

	// Daemon
	bool is_service_discovery_enabled_;

	// Application
	uint16_t client_id_;
	uint8_t slots_;
	bool is_watchdog_enabled_;
	bool is_endpoint_manager_enabled_;

	// Service Discovery
	std::string protocol_;
	std::string unicast_address_;
	std::string multicast_address_;
	std::string netmask_;
	uint16_t port_;
	uint32_t min_initial_delay_;
	uint32_t max_initial_delay_;
	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint32_t ttl_;
	uint32_t cyclic_offer_delay_;
	uint32_t cyclic_request_delay_;
	uint32_t request_response_delay_;

	uint8_t ref_;

	static std::string configuration_file_path_;
	static std::map< std::string, configuration * > configurations__;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CONFIGURATION_HPP
