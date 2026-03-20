// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// This application connects to a specific TCP server, sends a subscription
// and then verifies that it has received a notification from the server. It
// performs this operation, in parallel and in series, a number of times
// specified in the arguments used to launch the application. At the end, it
// returns the number of failures, which should be 0.

#define SOMEIP_SD_PORT 30490
#define TCP_SERVER_PORT 34511
#define SERVICE_ID 0x1111
#define INSTANCE_ID 0x0001
#define EVENTGROUP_ID 0x1000

static std::string server_ip_addr;
static std::string client_ip_addr;

static bool send_subscribe(uint16_t port) {
    static std::mutex sync;
    static uint16_t session_id{1};
    static int udp_client_socket{-1};
    static struct sockaddr_in server_addr { };
    static struct sockaddr_in client_addr { };
    std::lock_guard lock{sync};

    if (port == 0) {
        if (udp_client_socket > 0) {
            ::close(udp_client_socket);
            udp_client_socket = -1;
        }
        return false;
    }
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(SOMEIP_SD_PORT);
    inet_pton(AF_INET, client_ip_addr.c_str(), &client_addr.sin_addr);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SOMEIP_SD_PORT);
    inet_pton(AF_INET, server_ip_addr.c_str(), &server_addr.sin_addr);

    uint32_t ip_le = ntohl(client_addr.sin_addr.s_addr);

    std::uint8_t message[] = {// Service discovery / Method
                              0xFF, 0xFF, 0x81, 0x00,
                              // Message size
                              0x00, 0x00, 0x00, 0x30,
                              // Client / Session
                              0x00, 0x00, static_cast<uint8_t>((session_id >> 8) & 0xFF), static_cast<uint8_t>(session_id & 0xFF),
                              // Protocol / Interface / Type / Return code
                              0x01, 0x01, 0x02, 0x00,
                              // Flags (reboot) / Reserved
                              0x80, 0x00, 0x00, 0x00,
                              // Length of entries in bytes
                              0x00, 0x00, 0x00, 0x10,
                              // Subscribe / Index #1 / Index #2 / Nb #1 / Nb #2
                              0x06, 0x00, 0x00, 0x10,
                              // Service / Instance
                              (SERVICE_ID >> 8) & 0xFF, SERVICE_ID & 0xFF, (INSTANCE_ID >> 8) & 0xFF, INSTANCE_ID & 0xFF,
                              // Major version (any) / TTL (3 seconds)
                              0x00, 0x00, 0x00, 0x03,
                              // Reserved / Counter / EventGroup ID
                              0x00, 0x00, (EVENTGROUP_ID >> 8) & 0xFF, EVENTGROUP_ID & 0xFF,
                              // Length of options in bytes
                              0x00, 0x00, 0x00, 0x0C,
                              // Option IPv4
                              0x00, 0x09, 0x04, 0x00,
                              // 127.0.0.2
                              static_cast<uint8_t>((ip_le >> 24) & 0xFF), static_cast<uint8_t>((ip_le >> 16) & 0xFF),
                              static_cast<uint8_t>((ip_le >> 8) & 0xFF), static_cast<uint8_t>(ip_le & 0xFF),
                              // Reserved / TCP / port
                              0x00, 0x06, static_cast<uint8_t>((port >> 8) & 0xFF), static_cast<uint8_t>(port & 0xFF)};

    session_id += 1;

    if (udp_client_socket <= 0) {
        udp_client_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_client_socket <= 0) {
            std::cout << "[STRESS] Failed to created UDP socket (errno=" << errno << ")" << std::endl;
            return false;
        }

        int reuse = 1;
        int result = ::setsockopt(udp_client_socket, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse));
        if (result < 0) {
            std::cout << "[STRESS] Failed to set SO_REUSEADDR for UPD socket (errno=" << errno << ")" << std::endl;
            ::close(udp_client_socket);
            udp_client_socket = -1;
            return false;
        }

        result = ::bind(udp_client_socket, reinterpret_cast<const struct sockaddr*>(&client_addr), sizeof(client_addr));

        if (result < 0) {
            std::cout << "[STRESS] Failed to bind UDP socket (errno=" << errno << ")" << std::endl;
            ::close(udp_client_socket);
            udp_client_socket = -1;
            return false;
        }
    }

    // We can send the subscription just after a successful connection (PRS_SOMEIPSD_00461)
    ssize_t send_result = ::sendto(udp_client_socket, message, sizeof(message), MSG_NOSIGNAL,
                                   reinterpret_cast<const struct sockaddr*>(&server_addr), sizeof(server_addr));

    if (send_result != sizeof(message)) {
        std::cout << "[STRESS] Failed to send subscription (errno=" << errno << ")" << std::endl;
        return false;
    }

    return true;
}

static bool test_tcp_connect(uint16_t port) {
    int tcp_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket <= 0) {
        std::cout << "[STRESS] Failed to created TCP socket #" << port << " (errno=" << errno << ")" << std::endl;
        return false;
    }

    int reuse = 1;
    int result = ::setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse));
    if (result < 0) {
        std::cout << "[STRESS] Failed to set SO_REUSEADDR for TCP socket #" << port << " (errno=" << errno << ")" << std::endl;
        ::close(tcp_socket);
        tcp_socket = -1;
        return false;
    }

    struct sockaddr_in client_addr { };

    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    inet_pton(AF_INET, client_ip_addr.c_str(), &client_addr.sin_addr);

    result = ::bind(tcp_socket, reinterpret_cast<const struct sockaddr*>(&client_addr), sizeof(client_addr));
    if (result < 0) {
        std::cout << "[STRESS] Failed to bind TCP socket #" << port << " (errno=" << errno << ")" << std::endl;
        ::close(tcp_socket);
        return false;
    }

    struct sockaddr_in server_addr { };

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_SERVER_PORT);
    inet_pton(AF_INET, server_ip_addr.c_str(), &server_addr.sin_addr);

    result = ::connect(tcp_socket, reinterpret_cast<const struct sockaddr*>(&server_addr), sizeof(server_addr));
    if (result < 0) {
        std::cout << "[STRESS] Failed to connect TCP socket #" << port << " (errno=" << errno << ")" << std::endl;
        ::close(tcp_socket);
        return false;
    }

    if (!send_subscribe(port)) {
        ::close(tcp_socket);
        return false;
    }

    struct timeval timeout_value = {.tv_sec = 2, .tv_usec = 0};

    result = ::setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout_value, sizeof(timeout_value));
    if (result < 0) {
        std::cout << "[STRESS] Failed to set option TCP socket #" << port << " (errno=" << errno << ")" << std::endl;
        ::close(tcp_socket);
        return false;
    }

    std::byte buffer[256];

    ssize_t recv_result = ::recv(tcp_socket, buffer, sizeof(buffer), 0);
    if (recv_result <= 0) {
        std::cout << "[STRESS] Failed to receive TCP socket #" << port << " (errno=" << errno << ")" << std::endl;
        ::close(tcp_socket);
        return false;
    }

    ::close(tcp_socket);
    return true;
}

int main(int argc, char** argv) {
    size_t initial_port = 10000;
    size_t threads_count = 10;
    size_t iterations_count = 100;

    try {
        size_t str_index;

        if (argc > 2) {
            threads_count = std::stol(argv[2], &str_index);
            if (argv[2][str_index] != 0 || threads_count < 1 || threads_count > 1000) {
                throw std::invalid_argument("invalid threads count");
            }
        }

        if (argc > 3) {
            iterations_count = std::stol(argv[3], &str_index);
            if (argv[3][str_index] != 0 || iterations_count < 1 || iterations_count > 10000) {
                throw std::invalid_argument("invalid iterations count");
            }
        }

        if (argc > 1) {
            initial_port = std::stol(argv[1], &str_index);

            if (argv[1][str_index] != 0 || initial_port < 1000 || (initial_port + threads_count * iterations_count) > 65536) {
                throw std::invalid_argument("invalid port range");
            }
        }

        if (argc > 4) {
            client_ip_addr = argv[4];
        }
        if (argc > 5) {
            server_ip_addr = argv[5];
        }
        if (argc > 6) {
            throw std::invalid_argument("too many arguments");
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << "Usage: " << argv[0] << " [start-port] [threads-count] [iterations-count]" << std::endl;
        return -1;
    }

    std::atomic<int> errors_count{};
    std::vector<std::thread> threads;

    for (size_t i = 0; i < threads_count; ++i) {
        threads.push_back(std::thread([&errors_count, start_port = initial_port + i * iterations_count, iterations_count] {
            for (size_t j = 0; j < iterations_count; ++j) {
                if (!test_tcp_connect(static_cast<uint16_t>(start_port + j))) {
                    errors_count += 1;
                }
            }
        }));
    }

    for (size_t i = 0; i < threads_count; ++i) {
        threads[i].join();
    }

    std::cout << "[STRESS] " << errors_count.load() << " failures for " << (threads_count * iterations_count) << " attempts." << std::endl;

    // close udp socket
    send_subscribe(0);

    return errors_count.load();
}
