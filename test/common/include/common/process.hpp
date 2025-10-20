// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once

#include <set>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <thread>

#include "external/process.hpp"

namespace common {
/**
 * @brief Manages a single child process with lifecycle control.
 * @invariant Not copyable. Only one process per instance.
 * @note Use run(), join(), terminate(), and wait_for_start() for process control.
 */
class process_manager_t {
public:
    /**
     * @brief Construct a process manager for a given command and environment.
     * @param cmd Command to execute.
     * @param env_vars Environment variables for the process.
     */
    explicit process_manager_t(std::string_view cmd, std::map<std::string, std::string> env_vars = std::map<std::string, std::string>());

    process_manager_t(const process_manager_t&) = delete;
    process_manager_t& operator=(const process_manager_t&) = delete;
    /**
     * @brief Destructor.
     */
    ~process_manager_t();

    /**
     * @brief Block and wait for the process to finish
     */
    void wait_finish();

    /**
     * @brief Sends a SIGTERM to a process.
     */
    void terminate();

private:
    void spawn();

    std::atomic_int32_t exit_code_{0};
    std::unique_ptr<TinyProcessLib::Process> process_ = nullptr;
    std::unique_ptr<std::thread> thread_;
    std::string command_; // Changed from string_view to string for TinyProcessLib
    std::map<std::string, std::string> env_vars_;
    std::mutex process_mutex_;
    std::condition_variable finished_;
};

/**
 * @brief Manages a group of named processes, types, and daemons for test orchestration.
 * @invariant Not copyable. Types and configs must be defined before start_all().
 * @note Use define_type(), add_process(), add_daemon(), start_all(), stop_all().
 */
class process_group_t {
public:
    using EnvModifier = std::function<void(std::map<std::string, std::string>&)>;

    /**
     * @brief Construct a process group.
     */
    process_group_t();
    /**
     * @brief Destructor.
     */
    ~process_group_t();

    /**
     * @brief Define a process type with executable and environment setup.
     * @param type_name Name of the type.
     * @param executable Executable path.
     * @param env_setup Function to modify environment variables.
     * @return Reference to this group.
     */
    process_group_t& define_type(const std::string& type_name, std::string_view executable, EnvModifier env_setup);
    /**
     * @brief Add a process by name and type.
     * @param name Process name.
     * @param type_name Type name to use.
     * @return Reference to this group.
     */
    process_group_t& add_process(const std::string& name, const std::string& type_name);

    /**
     * @brief Add a process by name and type, and whether it will only be launched once per test.
     * @param name Process name.
     * @param type_name Type name to use.
     * @param start_once Whether the process will be launched only once in start_all();
     * @return Reference to this group.
     */
    process_group_t& add_process(const std::string& name, const std::string& type_name, bool start_once);
    /**
     * @brief Add a daemon process by name and config file.
     * @param name Daemon name.
     * @param config_file Configuration file path.
     * @return Reference to this group.
     */
    process_group_t& add_daemon(std::string config_file);

    /**
     * @brief Start the first process that matches the given name.
     */
    void start(const std::string& name);

    /**
     * @brief Start all processes in the group.
     */
    void start();
    /**
     * @brief Check if all processes are valid.
     * @return True if all valid.
     */
    // bool all_valid();
    /**
     * @brief Stop all processes in the group.
     */
    void stop();
    /**
     * @brief Force stop all processes in the group.
     */
    void force_stop();

    /**
     * @brief Access a process manager by process name.
     * @param name Process name.
     * @return Reference to process manager.
     */
    process_manager_t& operator[](const std::string& name);

public:
    struct ProcessType {
        std::string_view executable;
        EnvModifier env_setup;
    };

    struct ProcessConfig {
        std::string name;
        std::string_view executable;
        std::map<std::string, std::string> env_vars;
    };

    std::map<std::string, ProcessType> types_;
    std::vector<ProcessConfig> configs_;
    std::map<std::string, std::unique_ptr<process_manager_t>> processes_;
};
} // namespace common