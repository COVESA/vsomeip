// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <common/utility.hpp>

#include "mocked_vsomeip_dependencies.hpp"

struct usei_fixture : public ::testing::Test {
    std::shared_ptr<boost::asio::io_context> context_;
    std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::thread mainloop_;
    std::shared_ptr<vsomeip_v3::cfg::configuration_impl> conf_;
    std::shared_ptr<vsomeip_v3::udp_server_endpoint_impl> server_;
    std::shared_ptr<mock_endpoint_host> endpoint_;
    std::shared_ptr<mock_routing_host> routing_;
    boost::asio::ip::udp::endpoint unicast_parameters_{boost::asio::ip::make_address("127.0.0.1"), 5000};
    boost::asio::ip::udp::endpoint multicast_parameters_{boost::asio::ip::make_address("0.0.0.0"), 5000};
    boost::asio::ip::udp::endpoint tester_parameters_{boost::asio::ip::make_address("127.0.0.2"), 5002};
    int udp_socket_{-1};

    void SetUp() override {
        // Boost ASIO main loop
        context_ = std::make_shared<boost::asio::io_context>();
        work_guard_ = std::make_shared<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(context_->get_executor());

        mainloop_ = std::thread([this] { context_->run(); });

        // VSOMEIP objects
        conf_ = std::make_shared<vsomeip_v3::cfg::configuration_impl>("usei_fixture.json");
        endpoint_ = std::make_shared<mock_endpoint_host>();
        routing_ = std::make_shared<mock_routing_host>();
        server_ = std::make_shared<vsomeip_v3::udp_server_endpoint_impl>(endpoint_, routing_, *context_, conf_);

        // UDP tester socket
        udp_socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        ASSERT_GE(udp_socket_, 0);

        struct sockaddr_in tester_address { };
        tester_address.sin_family = AF_INET;
        tester_address.sin_addr.s_addr = htonl(tester_parameters_.address().to_v4().to_uint());
        tester_address.sin_port = htons(tester_parameters_.port());

        int result = ::bind(udp_socket_, reinterpret_cast<const struct sockaddr*>(&tester_address), sizeof(tester_address));
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        // UDP tester socket
        ::close(udp_socket_);

        // VSOMEIP objects
        server_.reset();
        routing_.reset();
        endpoint_.reset();

        // Boost ASIO main loop
        context_->stop();
        mainloop_.join();
        work_guard_.reset();
        context_.reset();
    }

    void send(const boost::asio::ip::udp::endpoint& target, const std::byte* buffer, std::size_t len) {
        struct sockaddr_in target_address { };

        target_address.sin_family = AF_INET;
        target_address.sin_addr.s_addr = htonl(target.address().to_v4().to_uint());
        target_address.sin_port = htons(target.port());

        ssize_t result =
                ::sendto(udp_socket_, buffer, len, 0, reinterpret_cast<const struct sockaddr*>(&target_address), sizeof(target_address));
        ASSERT_EQ(result, len);
    }

    template<size_t N>
    void send(const boost::asio::ip::udp::endpoint& target, const std::array<std::byte, N>& data) {
        send(target, data.data(), N);
    }
};

// https://stackoverflow.com/questions/45172052/correct-way-to-initialize-a-container-of-stdbyte
template<typename... Ts>
std::array<std::byte, sizeof...(Ts)> make_bytes(Ts&&... args) noexcept {
    return {std::byte(std::forward<Ts>(args))...};
}

extern "C" int __real_setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);
extern "C" int __wrap_setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen);

int __wrap_setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen) {
    if (level == IPPROTO_IP && (optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP || optname == IP_PKTINFO)) {
        return 0;
    }

    return __real_setsockopt(sockfd, level, optname, optval, optlen);
}

using namespace testing;

TEST_F(usei_fixture, basic) {
    using namespace std::chrono_literals;

    std::mutex sync;
    bool received{false};
    std::condition_variable event;

    EXPECT_CALL(*routing_, on_message)
            .WillOnce([&](const vsomeip_v3::byte_t*, vsomeip_v3::length_t, vsomeip_v3::boardnet_endpoint*, bool, vsomeip_v3::client_t,
                          const vsomeip_sec_client_t*, const boost::asio::ip::address&, uint16_t) {
                std::unique_lock lock(sync);
                received = true;
                event.notify_one();
            });

    boost::system::error_code error;
    server_->init(unicast_parameters_, error);
    server_->start();

    send(unicast_parameters_, make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x08, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C));

    std::unique_lock lock(sync);
    EXPECT_EQ(event.wait_for(lock, 5s, [&] { return received; }), true);

    server_->stop(false);
}

TEST_F(usei_fixture, corrupted_data) {
    using namespace std::chrono_literals;

    auto good_data = make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x08, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                                0x0F, 0x10, 0x00, 0x00, 0x00, 0x09, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19);
    auto end_message = make_bytes(0x1A, 0x1B, 0x1C, 0x1D, 0x00, 0x00, 0x00, 0x08, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x025);

    constexpr size_t MESSAGE_SENT_COUNT = 10000;
    std::mutex sync;
    bool received{false};
    std::condition_variable event;

    EXPECT_CALL(*endpoint_, on_error).Times(AtLeast(MESSAGE_SENT_COUNT / 40));
    EXPECT_CALL(*routing_, on_message)
            .WillRepeatedly([&](const vsomeip_v3::byte_t* data, vsomeip_v3::length_t len, vsomeip_v3::boardnet_endpoint*, bool,
                                vsomeip_v3::client_t, const vsomeip_sec_client_t*, const boost::asio::ip::address&, uint16_t) {
                if (len == 16 && data[0] == 0x1A && data[1] == 0x1B && data[2] == 0x1C && data[3] == 0x1D) {
                    std::unique_lock lock(sync);
                    received = true;
                    event.notify_one();
                }
            });

    boost::system::error_code error;
    server_->init(unicast_parameters_, error);
    server_->start();

    for (size_t i = 0; i < MESSAGE_SENT_COUNT; ++i) {
        auto bad_data = good_data;
        bad_data[i % bad_data.size()] = static_cast<std::byte>(i);
        send(unicast_parameters_, bad_data);
        std::this_thread::sleep_for(10us);
    }

    send(unicast_parameters_, end_message);

    std::unique_lock lock(sync);
    EXPECT_EQ(event.wait_for(lock, 5s, [&] { return received; }), true);

    server_->stop(false);
}

TEST_F(usei_fixture, basic_multicast) {
    using namespace std::chrono_literals;

    std::mutex sync;
    bool received{false};
    std::condition_variable event;

    EXPECT_CALL(*routing_, on_message)
            .WillOnce([&](const vsomeip_v3::byte_t*, vsomeip_v3::length_t, vsomeip_v3::boardnet_endpoint*, bool, vsomeip_v3::client_t,
                          const vsomeip_sec_client_t*, const boost::asio::ip::address&, uint16_t) {
                std::unique_lock lock(sync);
                received = true;
                event.notify_one();
            });

    boost::system::error_code error;
    server_->init(unicast_parameters_, error);
    server_->start();
    server_->set_multicast_option(multicast_parameters_.address(), true, error);

    send(multicast_parameters_,
         make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x09, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D));

    std::unique_lock lock(sync);
    EXPECT_EQ(event.wait_for(lock, 5s, [&] { return received; }), true);

    server_->joined_.clear(); // We don't want to call `leave` method, so we do its job
    server_->set_multicast_option(multicast_parameters_.address(), false, error);
    server_->stop(false);
}

TEST_F(usei_fixture, no_overwrite_during_restart) {
    
    // Before the fix, this test was able to reproduce the following race condition:
    // when the server was restarted, if there were still messages to be unstacked
    // from the Linux buffers, the old asynchronous task unstacking these messages
    // continued to run and, since it shared the same buffer with the new task, this
    // corrupted the data. The problem is that this condition occurs at restart (so
    // messages are lost) and that Linux can too discard UDP messages. This makes it
    // difficult to detect the problem

    constexpr size_t MESSAGE_SENT_COUNT = 10000;
    using namespace std::chrono_literals;
    std::atomic<bool> finished{false};

    EXPECT_CALL(*endpoint_, on_error).Times(0);
    EXPECT_CALL(*routing_, on_message).Times(AtLeast(100));

    boost::system::error_code error;
    server_->init(unicast_parameters_, error);
    server_->start();

    auto producer = std::thread([&] {
        for (size_t i = 0; i < MESSAGE_SENT_COUNT; ++i) {
            send(unicast_parameters_,
                 make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x08, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C));
            send(unicast_parameters_,
                 make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x09, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D));
            std::this_thread::sleep_for(10us);
        }
        finished.store(true);
    });

    while (!finished.load()) {
        server_->restart(true);
        std::this_thread::sleep_for(1ms);
    }

    producer.join();
    server_->stop(false);
}

TEST_F(usei_fixture, no_overwrite_during_restart_with_multicast) {
    constexpr size_t MESSAGE_SENT_COUNT = 1000000;
    using namespace std::chrono_literals;
    std::atomic<bool> finished{false};

    EXPECT_CALL(*endpoint_, on_error).Times(0);
    EXPECT_CALL(*endpoint_, add_multicast_option).Times(AtLeast(5));
    EXPECT_CALL(*routing_, on_message).Times(AtLeast(1000));

    boost::system::error_code error;
    server_->init(unicast_parameters_, error);
    server_->start();
    server_->set_multicast_option(multicast_parameters_.address(), true, error);

    auto producer = std::thread([&] {
        for (size_t i = 0; i < MESSAGE_SENT_COUNT; ++i) {
            send(unicast_parameters_,
                 make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x08, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C));
            send(multicast_parameters_,
                 make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x09, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D));
            send(unicast_parameters_,
                 make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E));
            send(multicast_parameters_,
                 make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x0B, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                            0x0F));
        }
        finished.store(true);
    });

    while (!finished.load()) {
        server_->restart(true);
        std::this_thread::sleep_for(1ms);
    }

    server_->joined_.clear();
    server_->set_multicast_option(multicast_parameters_.address(), false, error);
    producer.join();
    server_->stop(false);
}

struct usei_multi_fixture : public usei_fixture {
    std::thread worker_; // extra thread (in addition to `mainloop_`)

    void SetUp() override {
        usei_fixture::SetUp();

        conf_->load(""); // to enable logging
        worker_ = std::thread([this] { context_->run(); });
    }

    void TearDown() override {
        auto ctx = context_; // hold context alive
        usei_fixture::TearDown();

        worker_.join(); // must be after `context_->stop()`
    }
};

TEST_F(usei_multi_fixture, multicast_ordering_rejoin) {
    /// check that multicast messages are read in the correct order
    /// specifically after there are usei multicast leave/join; we can only break this order if we somehow
    /// (concurrently) read from the multicast socket


    using namespace std::chrono_literals;

    std::mutex mutex;
    std::function<void(const vsomeip_v3::byte_t* data, vsomeip_v3::length_t len)> on_message_cbk;

    EXPECT_CALL(*routing_, on_message)
            .WillRepeatedly([&mutex, &on_message_cbk](const vsomeip_v3::byte_t* data, vsomeip_v3::length_t len,
                                                      vsomeip_v3::boardnet_endpoint*, bool, vsomeip_v3::client_t,
                                                      const vsomeip_sec_client_t*, const boost::asio::ip::address&, uint16_t) {
                std::unique_lock lock(mutex);
                if (auto fn = on_message_cbk) {
                    lock.unlock();
                    fn(data, len);
                }
            });

    boost::system::error_code error;
    server_->init(unicast_parameters_, error);
    server_->start();
    server_->set_multicast_option(multicast_parameters_.address(), true, error);

    // hell of a hack, but need to stop the unicast socket, otherwise messages go to it and not the multicast socket
    // (note the `__wrap_setsockopt` code..)
    server_->unicast_socket_.reset();

    std::condition_variable cv;
    std::vector<std::vector<uint8_t>> received;

    // setup a callback that will rejoin the multicast group
    // unrealistic, but this hits exactly the "window" where the previous multicast socket read-loop will wrongly use the new socket
    {
        std::unique_lock lock(mutex);
        on_message_cbk = [this, &mutex, &cv, &received](const vsomeip_v3::byte_t*, vsomeip_v3::length_t) {
            std::unique_lock lock(mutex);
            boost::system::error_code error;
            // same as ::leave
            server_->joined_.clear();
            server_->set_multicast_option(multicast_parameters_.address(), false, error);
            // same as ::join
            server_->set_multicast_option(multicast_parameters_.address(), true, error);

            received.push_back({0x00});
            cv.notify_one();
        };
    }

    // send + wait for message
    {
        send(multicast_parameters_,
             make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00));

        std::unique_lock lock(mutex);
        std::vector<std::vector<uint8_t>> expected{{0x00}};
        cv.wait_for(lock, 30s, [&expected, &received] { return expected == received; });
        ASSERT_EQ(expected, received);
    }

    // setup new callback
    {
        std::unique_lock lock(mutex);
        received.clear();
        on_message_cbk = [&mutex, &cv, &received](const vsomeip_v3::byte_t* data, vsomeip_v3::length_t len) {
            static std::atomic<size_t> count{0};
            size_t c = ++count;
            VSOMEIP_INFO << "callback #" << c << " - enter";
            std::unique_lock lock(mutex);
            received.push_back(std::vector<uint8_t>(data, data + len));
            cv.notify_one();
            VSOMEIP_INFO << "callback #" << c << " - exit";
        };
    }

    // send + wait for messages
    {
        std::vector<std::vector<uint8_t>> expected;
        for (size_t i = 0; i < 256; ++i) {
            // does not matter, just has to be >= 16 bytes; note that only last byte varies
            auto bytes = make_bytes(0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    static_cast<uint8_t>(i));
            send(multicast_parameters_, bytes);
            expected.push_back(
                    std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&bytes[0]), reinterpret_cast<uint8_t*>(&bytes[0]) + bytes.size()));
        }

        std::unique_lock lock(mutex);
        cv.wait_for(lock, 30s, [&expected, &received] { return expected == received; });
        ASSERT_EQ(expected, received);
    }

    server_->joined_.clear(); // We don't want to call `leave` method, so we do its job
    server_->set_multicast_option(multicast_parameters_.address(), false, error);
    server_->stop(false);
}
