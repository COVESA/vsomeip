// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/process.hpp>
#include <boost/asio/io_service.hpp>

#include <iostream>

/**
 * Wrapper to create and manage processes via boost process API.
 */
class process_manager {
public:
    /**
     * @brief Construct a new process manager object
     *
     * @param cmd Command to execute.
     * @param env_vars Specific environment variables to be passed to new process, new process environemnt base is copied from the current
     * one.
     */
    process_manager(std::string cmd, std::map<std::string, std::string> env_vars) : command_{cmd}, env_vars_{env_vars} { }

    /**
     * @brief Dtor, waits for spawning thread.
     * TBD - Should probably try terminate process also.
     */
    ~process_manager() { join(); }

    /**
     * @brief Creates Spawning thread.
     */
    void run() { thread_ = std::make_unique<std::thread>(std::bind(&process_manager::spawn, this)); }

    /**
     * @brief If joinable, joins spawning thread.
     */
    void join() {
        if (thread_->joinable()) {
            thread_->join();
        }
    }

    /**
     * @brief Forces the restart of the process with same conditions.
     */
    void reset() {
        terminate();
        join();
        thread_.reset(std::make_unique<std::thread>(std::bind(&process_manager::spawn, this)).release());
    }

    /**
     * @brief Forces the shutdown of the spawned process.
     */
    void terminate() {
        std::scoped_lock lock(process_mutex_);
        if (process_.running()) {
            process_.terminate();
        }
    }

    /**
     * @brief Waits for spawned process to be shifted into ready state i.e., the process might not have started yet.
     */
    void wait_for_start() {
        std::unique_lock<std::mutex> lock(process_mutex_);
        start_cv_.wait(lock, [this]() { return process_.valid(); });
    }

    /**
     * @brief Verifies if process native handle has been created.
     *
     * @return True is valid, false otherwise.
     */
    bool valid() { return process_.valid(); }

    /**
     * @brief Send a specified signal type to the spawned process.
     *
     * @param signal Signal to be sent.
     */
    void send_signal(int signal) {
        std::scoped_lock lock(process_mutex_);
        kill(process_.id(), signal);
    }

    std::atomic_int32_t exit_code_{0};

private:
    /**
     * @brief Manages the creation of the process and enforces the wait on it to ensure it's lifetime.
     */
    void spawn() {
        std::unique_lock<std::mutex> lock(process_mutex_);
        boost::asio::io_service io;

        // Handle env vars
        boost::process::environment cp_env;
        for (const auto& env_entry : boost::this_process::environment()) {
            cp_env[env_entry.get_name()] = env_entry.to_string();
        }
        for (const auto& env_map : env_vars_) {
            const std::string& k = env_map.first;
            const std::string& v = env_map.second;
            cp_env[k] = v;
        }

        process_ = boost::process::child(
                command_, cp_env, io, boost::process::on_exit = [this](int exit, const std::error_code& eci) {
                    exit_code_ = exit;
                    std::cout << "Process " << command_ << " exited with " << exit << " and ec " << eci.message() << std::endl;
                });
        start_cv_.notify_all();
        if (process_.running()) {
            lock.unlock();
            io.run();
        }
    }

    boost::process::child process_;
    std::unique_ptr<std::thread> thread_;
    std::string command_;
    std::map<std::string, std::string> env_vars_;
    std::mutex process_mutex_;
    std::condition_variable start_cv_;
};
