// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <stdexcept>
#include <utility>

namespace common {

namespace bip = boost::interprocess;

const size_t DEFAULT_SHSIZE_ = 65536; // 64kb

// ----------------------------------------------------------------------------------------
// bounded_channel_t
// ----------------------------------------------------------------------------------------

/**
 * @brief One-to-one bounded single-value channel for interprocess communication suitable for shared memory.
 * @tparam T Trivially copyable type to send/receive.
 * @invariant Only one value can be stored at a time. Supports only one producer and one consumer.
 * @note Use send/receive methods for communication. Waits if full/empty.
 */
template<typename T, size_t Size = sizeof(T)>
struct bounded_channel_t {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for shared memory");
    using pair_t = std::pair<bool, T>;

    void send(T value) {
        bip::scoped_lock<bip::interprocess_mutex> lock(mutex_);

        while (has_data_) {
            auto timeout_point = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(100);
            bool result = empty_condition_.timed_wait(lock, timeout_point, [this]() { return !has_data_; });
            if (!result) {
                continue; // timeout
            }
        }

        data_ = value;
        has_data_ = true;
        full_condition_.notify_all();
    }

    T receive() {
        bip::scoped_lock<bip::interprocess_mutex> lock(mutex_);

        while (!has_data_) {
            auto timeout_point = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(100);
            bool result = full_condition_.timed_wait(lock, timeout_point, [this]() { return has_data_; });
            if (!result) {
                continue; // timeout
            }
        }

        T value = data_;
        has_data_ = false;
        empty_condition_.notify_all();
        return value;
    }

    T receive(size_t timeout_ms) {
        bip::scoped_lock<bip::interprocess_mutex> lock(mutex_);
        auto start = boost::posix_time::microsec_clock::universal_time();
        while (!has_data_) {
            auto now = boost::posix_time::microsec_clock::universal_time();
            auto elapsed_ms = (now - start).total_milliseconds();
            if (elapsed_ms >= static_cast<long>(timeout_ms)) {
                return T{0};
            }
            auto timeout_point = now + boost::posix_time::milliseconds(100);
            bool result = full_condition_.timed_wait(lock, timeout_point, [this]() { return has_data_; });
            if (!result) {
                continue;
            }
        }
        T value = data_;
        has_data_ = false;
        empty_condition_.notify_all();
        return value;
    }

private:
    bip::interprocess_mutex mutex_;
    bip::interprocess_condition empty_condition_;
    bip::interprocess_condition full_condition_;
    T data_;
    bool has_data_{false};
};

// ----------------------------------------------------------------------------------------
// barrier_t
// ----------------------------------------------------------------------------------------

/**
 * @brief Barrier for synchronizing multiple processes in shared memory.
 * @invariant Number of expected processes must be set before use.
 * @note Use wait_unlocked() to synchronize all participants.
 */

struct barrier_t {
    explicit barrier_t(size_t count) : count_(count), threshold_(count) {
        if (count == 0) {
            throw std::invalid_argument("Count cannot be zero");
        }
        count_ = count;
        threshold_ = count;
        generation_ = 0;
    }

    /**
     * @brief Wait at the barrier until all expected processes arrive.
     */
    bool wait() {
        bip::scoped_lock<bip::interprocess_mutex> lock{mutex_};
        size_t gen = generation_;
        if (--count_ == 0) {
            generation_++;
            count_ = threshold_;
            cond_.notify_all();
            return true;
        }

        while (gen == generation_) {
            cond_.wait(lock);
        }
        return false;
    }

    void reset() {
        bip::scoped_lock<bip::interprocess_mutex> lock{mutex_};
        count_ = threshold_;
        generation_++;
    }

private:
    size_t count_{0};
    size_t threshold_{0};
    size_t generation_{0};
    boost::interprocess::interprocess_mutex mutex_;
    boost::interprocess::interprocess_condition cond_;
};

// ----------------------------------------------------------------------------------------
// shared_memory_t & shared_memory_master_t & shared_memory_slave_t
// ----------------------------------------------------------------------------------------

/**
 * @brief Shared memory region for interprocess communication, with message queues and barrier.
 * @tparam T Type of message for the channel.
 * @invariant Not copyable. Use master/slave variants for creation/attachment.
 * @note Provides access to per-process queues and synchronization barrier.
 */

template<typename T>
struct shared_memory_t {
    using message_queue_t = bounded_channel_t<T>;

    std::string test_name_;
    bip::managed_shared_memory shm_;
    bip::mapped_region region_;
    barrier_t* barrier_ = nullptr;
    message_queue_t* queues_ = nullptr;

    explicit shared_memory_t(bip::create_only_t flag, std::string test_name) :
        test_name_(std::move(test_name)), shm_(bip::managed_shared_memory(flag, test_name_.c_str(), DEFAULT_SHSIZE_)) { }

    explicit shared_memory_t(bip::open_only_t flag, std::string test_name) :
        test_name_(std::move(test_name)), shm_(bip::managed_shared_memory(flag, test_name_.c_str())) { }

    bounded_channel_t<T>& get_queue(size_t index) { return queues_[index]; }

    ~shared_memory_t() = default;
    shared_memory_t(const shared_memory_t&) = delete;
    shared_memory_t& operator=(const shared_memory_t&) = delete;

    void wait_next_step() { this->barrier_->wait(); }
};

template<typename T>
struct shared_memory_master_t : shared_memory_t<T> {
    explicit shared_memory_master_t(std::string test_name, size_t n_processes) : shared_memory_t<T>(bip::create_only, test_name) {

        this->queues_ = this->shm_.template construct<bounded_channel_t<T>>("queues_")[n_processes]();
        this->barrier_ = this->shm_.template construct<barrier_t>("barrier_")(n_processes);
    }

    shared_memory_master_t(const shared_memory_master_t&) = delete;
    shared_memory_master_t& operator=(const shared_memory_master_t&) = delete;
    ~shared_memory_master_t() { boost::interprocess::shared_memory_object::remove(this->test_name_.c_str()); }
};

template<typename T>
struct shared_memory_slave_t : shared_memory_t<T> {
    explicit shared_memory_slave_t(std::string test_name) : shared_memory_t<T>(bip::open_only, test_name) {

        this->queues_ = this->shm_.template find<bounded_channel_t<T>>("queues_").first;
        this->barrier_ = this->shm_.template find<barrier_t>("barrier_").first;
    }

    shared_memory_slave_t(const shared_memory_slave_t&) = delete;
    shared_memory_slave_t& operator=(const shared_memory_slave_t&) = delete;
    ~shared_memory_slave_t() = default;
};

} // namespace common