// Copyright (C) 2024-2025 Valeo India Private Limited
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//
//  /file       async_sender.hpp
//
//  /brief      This file implements declares a class which
//              implements a queue, and functions for timeout
//              for the messages. On timeout, the client of
//              this class will receive a callback.
//


#pragma once

#include <memory>
#include <thread>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <atomic>
#include <vector>

#include "message_impl.hpp"

namespace async
{
    namespace sender
    {
        using time_point = std::chrono::steady_clock::time_point;

        struct async_packet_data
        {
            std::vector<std::shared_ptr<vsomeip_v3::sd::message_impl>> messages_;
            boost::asio::ip::address address_;
            time_point timeout_;
        };

        class async_packet_send_callback
        {
        public:
            virtual ~async_packet_send_callback() = default;
            virtual void on_async_send_pkt(std::shared_ptr<async_packet_data>) = 0;
        };

        class time_point_comparator
        {
        public:
            bool operator()(std::shared_ptr<async_packet_data> a, std::shared_ptr<async_packet_data> b)
            {
                return a->timeout_ > b->timeout_;
            }
        };

        using priority_queue = std::priority_queue<
            std::shared_ptr<async_packet_data>,
            std::vector<std::shared_ptr<async_packet_data>>,
            time_point_comparator>;

        class async_sender
        {
        public:
            async_sender() = default;
            ~async_sender();
            void add_queue(std::shared_ptr<async_packet_data> _pkt);
            void set_packet_sender_callback(std::shared_ptr<async_packet_send_callback> callback);
            void start();
            void stop();
            inline bool check_if_running() { return is_running_.load(); };

        private:
            void worker();
            void handle_packet(std::shared_ptr<async_packet_data> _pkt);
            void iterate_over_queue();

        private:
            std::atomic<bool> is_running_;
            std::thread thread_;
            std::condition_variable cv_;
            std::mutex mtx_;
            priority_queue queue_;
            std::shared_ptr<async_packet_send_callback> callback_;
            uint64_t minWait_;
        };
    }
}
