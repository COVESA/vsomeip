// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#if BOOST_VERSION < 108800
#include <boost/process.hpp>
#else
#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1/args.hpp>
#include <boost/process/v1/async.hpp>
#include <boost/process/v1/async_system.hpp>
#include <boost/process/v1/group.hpp>
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/cmd.hpp>
#include <boost/process/v1/env.hpp>
#include <boost/process/v1/environment.hpp>
#include <boost/process/v1/error.hpp>
#include <boost/process/v1/exe.hpp>
#include <boost/process/v1/group.hpp>
#include <boost/process/v1/handles.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/pipe.hpp>
#include <boost/process/v1/shell.hpp>
#include <boost/process/v1/search_path.hpp>
#include <boost/process/v1/spawn.hpp>
#include <boost/process/v1/system.hpp>
#include <boost/process/v1/start_dir.hpp>
#endif

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
    process_manager(const std::string& cmd, const std::map<std::string, std::string>& env_vars) : command_{cmd}, env_vars_{env_vars} { }

    /**
     * @brief Destructor
     *
     * EXPLODES if you do not wait for the launched process!
     */
    ~process_manager() {
        if (process_) {
            std::cerr << "ABORTING BECAUSE THERE ARE STILL BACKGROUND PROCESSES RUNNING!" << std::endl;
            std::abort();
        };
    }

    /**
     * @brief Laucnh process
     */
    void run() {
        std::scoped_lock lock(mutex_);
        if (process_) {
            std::cerr << "ABORTING BECAUSE THERE ARE STILL BACKGROUND PROCESSES RUNNING!" << std::endl;
            std::abort();
        }

        boost::process::environment cp_env;
        for (const auto& env_entry : boost::this_process::environment()) {
            cp_env[env_entry.get_name()] = env_entry.to_string();
        }
        for (const auto& env_map : env_vars_) {
            const std::string& k = env_map.first;
            const std::string& v = env_map.second;
            cp_env[k] = v;
        }

        process_ = std::make_unique<boost::process::child>(command_, cp_env);

        std::cout << "Process '" << command_ << "' started, pid " << process_->id() << std::endl;
    }

    /**
     * @brief Wait for process to finish, return exit code
     */
    [[nodiscard]] int wait() {
        std::unique_lock lock(mutex_);
        if (process_ == nullptr) {
            std::cerr << "Aborting because there is no process '" << command_ << "' running!" << std::endl;
            std::abort();
        }

        lock.unlock();
        process_->wait();
        lock.lock();

        int exit_code = process_->exit_code();
        std::cout << "Process '" << command_ << "' exited with " << exit_code << std::endl;
        process_ = nullptr;
        return exit_code;
    }

    /**
     * @brief Send a specified signal type to the spawned process.
     *
     * @param signal Signal to be sent.
     */
    void send_signal(int signal) {
        std::scoped_lock lock(mutex_);
        if (process_ == nullptr) {
            std::cerr << "Aborting because there is no process '" << command_ << "' running!" << std::endl;
            std::abort();
        }

        kill(process_->id(), signal);
    }

private:
    const std::string command_;
    const std::map<std::string, std::string> env_vars_;
    std::mutex mutex_;
    std::unique_ptr<boost::process::child> process_;
};
