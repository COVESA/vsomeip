// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>

#include "../../../implementation/configuration/include/configuration_impl.hpp"
#include "../../../implementation/logger/include/logger_impl.hpp"
#include "../../../implementation/utility/include/blocking_guard.hpp"

namespace {

using vsomeip_v3::logger::level_e;
using vsomeip_v3::logger::logger_impl;

// configuration has >100 pure virtuals, so derive from the concrete configuration_impl and override only the logging getters
// needed to send the guard's output to the console.
class console_configuration : public vsomeip_v3::cfg::configuration_impl {
public:
    console_configuration() : configuration_impl("") { }

    bool has_console_log() const override { return true; }
    bool has_file_log() const override { return false; }
    bool has_dlt_log() const override { return false; }
    const std::string& get_logfile() const override { return logfile_value_; }
    level_e get_loglevel() const override { return level_e::LL_DEBUG; }

    std::string logfile_value_{};
};

// Redirects std::cout into a buffer so the emitted log can be inspected.
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

void block_for(std::chrono::milliseconds _duration) {
    std::this_thread::sleep_for(_duration);
}

constexpr auto WARN_TIME = vsomeip_v3::VSOMEIP_CALLBACK_WARN_TIME;
constexpr auto ERROR_TIME = vsomeip_v3::VSOMEIP_CALLBACK_ERROR_TIME;

class blocking_guard_test : public ::testing::Test {
protected:
    // The logger is a process-wide singleton; enable the console around each test and turn it off again.
    void SetUp() override { logger_impl::get()->set_configuration(std::make_shared<console_configuration>()); }
    void TearDown() override { logger_impl::get()->set_configuration(nullptr); }
};

TEST_F(blocking_guard_test, returns_the_callback_result) {
    const bool its_result = vsomeip_v3::invoke_watched([](bool _accept) { return _accept; }, "test_callback()", true);
    EXPECT_TRUE(its_result);
}

TEST_F(blocking_guard_test, says_nothing_about_a_fast_callback) {
    cout_capture its_capture;
    vsomeip_v3::invoke_watched([] {}, "test_callback()");
    EXPECT_EQ("", its_capture.str());
}

TEST_F(blocking_guard_test, warns_when_the_callback_blocks_beyond_the_warn_time) {
    cout_capture its_capture;
    vsomeip_v3::invoke_watched([] { block_for(WARN_TIME * 2); }, "test_callback()");

    const std::string its_output = its_capture.str();
    EXPECT_NE(std::string::npos, its_output.find("[warning]"));
    EXPECT_NE(std::string::npos, its_output.find("BLOCKING CALL test_callback() returned after "));
}

TEST_F(blocking_guard_test, errors_when_the_callback_blocks_beyond_the_error_time) {
    cout_capture its_capture;
    vsomeip_v3::invoke_watched([] { block_for(ERROR_TIME + std::chrono::milliseconds(20)); }, "test_callback()");

    const std::string its_output = its_capture.str();
    EXPECT_NE(std::string::npos, its_output.find("[error]"));
    EXPECT_NE(std::string::npos, its_output.find("BLOCKING CALL test_callback() returned after "));
}

TEST_F(blocking_guard_test, formats_a_lazy_description_once_the_callback_blocks) {
    int its_formats{0};
    const auto its_description = vsomeip_v3::lazy_description{[&its_formats](std::ostream& _log) {
        ++its_formats;
        _log << "test_callback(0.0.0.0)";
    }};

    cout_capture its_capture;
    vsomeip_v3::invoke_watched([] { block_for(WARN_TIME * 2); }, its_description);

    EXPECT_EQ(1, its_formats);
    EXPECT_NE(std::string::npos, its_capture.str().find("BLOCKING CALL test_callback(0.0.0.0) returned after "));
}

}
