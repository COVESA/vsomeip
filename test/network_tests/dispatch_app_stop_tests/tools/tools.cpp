// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "tools.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path.string());
    }

    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to create config file: " + path.string());
    }

    output << content;
}

std::string replace_json_value(std::string content, const std::string& key, const std::string& to) {
    const std::regex pattern("\"" + key + "\"\\s*:\\s*\"[^\"]*\"");
    return std::regex_replace(content, pattern, "\"" + key + "\": \"" + to + "\"");
}

bool is_writable_directory(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
        return false;
    }

    const auto probe_file = path / ".dispatch_app_stop_write_probe";
    {
        std::ofstream output(probe_file, std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
    }

    std::filesystem::remove(probe_file, ec);
    return true;
}

std::filesystem::path resolve_target_directory(const std::filesystem::path& fallback) {
    if (const char* test_tmpdir = std::getenv("TEST_TMPDIR"); test_tmpdir != nullptr) {
        const std::filesystem::path candidate(test_tmpdir);
        if (is_writable_directory(candidate)) {
            return candidate;
        }
    }

    if (const char* base_path = std::getenv("VSOMEIP_BASE_PATH"); base_path != nullptr) {
        const std::filesystem::path candidate(base_path);
        if (is_writable_directory(candidate)) {
            return candidate;
        }
    }

    return fallback;
}

void set_env_or_throw(const std::string& key, const std::string& value) {
#ifdef _WIN32
    if (_putenv_s(key.c_str(), value.c_str()) != 0) {
        throw std::runtime_error("Failed to export environment variable: " + key);
    }
#else
    if (setenv(key.c_str(), value.c_str(), 1) != 0) {
        throw std::runtime_error("Failed to export environment variable: " + key);
    }
#endif
}

} // namespace

bool create_config(std::string _app_name) {
    if (_app_name.empty()) {
        return true;
    }

    try {
        const char* base_config = std::getenv("VSOMEIP_CONFIGURATION");
        if (base_config == nullptr || base_config[0] == '\0') {
            return true;
        }
        const std::filesystem::path source_path(base_config);

        const std::string target_name = _app_name;

        // Write generated configs into the temporary test directory when available.
        const std::filesystem::path target_dir = resolve_target_directory(source_path.parent_path());
        const std::filesystem::path target_path = target_dir / (target_name + source_path.extension().string());

        // Load base configuration file into memory.
        std::string content = read_file(source_path);

        // Ensure generated config references the target application consistently.
        content = replace_json_value(content, "network", _app_name);
        content = replace_json_value(content, "name", _app_name);
        content = replace_json_value(content, "routing", _app_name);

        // Persist modified configuration to new file.
        write_file(target_path, content);

        // Bind app name to generated config via VSOMEIP_CONFIGURATION_<app_name>.
        set_env_or_throw("VSOMEIP_CONFIGURATION_" + _app_name, target_path.string());
    } catch (...) {
        return true;
    }

    return false;
}
