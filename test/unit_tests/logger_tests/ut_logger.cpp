// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <vsomeip/internal/logger.hpp>

#include "../../../implementation/configuration/include/configuration_impl.hpp"
#include "../../../implementation/logger/include/logger_impl.hpp"

namespace {

using vsomeip_v3::logger::level_e;
using vsomeip_v3::logger::logger_impl;

// config is stored in a std::atomic, so it must stay trivially copyable.
static_assert(std::is_trivially_copyable_v<logger_impl::config>,
              "logger_impl::config must stay trivially copyable (it lives in a std::atomic)");

// configuration has >100 pure virtuals, so derive from the concrete configuration_impl and override
// only the five logging getters instead of hand-mocking.
class test_configuration : public vsomeip_v3::cfg::configuration_impl {
public:
    test_configuration() : configuration_impl("") { }

    bool has_console_log() const override { return console_; }
    bool has_file_log() const override { return file_; }
    bool has_dlt_log() const override { return dlt_; }
    const std::string& get_logfile() const override { return logfile_value_; }
    level_e get_loglevel() const override { return level_; }

    bool console_{false};
    bool file_{false};
    bool dlt_{false};
    std::string logfile_value_{};
    level_e level_{level_e::LL_NONE};
};

// Redirects std::cout into a buffer so console output can be inspected.
class cout_capture {
public:
    cout_capture() : previous_{std::cout.rdbuf(buffer_.rdbuf())} { }
    ~cout_capture() { std::cout.rdbuf(previous_); }

    cout_capture(const cout_capture&) = delete;
    cout_capture& operator=(const cout_capture&) = delete;

    std::string str() const { return buffer_.str(); }

private:
    std::ostringstream buffer_;
    std::streambuf* previous_;
};

std::filesystem::path make_temp_logfile() {
    static std::atomic<unsigned> counter{0};
    auto name = "vsomeip_logger_ut_" + std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1)) + ".log";
    return std::filesystem::temp_directory_path() / name;
}

class logger_test : public ::testing::Test {
protected:
    // The logger is a process-wide singleton; reset to "all sinks off" around each test.
    void SetUp() override { apply_config(false, false, false, level_e::LL_NONE, ""); }
    void TearDown() override { apply_config(false, false, false, level_e::LL_NONE, ""); }

    static void apply_config(bool console, bool file, bool dlt, level_e level, const std::string& logfile) {
        auto cfg = std::make_shared<test_configuration>();
        cfg->console_ = console;
        cfg->file_ = file;
        cfg->dlt_ = dlt;
        cfg->level_ = level;
        cfg->logfile_value_ = logfile;
        logger_impl::get()->set_configuration(cfg);
    }

    static std::string read_file(const std::filesystem::path& path) {
        std::ifstream in{path, std::ios::binary};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
};

// --- Singleton lifetime ----------------------------------------------------------------------

TEST_F(logger_test, get_returns_stable_non_null_instance) {
    auto* first = logger_impl::get();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, logger_impl::get());
    EXPECT_EQ(first, logger_impl::get());
}

TEST_F(logger_test, get_is_stable_across_threads) {
    auto* expected = logger_impl::get();

    constexpr int num_threads = 16;
    std::vector<logger_impl*> results(num_threads, nullptr);
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&results, i] { results[static_cast<size_t>(i)] = logger_impl::get(); });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::set<logger_impl*> unique{results.begin(), results.end()};
    ASSERT_EQ(unique.size(), 1u);
    EXPECT_EQ(*unique.begin(), expected);
}

// --- Configuration plumbing ------------------------------------------------------------------

TEST_F(logger_test, set_configuration_is_reflected_in_get_configuration) {
    apply_config(true, false, true, level_e::LL_WARNING, "");

    const auto cfg = logger_impl::get()->get_configuration();
    EXPECT_TRUE(cfg.console_enabled);
    EXPECT_FALSE(cfg.file_enabled);
    EXPECT_TRUE(cfg.dlt_enabled);
    EXPECT_EQ(cfg.loglevel, level_e::LL_WARNING);
}

TEST_F(logger_test, set_configuration_ignores_nullptr) {
    apply_config(true, false, false, level_e::LL_INFO, "");
    const auto before = logger_impl::get()->get_configuration();

    logger_impl::get()->set_configuration(nullptr);

    const auto after = logger_impl::get()->get_configuration();
    EXPECT_EQ(after.console_enabled, before.console_enabled);
    EXPECT_EQ(after.file_enabled, before.file_enabled);
    EXPECT_EQ(after.dlt_enabled, before.dlt_enabled);
    EXPECT_EQ(after.loglevel, before.loglevel);
}

TEST_F(logger_test, init_applies_configuration) {
    auto cfg = std::make_shared<test_configuration>();
    cfg->console_ = true;
    cfg->dlt_ = false;
    cfg->file_ = false;
    cfg->level_ = level_e::LL_DEBUG;

    logger_impl::init(cfg);

    const auto applied = logger_impl::get()->get_configuration();
    EXPECT_TRUE(applied.console_enabled);
    EXPECT_EQ(applied.loglevel, level_e::LL_DEBUG);
}

// --- File logging ----------------------------------------------------------------------------

TEST_F(logger_test, log_to_file_writes_content_when_file_enabled) {
    const auto path = make_temp_logfile();
    std::filesystem::remove(path);

    apply_config(false, true, false, level_e::LL_VERBOSE, path.string());
    logger_impl::get()->log_to_file("payload-to-be-written\n");

    EXPECT_NE(read_file(path).find("payload-to-be-written"), std::string::npos);

    apply_config(false, false, false, level_e::LL_NONE, ""); // close the file so it can be deleted
    std::filesystem::remove(path);
}

TEST_F(logger_test, log_to_file_is_noop_when_file_disabled) {
    // With file logging disabled (SetUp state), writing must be a safe no-op rather than crashing.
    EXPECT_NO_THROW(logger_impl::get()->log_to_file("must-not-crash"));
}

// --- Message: level filtering and formatting (console sink) ----------------------------------

TEST_F(logger_test, message_respects_configured_loglevel) {
    apply_config(true, false, false, level_e::LL_WARNING, "");

    cout_capture capture;
    { VSOMEIP_ERROR << "error-should-appear"; } // ERROR(2) <= WARNING(3) -> emitted
    { VSOMEIP_WARNING << "warning-should-appear"; } // WARNING(3) <= WARNING(3) -> emitted
    { VSOMEIP_INFO << "info-should-be-dropped"; } // INFO(4) > WARNING(3) -> dropped

    const auto out = capture.str();
    EXPECT_NE(out.find("error-should-appear"), std::string::npos);
    EXPECT_NE(out.find("warning-should-appear"), std::string::npos);
    EXPECT_EQ(out.find("info-should-be-dropped"), std::string::npos);
}

TEST_F(logger_test, message_format_contains_level_text_and_newline) {
    apply_config(true, false, false, level_e::LL_VERBOSE, "");

    cout_capture capture;
    { VSOMEIP_INFO << "formatted-payload"; }

    const auto out = capture.str();
    EXPECT_NE(out.find("[info]"), std::string::npos);
    EXPECT_NE(out.find("formatted-payload"), std::string::npos);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.back(), '\n');
}

TEST_F(logger_test, message_produces_no_output_when_all_sinks_disabled) {
    // All sinks off (SetUp): even FATAL produces nothing.
    cout_capture capture;
    { VSOMEIP_FATAL << "nothing-should-be-logged"; }
    EXPECT_TRUE(capture.str().empty());
}

TEST_F(logger_test, message_is_written_to_file_sink) {
    const auto path = make_temp_logfile();
    std::filesystem::remove(path);

    apply_config(false, true, false, level_e::LL_VERBOSE, path.string());
    { VSOMEIP_WARNING << "message-file-sink-payload"; }

    const auto contents = read_file(path);
    EXPECT_NE(contents.find("message-file-sink-payload"), std::string::npos);
    EXPECT_NE(contents.find("[warning]"), std::string::npos);

    apply_config(false, false, false, level_e::LL_NONE, ""); // close the file so it can be deleted
    std::filesystem::remove(path);
}

// --- DLT routing -----------------------------------------------------------------------------

// Runs in both configs: with USE_DLT it reaches log_to_dlt(), otherwise the DLT block is compiled
// out. Either way, logging every level via the DLT sink must be crash-safe.
TEST_F(logger_test, dlt_enabled_messages_are_crash_safe) {
    apply_config(false, false, true, level_e::LL_VERBOSE, "");

    EXPECT_NO_THROW({
        VSOMEIP_FATAL << "dlt-fatal";
        VSOMEIP_ERROR << "dlt-error";
        VSOMEIP_WARNING << "dlt-warning";
        VSOMEIP_INFO << "dlt-info";
        VSOMEIP_DEBUG << "dlt-debug";
        VSOMEIP_TRACE << "dlt-verbose";
    });
}

#ifdef USE_DLT

// DLT-backend specific: calling log_to_dlt() directly for every level (including the default
// branch) must not crash, and the by-value DLT context member must be reachable.
TEST_F(logger_test, log_to_dlt_all_levels_does_not_crash) {
    auto* logger = logger_impl::get();

    // context member is reachable
    EXPECT_NE(&logger->dlt_context_, nullptr);

    const level_e levels[] = {level_e::LL_NONE, level_e::LL_FATAL, level_e::LL_ERROR,  level_e::LL_WARNING,
                              level_e::LL_INFO, level_e::LL_DEBUG, level_e::LL_VERBOSE};
    for (auto level : levels) {
        // log_to_dlt expects a null-terminated payload.
        std::string msg = "dlt-direct-payload";
        msg.push_back('\0');
        EXPECT_NO_THROW(logger->log_to_dlt(level, msg));
    }
}

#endif // USE_DLT

} // namespace
