#include <algorithm>
#include <common/process.hpp>
#include <csignal>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <mutex>

#include <assert.h>

#ifndef _WIN32
extern char** environ;
#else
extern char** _environ;
#endif

namespace common {
// ----------------------------------------------------------------------------
// PROCESS MANAGER
// ----------------------------------------------------------------------------
process_manager_t::process_manager_t(std::string_view cmd, std::map<std::string, std::string> env_vars) :
    command_{cmd}, env_vars_{std::move(env_vars)} {
    thread_ = std::make_unique<std::thread>(std::bind(&process_manager_t::spawn, this));
}

process_manager_t::~process_manager_t() {
    if (thread_->joinable()) {
        thread_->join();
    }
}

void process_manager_t::spawn() {
    {
        std::unique_lock<std::mutex> lock{process_mutex_};
        // Build environment map for TinyProcessLib
        TinyProcessLib::Process::environment_type env;
        for (const auto& [key, value] : env_vars_) {
            env[key] = value;
        }

        // NOTE! need to use argv-style so TinyProcessLib does execv
        // otherwise it spawns /bin/sh.. which is problematic if we have to signal spawned processes
        process_ = std::make_unique<TinyProcessLib::Process>(std::vector<std::string>({command_}), "", env);
    }
}

void process_manager_t::wait_finish() {
    std::unique_ptr<TinyProcessLib::Process> local_proc;
    {
        std::scoped_lock lock{process_mutex_};
        if (process_) {
            local_proc = std::move(process_);
        }
    }
    if (local_proc) {
        exit_code_ = local_proc->get_exit_status();
        assert(exit_code_ == 0 && "Process exited with non-zero code");
    }
}

void process_manager_t::terminate() {
    std::scoped_lock lock{process_mutex_};
    if (process_) {
#ifdef __linux__
        process_->signal(SIGTERM);
#else
        process_->kill();
#endif
    }
}

// ----------------------------------------------------------------------------
// PROCESS GROUP
// ----------------------------------------------------------------------------

static std::map<std::string, std::string> setup_env(const std::string& name) {
    std::map<std::string, std::string> env;
    // pass all environment variables
#ifndef _WIN32
    for (char** e = environ; *e; e++) {
#else
    for (char** e = _environ; *e; e++) {
#endif
        // format: "key=value"
        std::string e1(*e);
        size_t pos = e1.find("=");
        assert(pos != std::string::npos);
        std::string name = e1.substr(0, pos);
        std::string value = e1.substr(pos + 1);
        env[name] = value;
    }
    // always set application name to the process name
    env["VSOMEIP_APPLICATION_NAME"] = name;

    return env;
}

process_group_t::process_group_t() {
    types_["daemon"] = {"../../../examples/routingmanagerd/routingmanagerd", [](auto& env) {
                            (void)env; /* daemon uses empty env */
                        }};
}

process_group_t& process_group_t::define_type(const std::string& type_name, std::string_view executable, EnvModifier env_setup) {
    types_[type_name] = {executable, std::move(env_setup)};
    return *this;
}

process_group_t& process_group_t::add_process(const std::string& name, const std::string& type_name) {
    auto& type = types_.at(type_name);
    std::map<std::string, std::string> env = setup_env(name);
    // apply type-specific environment setup
    type.env_setup(env);

    configs_.push_back({name, type.executable, env});
    return *this;
}

process_group_t& process_group_t::add_daemon(std::string config_file) {
    auto& type = types_.at("daemon");
    std::map<std::string, std::string> env = setup_env("daemon");
    env["VSOMEIP_CONFIGURATION"] = std::move(config_file);

    configs_.push_back({"daemon", type.executable, env});
    return *this;
}

void process_group_t::start(const std::string& name) {
    auto found = std::find_if(configs_.begin(), configs_.end(), [&](auto& config) -> bool { return config.name == name; });

    if (found != configs_.end()) {
        if (processes_.contains(found->name)) {
            return;
        }

        processes_[found->name] = std::make_unique<process_manager_t>(found->executable, found->env_vars);
    }
}

void process_group_t::start() {
    for (auto& config : configs_) {
        if (processes_[config.name]) {
            // std::cout << "PROCESS " << config.name << " ALREADY LAUNCHED" << std::endl;
            continue;
        }
        std::cout << "process_group_t::start -> Launching " << config.name << std::endl;
        processes_[config.name] = std::make_unique<process_manager_t>(config.executable, config.env_vars);
    }
    // KLUDGE(vcarvalho): testing on some sanitizer/valgrind types delays process initialization, such that introducing this delay
    // fixes odd, sporadic behaviours on process launches.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void process_group_t::stop() {
    for (auto& [name, proc] : processes_) {
        if (name == "daemon") {
            proc->terminate();
            proc->wait_finish();
        } else {
            proc->wait_finish();
        }
    }
    processes_.clear();
}

void process_group_t::force_stop() {
    for (auto& [name, proc] : processes_) {
        proc->terminate();
        proc->wait_finish();
    }
    processes_.clear();
}

process_manager_t& process_group_t::operator[](const std::string& name) {
    return *processes_.at(name);
}

process_group_t::~process_group_t() {
    force_stop();
}

} // namespace common