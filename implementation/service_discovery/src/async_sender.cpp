// Copyright (C) 2024-2025 Valeo India Private Limited
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//
//  /file       async_sender.cpp
//
//  /brief      This file defines a class implements a queue,
//              and functions for timeout for the messages.
//              On timeout, the client of this class will
//              receive a callback.
//

#include "../include/async_sender.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <memory>

#include "../../../interface/vsomeip/internal/logger.hpp"

namespace async
{
    namespace sender
    {
        async_sender::~async_sender()
        {
            stop();
        }

        void async_sender::start()
        {
            if (is_running_.load())
            {
                VSOMEIP_TRACE << "async::sender thread already running, stopping previous thread..";
                stop();
            }
            is_running_.store(true);
            minWait_ = 0;
            thread_ = std::thread(&async_sender::worker, this);
        }

        void async_sender::stop()
        {
            is_running_.store(false);
            cv_.notify_all(); // Notify the worker thread to stop
            if (thread_.joinable())
            {
                thread_.join();
            }
            queue_ = {};
        }

        void async_sender::add_queue(std::shared_ptr<async_packet_data> _pkt)
        {
            VSOMEIP_TRACE << "async::sender add element";
            {
                std::lock_guard<std::mutex> lock(mtx_);
                queue_.push(std::move(_pkt));
            }
            cv_.notify_one();
        }

        void async_sender::set_packet_sender_callback(std::shared_ptr<async_packet_send_callback> _callback)
        {
            callback_ = _callback;
        }

        void async_sender::worker()
        {
            VSOMEIP_TRACE << "async::sender thread started";
            while (is_running_.load())
            {
                std::unique_lock<std::mutex> lock(mtx_);
                if (minWait_)
                {
                    cv_.wait_for(lock, std::chrono::microseconds(minWait_), [this]
                             { return !is_running_.load(); });
                }
                else
                {
                    cv_.wait(lock, [this]
                             { return !queue_.empty() || !is_running_.load(); });
                }
                if (!is_running_.load())
                {
                    break; // Exit the loop if the sender is stopped
                }
                minWait_ = 0;
                iterate_over_queue();
            }
            VSOMEIP_TRACE << "async::sender thread exited";
        }

        void async_sender::handle_packet(std::shared_ptr<async_packet_data> _pkt)
        {
            if (callback_)
            {
                callback_->on_async_send_pkt(_pkt);
            }
            else
            {
                VSOMEIP_ERROR << "async::sender Callback not set ";
            }
        }

        void async_sender::iterate_over_queue()
        {
            time_point now = std::chrono::steady_clock::now();
            while (!queue_.empty())
            {
                auto delay = queue_.top()->timeout_;
                if (now >= delay)
                {
                    VSOMEIP_TRACE << "async::sender thread pkt timeout";
                    handle_packet( queue_.top());
                    queue_.pop();
                }
                else
                {
                    std::chrono::duration<uint64_t, std::micro> duration = std::chrono::duration_cast<std::chrono::microseconds>(delay - now);
                    minWait_ = duration.count();
                    VSOMEIP_TRACE << "async::sender thread will wait for "<<minWait_;
                    break;
                }
            }
        }
    }
}
