// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#ifdef _WIN32
    #include <iostream>
    #include <tchar.h>
    #include <intrin.h>
#else
    #include <dlfcn.h>
    #include <signal.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <thread>
    #include <sstream>
#endif

#include <sys/stat.h>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/byteorder.hpp"
#include "../include/utility.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip_v3 {

std::mutex utility::mutex__;
client_t utility::next_client__(VSOMEIP_CLIENT_UNSET);
std::map<client_t, std::string> utility::used_clients__;
#ifdef _WIN32
HANDLE utility::lock_handle__(INVALID_HANDLE_VALUE);
#else
int utility::lock_fd__(-1);
#endif
#ifndef VSOMEIP_ENABLE_CONFIGURATION_OVERLAYS
bool utility::is_checked__(false);
#else
std::set<std::string> utility::is_checked__;
#endif

uint64_t utility::get_message_size(const byte_t *_data, size_t _size) {
    uint64_t its_size(0);
    if (VSOMEIP_SOMEIP_HEADER_SIZE <= _size) {
        its_size = VSOMEIP_SOMEIP_HEADER_SIZE
                + VSOMEIP_BYTES_TO_LONG(_data[4], _data[5], _data[6], _data[7]);
    }
    return (its_size);
}

uint32_t utility::get_payload_size(const byte_t *_data, uint32_t _size) {
    uint32_t its_size(0);
    if (VSOMEIP_SOMEIP_HEADER_SIZE <= _size) {
        its_size = VSOMEIP_BYTES_TO_LONG(_data[4], _data[5], _data[6], _data[7])
                - VSOMEIP_SOMEIP_HEADER_SIZE;
    }
    return (its_size);
}

bool utility::is_routing_manager(const std::shared_ptr<configuration> &_config) {
    // Only the first caller can become routing manager.
    // Therefore, subsequent calls can be immediately answered...
    std::lock_guard<std::mutex> its_lock(mutex__);
#ifndef VSOMEIP_ENABLE_CONFIGURATION_OVERLAYS
    if (is_checked__)
        return false;

    is_checked__ = true;
#else
    if (is_checked__.find(_config->get_network()) != is_checked__.end())
        return false;

    is_checked__.insert(_config->get_network());
#endif
#ifdef _WIN32
    wchar_t its_tmp_folder[MAX_PATH];
    if (GetTempPathW(MAX_PATH, its_tmp_folder)) {
        std::wstring its_lockfile(its_tmp_folder);
        std::string its_network(_config->get_network() + ".lck");
        its_lockfile.append(its_network.begin(), its_network.end());
        lock_handle__ = CreateFileW(its_lockfile.c_str(), GENERIC_READ, 0, NULL, CREATE_NEW, 0, NULL);
        if (lock_handle__ == INVALID_HANDLE_VALUE) {
            VSOMEIP_ERROR << __func__ << ": CreateFileW failed: " << std::hex << GetLastError();
        }
    } else {
        VSOMEIP_ERROR << __func__ << ": Could not get temp folder: "
                << std::hex << GetLastError();
        lock_handle__ = INVALID_HANDLE_VALUE;
    }

    return (lock_handle__ != INVALID_HANDLE_VALUE);
#else
    std::string its_base_path(VSOMEIP_BASE_PATH + _config->get_network());
#ifdef __ANDROID__ // NDK
    const char *env_base_path = getenv(VSOMEIP_ENV_BASE_PATH);
    if (nullptr != env_base_path) {
        its_base_path = {env_base_path + _config->get_network()};
    }
#endif
    std::string its_lockfile(its_base_path + ".lck");
    int its_lock_ctrl(-1);

    struct flock its_lock_data = { F_WRLCK, SEEK_SET, 0, 0, 0 };

    lock_fd__ = open(its_lockfile.c_str(), O_WRONLY | O_CREAT, S_IWUSR | S_IWGRP);
    if (-1 != lock_fd__) {
        its_lock_data.l_pid = getpid();
        its_lock_ctrl = fcntl(lock_fd__, F_SETLK, &its_lock_data);
    } else {
        VSOMEIP_ERROR << __func__
                << ": Could not open " << its_lockfile << ": " << std::strerror(errno);
    }

    return (its_lock_ctrl != -1);
#endif
}

void utility::remove_lockfile(const std::shared_ptr<configuration> &_config) {
    std::lock_guard<std::mutex> its_lock(mutex__);
#ifndef VSOMEIP_ENABLE_CONFIGURATION_OVERLAYS
    if (!is_checked__) // No need to do anything as automatic
        return;
#else
    if (is_checked__.find(_config->get_network()) == is_checked__.end()) // No need to do anything as automatic
        return;
#endif

#ifdef _WIN32
    if (lock_handle__ != INVALID_HANDLE_VALUE) {
        if (CloseHandle(lock_handle__) == 0) {
            VSOMEIP_ERROR << __func__ << ": CloseHandle failed."
                    << std::hex << GetLastError();
        }
        wchar_t its_tmp_folder[MAX_PATH];
        if (GetTempPathW(MAX_PATH, its_tmp_folder)) {
            std::wstring its_lockfile(its_tmp_folder);
            std::string its_network(_config->get_network() + ".lck");
            its_lockfile.append(its_network.begin(), its_network.end());
            if (DeleteFileW(its_lockfile.c_str()) == 0) {
                VSOMEIP_ERROR << __func__ << ": DeleteFileW failed: "
                        << std::hex << GetLastError();

            }
        } else {
            VSOMEIP_ERROR << __func__ << ": Could not get temp folder."
                    << std::hex << GetLastError();
        }
    }
#else
    std::string its_base_path(VSOMEIP_BASE_PATH + _config->get_network());
#ifdef __ANDROID__ // NDK
    const char *env_base_path = getenv(VSOMEIP_ENV_BASE_PATH);
    if (nullptr != env_base_path) {
        its_base_path = {env_base_path + _config->get_network()};
    }
#endif
    std::string its_lockfile(its_base_path + ".lck");

    if (lock_fd__ != -1) {
       if (close(lock_fd__) == -1) {
           VSOMEIP_ERROR << __func__ << ": Could not close lock_fd__"
                   << std::strerror(errno);
       }
    }
    if (remove(its_lockfile.c_str()) == -1) {
        VSOMEIP_ERROR << __func__ << ": Could not remove " << its_lockfile
                << ": " << std::strerror(errno);
    }
#endif
#ifndef VSOMEIP_ENABLE_CONFIGURATION_OVERLAYS
    is_checked__ = false;
#else
    is_checked__.erase(_config->get_network());
#endif
}

bool utility::exists(const std::string &_path) {
    struct stat its_stat;
    return (stat(_path.c_str(), &its_stat) == 0);
}

bool utility::is_file(const std::string &_path) {
    struct stat its_stat;
    if (stat(_path.c_str(), &its_stat) == 0) {
        if (its_stat.st_mode & S_IFREG)
            return true;
    }
    return false;
}

bool utility::is_folder(const std::string &_path) {
    struct stat its_stat;
    if (stat(_path.c_str(), &its_stat) == 0) {
        if (its_stat.st_mode & S_IFDIR)
            return true;
    }
    return false;
}

const std::string utility::get_base_path(
        const std::shared_ptr<configuration> &_config) {
#ifdef __ANDROID__ // NDK
    const char *env_base_path = getenv(VSOMEIP_ENV_BASE_PATH);
    if (nullptr != env_base_path) {
        std::string its_base_path(env_base_path + _config->get_network());
        return std::string(env_base_path + _config->get_network() + "-");
    } else {
        return std::string(VSOMEIP_BASE_PATH + _config->get_network() + "-");
    }
#else
    return std::string(VSOMEIP_BASE_PATH + _config->get_network() + "-");
#endif
}

client_t
utility::request_client_id(
        const std::shared_ptr<configuration> &_config,
        const std::string &_name, client_t _client) {
    std::lock_guard<std::mutex> its_lock(mutex__);
    static const std::uint16_t its_max_num_clients = get_max_client_number(_config);

    static const std::uint16_t its_diagnosis_mask = _config->get_diagnosis_mask();
    static const std::uint16_t its_client_mask = static_cast<std::uint16_t>(~its_diagnosis_mask);
    static const client_t its_masked_diagnosis_address = static_cast<client_t>(
            (_config->get_diagnosis_address() << 8) & its_diagnosis_mask);
    static const client_t its_biggest_client = its_masked_diagnosis_address | its_client_mask;
    static const client_t its_smallest_client = its_masked_diagnosis_address;

    if (next_client__ == VSOMEIP_CLIENT_UNSET) {
        next_client__ = its_smallest_client;
    }

    if (_client != VSOMEIP_CLIENT_UNSET) { // predefined client identifier
        const auto its_iterator = used_clients__.find(_client);
        if (its_iterator == used_clients__.end()) { // unused identifier
            used_clients__[_client] = _name;
            return _client;
        } else { // already in use

            // The name matches the assigned name --> return client
            // NOTE: THIS REQUIRES A CONSISTENT CONFIGURATION!!!
            if (its_iterator->second == _name) {
                return _client;
            }

            VSOMEIP_WARNING << "Requested client identifier "
                    << std::setw(4) << std::setfill('0')
                    << std::hex << _client
                    << " is already used by application \""
                    << its_iterator->second
                    << "\".";
            // intentionally fall through
        }
    }

    if (next_client__ == its_biggest_client) {
        // start at beginning of client range again when the biggest client was reached
        next_client__ = its_smallest_client;
    }
    std::uint16_t increase_count = 0;
    do {
        next_client__ = (next_client__ & static_cast<std::uint16_t>(~its_client_mask)) // save diagnosis address bits
                | (static_cast<std::uint16_t>((next_client__ // set all diagnosis address bits to one
                        | static_cast<std::uint16_t>(~its_client_mask)) + 1u) //  and add one to the result
                                & its_client_mask); // set the diagnosis address bits to zero again
        if (increase_count++ == its_max_num_clients) {
            VSOMEIP_ERROR << __func__ << " no free client IDs left! "
                    "Max amount of possible concurrent active vsomeip "
                    "applications reached ("  << std::dec << used_clients__.size()
                    << ").";
            return VSOMEIP_CLIENT_UNSET;
        }
    } while (used_clients__.find(next_client__) != used_clients__.end()
            || _config->is_configured_client_id(next_client__));

    used_clients__[next_client__] = _name;
    return next_client__;
}

void
utility::release_client_id(client_t _client) {
    std::lock_guard<std::mutex> its_lock(mutex__);
    used_clients__.erase(_client);
}

std::set<client_t>
utility::get_used_client_ids() {
    std::lock_guard<std::mutex> its_lock(mutex__);
    std::set<client_t> its_used_clients;
    for (const auto& c : used_clients__)
        its_used_clients.insert(c.first);
    return its_used_clients;
}

void utility::reset_client_ids() {
    std::lock_guard<std::mutex> its_lock(mutex__);
    used_clients__.clear();
    next_client__ = VSOMEIP_CLIENT_UNSET;
}



std::uint16_t utility::get_max_client_number(
        const std::shared_ptr<configuration> &_config) {
    std::uint16_t its_max_clients(0);
    const int bits_for_clients =
#ifdef _WIN32
            __popcnt(
#else
            __builtin_popcount(
#endif
                    static_cast<std::uint16_t>(~_config->get_diagnosis_mask()));
    for (int var = 0; var < bits_for_clients; ++var) {
        its_max_clients = static_cast<std::uint16_t>(its_max_clients | (1 << var));
    }
    return its_max_clients;
}

} // namespace vsomeip_v3
