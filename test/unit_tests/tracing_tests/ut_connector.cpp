// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/primitive_types.hpp>

#include "../../../implementation/tracing/include/connector_impl.hpp"
#include "../../../implementation/tracing/include/channel_impl.hpp"
#include "../../../implementation/tracing/include/defines.hpp"
#include "../../../implementation/tracing/include/enumeration_types.hpp"
#include "../../../implementation/configuration/include/trace.hpp"
#include "../../../implementation/configuration/include/configuration_impl.hpp"
#include "../../../implementation/logger/include/logger_impl.hpp"

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

// These tests exercise the trace::connector_impl class: the trace() forwarding
// path (including the payload full-logging threshold), the channel and flag
// bookkeeping around it, and concurrent invocation of trace().
//
// In non-DLT builds trace() forwards matching messages to the vsomeip console
// logger, so the assertions inspect the text that trace() writes to std::cout.
//
// In DLT builds trace() forwards to the DLT backend instead and never writes to
// std::cout. The DLT output cannot be observed from a unit test, so there the
// same test cases only verify that the relevant code path executes without
// throwing and does not leak anything onto the console. Both variants are
// selected through the expect_* helpers below.

// connector_impl's sole friend (see connector_impl.hpp); must live directly in
// this namespace for the friend decl to resolve. Inline here since only this
// file needs it.
namespace vsomeip_v3::trace {
struct connector_impl_test_peer {
    static void set_enabled(connector_impl& _connector, bool _enabled) { _connector.set_enabled(_enabled); }

    static void set_sd_enabled(connector_impl& _connector, bool _enabled) { _connector.set_sd_enabled(_enabled); }

    static bool add_channel(connector_impl& _connector, const std::string& _id, const std::string& _description) {
        return _connector.add_channel(_id, _description);
    }

    static std::shared_ptr<channel_impl> get_channel(const connector_impl& _connector, const std::string& _id) {
        return _connector.get_channel(_id);
    }
};
} // namespace vsomeip_v3::trace

namespace {

using vsomeip_v3::byte_t;
using vsomeip_v3::instance_t;
using vsomeip_v3::method_t;
using vsomeip_v3::service_t;

using vsomeip_v3::ANY_INSTANCE;
using vsomeip_v3::ANY_METHOD;

using ::testing::HasSubstr;
using ::testing::Not;

using peer = vsomeip_v3::trace::connector_impl_test_peer;

constexpr uint16_t trace_header_size = 10; // VSOMEIP_TRACE_HEADER_SIZE
const std::string default_channel_id{"TC"}; // VSOMEIP_TC_DEFAULT_CHANNEL_ID

// RAII helper that redirects std::cout into an in-memory buffer for the
// lifetime of the object, so the output produced by trace() can be inspected.
class cout_capture {
public:
    cout_capture() : old_buffer_{std::cout.rdbuf(stream_.rdbuf())} { }

    ~cout_capture() { std::cout.rdbuf(old_buffer_); }

    cout_capture(const cout_capture&) = delete;
    cout_capture& operator=(const cout_capture&) = delete;

    std::string str() const { return stream_.str(); }

private:
    std::ostringstream stream_;
    std::streambuf* old_buffer_;
};

// A discarding stream buffer used to silence the (otherwise very noisy) console
// logging during the concurrency test. It is stateless, so concurrent writes
// through it do not introduce a data race of their own.
class null_streambuf : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize _n) override { return _n; }
    int overflow(int _c) override { return _c; }
};

// Redirects std::cout to a discarding buffer for its lifetime.
class scoped_cout_sink {
public:
    scoped_cout_sink() : old_buffer_{std::cout.rdbuf(&sink_)} { }

    ~scoped_cout_sink() { std::cout.rdbuf(old_buffer_); }

    scoped_cout_sink(const scoped_cout_sink&) = delete;
    scoped_cout_sink& operator=(const scoped_cout_sink&) = delete;

private:
    null_streambuf sink_;
    std::streambuf* old_buffer_;
};

// Builds a minimal SOME/IP message. Any additional trailing bytes requested via
// '_payload' are appended as payload after the 16 byte SOME/IP header.
std::vector<byte_t> make_message(service_t _service, method_t _method, const std::vector<byte_t>& _payload = {}) {
    // Sized up front instead of insert()-ing the payload: the reallocating
    // insert trips a -Wstringop-overread false positive on GCC 11 (aarch64).
    std::vector<byte_t> data(VSOMEIP_FULL_HEADER_SIZE + _payload.size(), 0x00);

    data[VSOMEIP_SERVICE_POS_MIN] = static_cast<byte_t>(_service >> 8);
    data[VSOMEIP_SERVICE_POS_MAX] = static_cast<byte_t>(_service & 0xff);
    data[VSOMEIP_METHOD_POS_MIN] = static_cast<byte_t>(_method >> 8);
    data[VSOMEIP_METHOD_POS_MAX] = static_cast<byte_t>(_method & 0xff);
    data[VSOMEIP_PROTOCOL_VERSION_POS] = 0x01;
    data[VSOMEIP_INTERFACE_VERSION_POS] = 0x01;

    std::copy(_payload.begin(), _payload.end(), data.begin() + VSOMEIP_FULL_HEADER_SIZE);
    return data;
}

// Builds a trace header carrying the given instance id at the expected position.
std::array<byte_t, trace_header_size> make_header(instance_t _instance) {
    std::array<byte_t, trace_header_size> header{};
    header[VSOMEIP_TC_INSTANCE_POS_MIN] = static_cast<byte_t>(_instance >> 8);
    header[VSOMEIP_TC_INSTANCE_POS_MAX] = static_cast<byte_t>(_instance & 0xff);
    return header;
}

// Distinctive marker written to the tail of a payload so that "full payload"
// versus "header only" logging can be told apart in non-DLT builds.
const std::string payload_marker_token = "de ad be ef";

std::vector<byte_t> payload_with_marker(size_t _size) {
    std::vector<byte_t> payload(_size, 0x00);
    if (_size >= 4) {
        payload[_size - 4] = 0xde;
        payload[_size - 3] = 0xad;
        payload[_size - 2] = 0xbe;
        payload[_size - 1] = 0xef;
    }
    return payload;
}

// A payload large enough that the whole message exceeds the default full-logging
// threshold, so the forward-everything default path logs it header-only.
std::vector<byte_t> oversized_payload() {
    return payload_with_marker(VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD + 64);
}

// Asserts that a message was forwarded by trace(). In non-DLT builds the given
// tokens must (or must not) appear in the captured console output; in DLT builds
// the message goes to the DLT backend, so we only require that nothing reached
// the console.
void expect_forwarded(const std::string& _output, const std::vector<std::string>& _present, const std::vector<std::string>& _absent = {}) {
#ifdef USE_DLT
    static_cast<void>(_present);
    static_cast<void>(_absent);
    EXPECT_TRUE(_output.empty());
#else
    EXPECT_FALSE(_output.empty());
    for (const auto& token : _present) {
        EXPECT_THAT(_output, HasSubstr(token));
    }
    for (const auto& token : _absent) {
        EXPECT_THAT(_output, Not(HasSubstr(token)));
    }
#endif
}

// Asserts that a message was dropped by trace(). This holds for both build
// variants: nothing must be written to the console.
void expect_dropped(const std::string& _output) {
    EXPECT_TRUE(_output.empty());
}

#ifndef USE_DLT
// Returns the console line that trace() produced for the given channel, or an
// empty string if that channel did not log anything. Each channel logs its own
// line prefixed with its id, which is what tells the per-channel output apart.
std::string channel_line(const std::string& _output, const std::string& _channel_id) {
    std::istringstream stream{_output};
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find(_channel_id + ":") != std::string::npos) {
            return line;
        }
    }
    return {};
}
#endif

class connector_test : public ::testing::Test {
protected:
    void SetUp() override {
        // trace() logs through the vsomeip console logger. A default constructed
        // configuration enables console logging at LL_INFO, which is required for
        // the log output to reach std::cout where the tests can capture it.
        auto configuration = std::make_shared<vsomeip_v3::cfg::configuration_impl>("");
        vsomeip_v3::logger::logger_impl::init(configuration);

#ifdef USE_DLT
        // In DLT builds the connector forwards traces to registered DLT contexts,
        // so an application has to be registered for the DLT calls to be valid.
        DLT_REGISTER_APP("VSIP", "vSomeIP tracing unit test");
#endif

        // A default configuration leaves the connector disabled; the individual
        // tests enable it and add channels/filters as needed.
        connector_ = std::make_shared<vsomeip_v3::trace::connector_impl>(std::make_shared<vsomeip_v3::cfg::trace>());
    }

    void TearDown() override {
        connector_.reset();
#ifdef USE_DLT
        DLT_UNREGISTER_APP();
#endif
    }

    // Traces a message and returns whatever trace() wrote to std::cout.
    std::string trace(const std::array<byte_t, trace_header_size>& _header, const std::vector<byte_t>& _data) {
        cout_capture capture;
        connector_->trace(_header.data(), static_cast<uint16_t>(_header.size()), _data.data(), static_cast<uint32_t>(_data.size()));
        return capture.str();
    }

    // Same as trace(), but for buffers whose size deliberately deviates from what
    // a well formed trace header / SOME/IP message would have.
    std::string trace_raw(const std::vector<byte_t>& _header, const std::vector<byte_t>& _data) {
        cout_capture capture;
        connector_->trace(_header.data(), static_cast<uint16_t>(_header.size()), _data.data(), static_cast<uint32_t>(_data.size()));
        return capture.str();
    }

    std::shared_ptr<vsomeip_v3::trace::connector_impl> connector_;
};

} // namespace

// A freshly created connector is disabled by default and must not produce any
// trace output.
TEST_F(connector_test, disabled_connector_produces_no_trace) {
    ASSERT_FALSE(connector_->is_enabled());

    const auto header = make_header(0x9abc);
    const auto data = make_message(0x1234, 0x5678, {0xde, 0xad});

    expect_dropped(trace(header, data));
}

// Enabling the connector but passing an empty payload must not produce output.
TEST_F(connector_test, empty_data_produces_no_trace) {
    peer::set_enabled(*connector_, true);

    const auto header = make_header(0x9abc);
    const std::vector<byte_t> empty_data;

    expect_dropped(trace(header, empty_data));
}

// With the default channel (no filters) an enabled connector traces the full
// header followed by the full payload.
TEST_F(connector_test, enabled_connector_traces_header_and_data) {
    peer::set_enabled(*connector_, true);

    const auto header = make_header(0x9abc);
    const auto data = make_message(0x1234, 0x5678, {0xde, 0xad});

    // The default channel id is used as prefix, followed by the instance from the
    // trace header, the service/method from the SOME/IP header and finally the
    // payload bytes that live beyond the 16 byte SOME/IP header.
    expect_forwarded(trace(header, data), {default_channel_id + std::string(":"), "9a bc", "12 34 56 78", "de ad"});
}

// Service discovery messages must be dropped while SD tracing is disabled
// (the connector default).
TEST_F(connector_test, sd_message_dropped_when_sd_disabled) {
    peer::set_enabled(*connector_, true);
    ASSERT_FALSE(connector_->is_sd_enabled());

    const auto header = make_header(0x0001);
    const auto data = make_message(0xffff, 0x8100);

    expect_dropped(trace(header, data));
}

// Once SD tracing is enabled, service discovery messages are traced as well.
TEST_F(connector_test, sd_message_traced_when_sd_enabled) {
    peer::set_enabled(*connector_, true);
    peer::set_sd_enabled(*connector_, true);

    const auto header = make_header(0x0001);
    const auto data = make_message(0xffff, 0x8100);

    expect_forwarded(trace(header, data), {"ff ff 81 00"});
}

// A negative filter must drop matching messages while still forwarding others.
TEST_F(connector_test, negative_filter_drops_matching_message) {
    peer::set_enabled(*connector_, true);

    auto channel = peer::get_channel(*connector_, default_channel_id);
    ASSERT_TRUE(channel);
    channel->add_filter(std::make_tuple(static_cast<service_t>(0x1111), ANY_INSTANCE, ANY_METHOD),
                        vsomeip_v3::trace::filter_type_e::NEGATIVE);

    const auto header = make_header(0x0001);

    // Matching message is dropped.
    expect_dropped(trace(header, make_message(0x1111, 0x0001)));

    // Non-matching message is still traced.
    expect_forwarded(trace(header, make_message(0x2222, 0x0001)), {"22 22"});
}

// A positive filter must trace only matching messages and drop the rest.
TEST_F(connector_test, positive_filter_traces_only_matching_message) {
    peer::set_enabled(*connector_, true);

    auto channel = peer::get_channel(*connector_, default_channel_id);
    ASSERT_TRUE(channel);
    channel->add_filter(std::make_tuple(static_cast<service_t>(0x3333), ANY_INSTANCE, ANY_METHOD),
                        vsomeip_v3::trace::filter_type_e::POSITIVE);

    const auto header = make_header(0x0001);

    // Matching message is traced.
    expect_forwarded(trace(header, make_message(0x3333, 0x0001)), {"33 33"});

    // Non-matching message is dropped.
    expect_dropped(trace(header, make_message(0x4444, 0x0001)));
}

// A header-only filter must trace the header and only the first
// VSOMEIP_FULL_HEADER_SIZE bytes of the data, truncating the payload.
TEST_F(connector_test, header_only_filter_truncates_payload) {
    peer::set_enabled(*connector_, true);

    auto channel = peer::get_channel(*connector_, default_channel_id);
    ASSERT_TRUE(channel);
    channel->add_filter(std::make_tuple(static_cast<service_t>(0x1234), ANY_INSTANCE, ANY_METHOD),
                        vsomeip_v3::trace::filter_type_e::HEADER_ONLY);

    const auto header = make_header(0x0001);
    // Payload bytes sit beyond the 16 byte SOME/IP header and must be truncated.
    const auto data = make_message(0x1234, 0x5678, {0xbe, 0xef, 0xca, 0xfe});

    // The SOME/IP header (within the first 16 data bytes) is still traced while
    // the truncated payload must not appear in the output.
    expect_forwarded(trace(header, data), {"12 34 56 78"}, {"be ef", "ca fe"});
}

// ---------------------------------------------------------------------------
// Enable / service-discovery flags
// ---------------------------------------------------------------------------

TEST_F(connector_test, enable_flag_round_trips) {
    EXPECT_FALSE(connector_->is_enabled());

    peer::set_enabled(*connector_, true);
    EXPECT_TRUE(connector_->is_enabled());

    peer::set_enabled(*connector_, false);
    EXPECT_FALSE(connector_->is_enabled());
}

TEST_F(connector_test, sd_flag_round_trips) {
    EXPECT_FALSE(connector_->is_sd_enabled());

    peer::set_sd_enabled(*connector_, true);
    EXPECT_TRUE(connector_->is_sd_enabled());

    peer::set_sd_enabled(*connector_, false);
    EXPECT_FALSE(connector_->is_sd_enabled());
}

// ---------------------------------------------------------------------------
// is_sd_message
// ---------------------------------------------------------------------------

TEST_F(connector_test, is_sd_message_detection) {
    const auto sd = make_message(0xffff, 0x8100);
    EXPECT_TRUE(connector_->is_sd_message(sd.data(), static_cast<uint16_t>(sd.size())));

    const auto non_sd = make_message(0x1234, 0x5678);
    EXPECT_FALSE(connector_->is_sd_message(non_sd.data(), static_cast<uint16_t>(non_sd.size())));

    // A buffer too short to even contain the method id is not an SD message.
    const std::array<byte_t, 2> too_short{0xff, 0xff};
    EXPECT_FALSE(connector_->is_sd_message(too_short.data(), static_cast<uint16_t>(too_short.size())));
}

// ---------------------------------------------------------------------------
// Channel management
// ---------------------------------------------------------------------------

TEST_F(connector_test, default_channel_exists) {
    EXPECT_TRUE(peer::get_channel(*connector_, default_channel_id));
    EXPECT_FALSE(peer::get_channel(*connector_, "does-not-exist"));
}

TEST_F(connector_test, add_channel_creates_unique_channel) {
    ASSERT_TRUE(peer::add_channel(*connector_, "CH1", "first channel"));

    auto channel = peer::get_channel(*connector_, "CH1");
    ASSERT_TRUE(channel);
    EXPECT_EQ(channel->get_id(), "CH1");

    // Re-adding an existing id (custom or default) must fail.
    EXPECT_FALSE(peer::add_channel(*connector_, "CH1", "duplicate"));
    EXPECT_FALSE(peer::add_channel(*connector_, default_channel_id, "duplicate default"));
}

// ---------------------------------------------------------------------------
// Construction from configuration
// ---------------------------------------------------------------------------

TEST_F(connector_test, constructor_applies_configuration) {
    auto config = std::make_shared<vsomeip_v3::cfg::trace>();
    config->is_enabled_ = true;
    config->is_sd_enabled_ = true;
    config->full_logging_threshold_ = 0; // 0 disables the threshold (always full)

    auto channel_cfg = std::make_shared<vsomeip_v3::cfg::trace_channel>();
    channel_cfg->id_ = "CFG";
    channel_cfg->name_ = "configured channel";
    config->channels_.push_back(channel_cfg);

    auto connector = std::make_shared<vsomeip_v3::trace::connector_impl>(config);

    EXPECT_TRUE(connector->is_enabled());
    EXPECT_TRUE(connector->is_sd_enabled());
    EXPECT_TRUE(peer::get_channel(*connector, "CFG"));

    // With the threshold disabled, an oversized default-path payload is still
    // logged in full.
    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, oversized_payload());

    cout_capture capture;
    connector->trace(header.data(), static_cast<std::uint16_t>(header.size()), data.data(), static_cast<std::uint32_t>(data.size()));
    expect_forwarded(capture.str(), {payload_marker_token});
}

// ---------------------------------------------------------------------------
// Full-logging threshold
// ---------------------------------------------------------------------------

TEST_F(connector_test, small_payload_logged_in_full_by_default) {
    peer::set_enabled(*connector_, true);

    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, {0xde, 0xad, 0xbe, 0xef});

    expect_forwarded(trace(header, data), {"12 34 56 78", payload_marker_token});
}

TEST_F(connector_test, oversized_default_payload_is_truncated_to_header) {
    peer::set_enabled(*connector_, true);

    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, oversized_payload());

    // On the forward-everything default path a payload beyond the threshold is
    // logged header-only: the SOME/IP header is present, the payload marker is not.
    expect_forwarded(trace(header, data), {"12 34 56 78"}, {payload_marker_token});
}

TEST_F(connector_test, positive_filter_bypasses_threshold) {
    peer::set_enabled(*connector_, true);

    auto channel = peer::get_channel(*connector_, default_channel_id);
    ASSERT_TRUE(channel);
    channel->add_filter(std::make_tuple(static_cast<service_t>(0x1234), ANY_INSTANCE, ANY_METHOD),
                        vsomeip_v3::trace::filter_type_e::POSITIVE);

    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, oversized_payload());

    // An explicit positive filter forces full logging regardless of the threshold.
    expect_forwarded(trace(header, data), {payload_marker_token});
}

TEST_F(connector_test, full_payload_filter_forces_full_logging) {
    peer::set_enabled(*connector_, true);

    auto channel = peer::get_channel(*connector_, default_channel_id);
    ASSERT_TRUE(channel);
    channel->add_filter(std::make_tuple(static_cast<service_t>(0x1234), ANY_INSTANCE, ANY_METHOD),
                        vsomeip_v3::trace::filter_type_e::FULL_PAYLOAD);

    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, oversized_payload());

    // A full-payload filter forces full logging just like a positive filter.
    expect_forwarded(trace(header, data), {payload_marker_token});
}

// ---------------------------------------------------------------------------
// Per-channel logging decisions
// ---------------------------------------------------------------------------

// Whether a message is logged in full or header-only is decided per channel, so
// one channel truncating its output must not affect any other channel. The
// channels are iterated in id order, hence the ids chosen here: the truncating
// channel is visited before the one that expects the full payload.
TEST_F(connector_test, header_only_channel_does_not_truncate_later_channel) {
    peer::set_enabled(*connector_, true);

    ASSERT_TRUE(peer::add_channel(*connector_, "AAA", "header-only channel"));
    auto truncating_channel = peer::get_channel(*connector_, "AAA");
    ASSERT_TRUE(truncating_channel);
    truncating_channel->add_filter(std::make_tuple(static_cast<service_t>(0x1234), ANY_INSTANCE, ANY_METHOD),
                                   vsomeip_v3::trace::filter_type_e::HEADER_ONLY);

    // The default channel has no filters and the payload stays below the
    // threshold, so it must log the message in full.
    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, {0xde, 0xad, 0xbe, 0xef});

    const auto output = trace(header, data);
#ifdef USE_DLT
    EXPECT_TRUE(output.empty());
#else
    const auto truncated_output = channel_line(output, "AAA");
    const auto full_output = channel_line(output, default_channel_id);
    ASSERT_FALSE(truncated_output.empty());
    ASSERT_FALSE(full_output.empty());

    EXPECT_THAT(truncated_output, HasSubstr("12 34 56 78"));
    EXPECT_THAT(truncated_output, Not(HasSubstr(payload_marker_token)));
    EXPECT_THAT(full_output, HasSubstr(payload_marker_token));
#endif
}

// Mirror of the test above with the roles swapped: a channel that logs in full
// must not widen the output of a header-only channel visited after it.
TEST_F(connector_test, full_channel_does_not_widen_later_header_only_channel) {
    peer::set_enabled(*connector_, true);

    ASSERT_TRUE(peer::add_channel(*connector_, "ZZZ", "header-only channel"));
    auto truncating_channel = peer::get_channel(*connector_, "ZZZ");
    ASSERT_TRUE(truncating_channel);
    truncating_channel->add_filter(std::make_tuple(static_cast<service_t>(0x1234), ANY_INSTANCE, ANY_METHOD),
                                   vsomeip_v3::trace::filter_type_e::HEADER_ONLY);

    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, {0xde, 0xad, 0xbe, 0xef});

    const auto output = trace(header, data);
#ifdef USE_DLT
    EXPECT_TRUE(output.empty());
#else
    const auto truncated_output = channel_line(output, "ZZZ");
    const auto full_output = channel_line(output, default_channel_id);
    ASSERT_FALSE(truncated_output.empty());
    ASSERT_FALSE(full_output.empty());

    EXPECT_THAT(truncated_output, Not(HasSubstr(payload_marker_token)));
    EXPECT_THAT(full_output, HasSubstr(payload_marker_token));
#endif
}

// The same independence must hold for the size threshold: a channel taking the
// forward-everything default path truncates an oversized payload, while a
// channel with an explicit positive filter still logs it in full - regardless of
// which of the two is visited first.
TEST_F(connector_test, threshold_truncation_does_not_affect_positive_filter_channel) {
    peer::set_enabled(*connector_, true);

    ASSERT_TRUE(peer::add_channel(*connector_, "ZZZ", "positive filter channel"));
    auto full_channel = peer::get_channel(*connector_, "ZZZ");
    ASSERT_TRUE(full_channel);
    full_channel->add_filter(std::make_tuple(static_cast<service_t>(0x1234), ANY_INSTANCE, ANY_METHOD),
                             vsomeip_v3::trace::filter_type_e::POSITIVE);

    const auto header = make_header(0x0001);
    const auto data = make_message(0x1234, 0x5678, oversized_payload());

    const auto output = trace(header, data);
#ifdef USE_DLT
    EXPECT_TRUE(output.empty());
#else
    const auto truncated_output = channel_line(output, default_channel_id);
    const auto full_output = channel_line(output, "ZZZ");
    ASSERT_FALSE(truncated_output.empty());
    ASSERT_FALSE(full_output.empty());

    EXPECT_THAT(truncated_output, Not(HasSubstr(payload_marker_token)));
    EXPECT_THAT(full_output, HasSubstr(payload_marker_token));
#endif
}

// ---------------------------------------------------------------------------
// Undersized buffers
// ---------------------------------------------------------------------------

// trace() reads the service and method ids from the SOME/IP header; data too
// short to carry them must be dropped rather than read out of bounds.
TEST_F(connector_test, undersized_data_is_dropped) {
    peer::set_enabled(*connector_, true);

    const std::vector<byte_t> header(trace_header_size, 0x00);

    // One byte short of holding the method id.
    const std::vector<byte_t> short_data{0x12, 0x34, 0x56};
    expect_dropped(trace_raw(header, short_data));

    // Service and method ids complete: this is the smallest data still traced.
    const std::vector<byte_t> minimal_data{0x12, 0x34, 0x56, 0x78};
    expect_forwarded(trace_raw(header, minimal_data), {"12 34 56 78"});
}

// The instance id is read from the trace header; a header too short to carry it
// must be dropped rather than read out of bounds.
TEST_F(connector_test, undersized_header_is_dropped) {
    peer::set_enabled(*connector_, true);

    const auto data = make_message(0x1234, 0x5678, {0xde, 0xad});

    // One byte short of holding the instance id.
    const std::vector<byte_t> short_header(VSOMEIP_TC_INSTANCE_POS_MAX, 0x00);
    expect_dropped(trace_raw(short_header, data));

    // Instance id complete: traced again.
    const std::vector<byte_t> minimal_header(VSOMEIP_TC_INSTANCE_POS_MAX + 1, 0x00);
    expect_forwarded(trace_raw(minimal_header, data), {"12 34 56 78"});
}

// ---------------------------------------------------------------------------
// Concurrency
// ---------------------------------------------------------------------------

// Invokes trace() from several threads at once. With a correct implementation
// all shared state is accessed under the connector's locks, so this is race
// free; a faulty implementation (e.g. dropping a lock or introducing shared
// scratch state) would be flagged by ThreadSanitizer or corrupt memory.
//
// Only trace() is exercised concurrently: construction, configuration and
// channel handling are intentionally left single threaded.
TEST_F(connector_test, concurrent_trace_calls_are_race_free) {
    peer::set_enabled(*connector_, true);
    peer::set_sd_enabled(*connector_, true);

    // Shared, read-only inputs. Sharing them across threads also asserts that
    // trace() never writes to its input buffers.
    const auto header = make_header(0x0001);
    const std::vector<std::vector<byte_t>> messages{
            make_message(0x1234, 0x5678, {0xde, 0xad}), // small -> full payload
            make_message(0xffff, 0x8100), // service discovery
            make_message(0x1234, 0x5678, oversized_payload()), // large -> header only
    };

    // Silence the (otherwise very noisy) per-message console logging.
    scoped_cout_sink silence;

    constexpr int thread_count = 8;
    constexpr int iterations = 500;
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&] {
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < iterations; ++i) {
                const auto& message = messages[static_cast<size_t>(i) % messages.size()];
                connector_->trace(header.data(), static_cast<uint16_t>(header.size()), message.data(),
                                  static_cast<uint32_t>(message.size()));
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto& thread : threads) {
        thread.join();
    }

    // Reaching this point without a ThreadSanitizer report or crash is the real
    // assertion; the connector must remain usable afterwards.
    EXPECT_TRUE(connector_->is_enabled());
}
