// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>

namespace common {

/**
 * Redirects everything written to std::cout - where both the vsomeip logger and TEST_LOG emit -
 * into an in-memory, per-iteration buffer.
 *
 * Stress tests that rebuild the whole scenario in a loop produce one full trace per iteration; when
 * such a test fails on, say, iteration 37 the console is flooded with the 37 preceding (passing)
 * traces, which makes the failure hard to read. This helper keeps only the current iteration's
 * output and dumps it (to std::cerr, which is not captured) when the iteration fails, so the
 * failure shows just the relevant run.
 *
 * Usage:
 *   iteration_log_capture capture;            // installs the redirection (RAII)
 *   for (...) {
 *       capture.begin_iteration();            // drop the previous iteration's capture
 *       try {
 *           ... ASSERT_ / EXPECT_ macros ...
 *       } catch (...) {
 *           capture.dump();                   // emit only this iteration's trace
 *           throw;                            // let gtest handle the failure
 *       }
 *   }
 */
class iteration_log_capture {
public:
    iteration_log_capture() : original_(std::cout.rdbuf(&buffer_)) { }
    ~iteration_log_capture() { std::cout.rdbuf(original_); }

    iteration_log_capture(iteration_log_capture const&) = delete;
    iteration_log_capture& operator=(iteration_log_capture const&) = delete;

    // Discard the previously captured output.
    // Must be called at the start of every iteration.
    void begin_iteration() { buffer_.clear_captured(); }

    // Write the current iteration's captured output to std::cerr (which is not redirected).
    void dump() const {
        std::cerr << buffer_.captured();
        std::cerr.flush();
    }

private:
    class capturing_buf : public std::streambuf {
    public:
        void clear_captured() {
            std::lock_guard<std::mutex> lock(mutex_);
            data_.clear();
        }

        std::string captured() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return data_;
        }

    protected:
        std::streamsize xsputn(char const* s, std::streamsize n) override {
            std::lock_guard<std::mutex> lock(mutex_);
            data_.append(s, static_cast<size_t>(n));
            return n;
        }

        int_type overflow(int_type c) override {
            if (!traits_type::eq_int_type(c, traits_type::eof())) {
                std::lock_guard<std::mutex> lock(mutex_);
                data_.push_back(traits_type::to_char_type(c));
            }
            return traits_type::not_eof(c);
        }

    private:
        mutable std::mutex mutex_;
        std::string data_;
    };

    capturing_buf buffer_;
    std::streambuf* original_;
};

} // namespace common
