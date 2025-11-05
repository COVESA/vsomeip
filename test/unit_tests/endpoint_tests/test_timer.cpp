// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "test_timer.hpp"

namespace vsomeip_v3::testing {
using namespace std::chrono_literals;

std::shared_ptr<fake_factory> test_timer_base::factory_{std::make_shared<fake_factory>()};

struct test_timer_with_fake : public test_timer_base {

    test_timer_with_fake() { factory_->timer_ = std::make_unique<fake_timer>(timer_state_); }

    uint32_t start_count() { return timer_state_->start_count_; }
    std::optional<std::chrono::milliseconds> interval() { return timer_state_->timeout_; }
    boost::system::error_code execute_handler() {
        boost::system::error_code ec;
        timer_state_->handler_(ec);
        return ec;
    }

    boost::asio::io_context dummy_context_;
    std::shared_ptr<timer_state> timer_state_{std::make_shared<timer_state>()};
};

struct test_example_use_case : public test_timer_with_fake {
    // Test cases with this fixture showcase the usage of the timer utility,
    // but don't really test the implementation of the timer itself
};

TEST_F(test_example_use_case, a_const_shared_ptr_of_timer_is_thread_safe) {
    class ExampleOwner : public std::enable_shared_from_this<ExampleOwner> {
        struct hidden { }; // private struct ensures that ::create is used for a proper setup
    public:
        static std::shared_ptr<ExampleOwner> create(boost::asio::io_context& _io) {
            auto p = std::make_shared<ExampleOwner>(hidden{}, _io);
            // 1. ensures that task is properly set before any actual usage of the timer.
            // 2. capture this by weak_ref to avoid the need of manual destruction of the timer.
            // Note: This step can not be done in the c'tor as the weak_from_this/shared_from_this call would return a nullptr at this
            // point.
            // Note: This two step setup is inevitable as the task of the timer and the owner of the timer form an ownership cycle (although
            // only temporary due to the usage of weak_from_this).

            // the return can be ignored in this very instance, because the timer had no chance to have been
            // started before. The return is given, to highlight that adjusting the timers task at a later
            // stage is a dangerous operation and has a chance to fail (because the timer class itself will
            // protect against the change of the task while the timer is running).
            [[maybe_unused]] bool set_was_success = p->timer_->set_task([weak_ref = p->weak_from_this()] {
                if (auto self = weak_ref.lock(); self) {
                    self->counter_ += 1;
                }
                return true;
            });
            return p;
        }

        ExampleOwner([[maybe_unused]] hidden _, boost::asio::io_context& _io) :
            // create a timer with a dummy task to ensure the shared_ptr is receiving its memory in the ctor and can therefore be
            // used from any thread without further inspection
            timer_(timer::create(_io, 12ms, [] { return false; })) { }

        // Because the timer has a thread-safe interface, the shared_ptr to the timer is const, and the task is setup is ensured
        // in a dedicated create function there is no need for synchronization primitives when using the timer itself.
        void start() { timer_->start(); }
        void stop() { timer_->stop(); }

        std::atomic<uint32_t> counter_{0};

    private:
        std::shared_ptr<timer> const timer_;
    };

    auto owner = ExampleOwner::create(dummy_context_);
    // start the timer
    owner->start();
    ASSERT_EQ(1, start_count());
    ASSERT_EQ(2, timer_state_.use_count()); // 1 from the fixture + 1 from the timer

    // because the owner passed in a weak_ref, releasing the owner releases the timer without further action
    owner = nullptr;
    ASSERT_EQ(1, timer_state_.use_count()); // 1 from the fixture
}
using return_channel = std::function<void(bool)>;
// For explanation of the ownership model refer to the test before
class TimeboxedOperation : public std::enable_shared_from_this<TimeboxedOperation> {
    struct hidden { };

public:
    static std::shared_ptr<TimeboxedOperation> create(boost::asio::io_context& io_, return_channel channel_) {
        auto p = std::make_shared<TimeboxedOperation>(hidden{}, io_, std::move(channel_));
        (void)p->timer_->set_task([weak_ref = p->weak_from_this()] {
            if (auto self = weak_ref.lock(); self) {
                self->timer_expired();
            }
            return false;
        });
        return p;
    }
    TimeboxedOperation([[maybe_unused]] hidden, boost::asio::io_context& _io, return_channel channel_) :
        timer_(timer::create(_io, 12ms, [] { return false; })), answer_(std::move(channel_)) { }

    void start() {
        // place holder comment. Here the async operation would be started
        // that would call async_return
        timer_->start();
    }

    void async_return() {
        // check whether the timer task was executed already
        if (c_.fetch_add(1) > 0) {
            return;
        }
        answer_(true);
    }

private:
    void timer_expired() {
        // check async_return was executed already
        if (c_.fetch_add(1) > 0) {
            return;
        }
        answer_(false);
    }
    void answer(bool success) { answer_(success); }
    std::atomic<int> c_{0};
    std::shared_ptr<timer> const timer_;
    return_channel const answer_;
};

static constexpr int async_finished = 1;
static constexpr int timer_expired = 2;
TEST_F(test_example_use_case, use_timer_as_timebox_async_finishes) {
    int result{0};
    auto t1 = TimeboxedOperation::create(dummy_context_, [&](bool success) { result = success ? async_finished : timer_expired; });

    // start the timer
    t1->start();
    // before the timer elapses the timeboxed event occurrs:
    t1->async_return();
    EXPECT_EQ(result, async_finished);
}
TEST_F(test_example_use_case, use_timer_as_timebox_timer_expires) {
    int result{0};
    auto t2 = TimeboxedOperation::create(dummy_context_, [&](bool success) { result = success ? async_finished : timer_expired; });

    // start the timer
    t2->start();
    // let the timer expire
    execute_handler();
    EXPECT_EQ(result, timer_expired);
}

TEST_F(test_timer_with_fake, init) {
    auto timer = timer::create(dummy_context_, 12ms, [] { return false; });
}
TEST_F(test_timer_with_fake, starting_a_timer_will_set_the_interval_and_wait) {
    auto input = 2min; // arbitrary
    auto timer = timer::create(dummy_context_, input, [] { return false; });

    ASSERT_EQ(0, start_count());
    timer->start();

    EXPECT_EQ(input, interval());
    EXPECT_EQ(1, start_count());
}
TEST_F(test_timer_with_fake, a_one_shot_timer_is_not_restarted) {
    auto timer = timer::create(dummy_context_, 2s, [] { return false; });
    ASSERT_EQ(0, start_count());
    timer->start();
    ASSERT_EQ(1, start_count());

    execute_handler();

    EXPECT_EQ(1, start_count());
}
TEST_F(test_timer_with_fake, a_one_shot_timer_that_starts_itself_is_restarted) {
    std::shared_ptr<timer> timer;
    timer = timer::create(dummy_context_, 2s, [&] {
        timer->start();
        return false;
    });
    ASSERT_EQ(0, start_count());
    timer->start();
    ASSERT_EQ(1, start_count());

    execute_handler();

    EXPECT_EQ(2, start_count());
}
TEST_F(test_timer_with_fake, a_one_shot_timer_that_is_started_twice_is_restarted) {
    auto timer = timer::create(dummy_context_, 2s, [] { return false; });
    ASSERT_EQ(0, start_count());
    timer->start();
    ASSERT_EQ(1, start_count());
    timer->start();
    ASSERT_EQ(2, start_count());

    execute_handler();

    EXPECT_EQ(2, start_count());
}
TEST_F(test_timer_with_fake, a_repeating_timer_is_restarted) {
    auto timer = timer::create(dummy_context_, 2s, [] { return true; });
    ASSERT_EQ(0, start_count());
    timer->start();
    ASSERT_EQ(1, start_count());

    for (int i = 0; i < 6; ++i) {
        execute_handler();
        EXPECT_EQ(i + 2, start_count());
    }
}
TEST_F(test_timer_with_fake, a_timer_that_stops_itself_within_the_task_is_not_restarted) {
    std::shared_ptr<timer> timer;
    // notice that the timer would be restarted, if the stop would not be processed
    timer = timer::create(dummy_context_, 2s, [&] {
        timer->stop();
        return true;
    });

    ASSERT_EQ(0, start_count());
    timer->start();
    ASSERT_EQ(1, start_count());
    execute_handler();

    EXPECT_EQ(1, start_count());
}

struct test_timer_with_asio : public test_timer_base {
    test_timer_with_asio() { factory_->timer_ = std::make_unique<asio_timer>(io_); }
    boost::asio::io_context io_;
};

TEST_F(test_timer_with_asio, a_timer_can_be_restarted_before_the_cancellation_is_internally_handled) {

    uint32_t execution_count{0};
    auto timer = timer::create(io_, 500ms /*arbitrary, but big enough that start, stop sequence below is done before expiration*/, [&] {
        ++execution_count;
        return true;
    });
    timer->start();
    timer->stop();
    timer->start();
    io_.poll_one();
    std::this_thread::sleep_for(501ms);
    io_.poll_one();
    EXPECT_EQ(1, execution_count);
}

TEST_F(test_timer_with_asio, a_stopped_timer_will_not_execute_the_handler) {
    uint32_t execution_count{0};
    auto timer = timer::create(io_, 10ms, [&] {
        ++execution_count;
        return true;
    });

    ASSERT_EQ(0, execution_count);
    timer->start();
    std::this_thread::sleep_for(11ms);
    io_.poll_one();
    ASSERT_EQ(1, execution_count);
    std::this_thread::sleep_for(11ms);
    io_.poll_one();
    ASSERT_EQ(2, execution_count);
    std::this_thread::sleep_for(11ms);
    io_.poll_one();
    ASSERT_EQ(3, execution_count);
    timer->stop();
    std::this_thread::sleep_for(11ms);
    io_.poll_one();

    EXPECT_EQ(3, execution_count);
}

TEST_F(test_timer_with_asio, a_timer_stopped_during_execution_from_another_thread_is_not_restarted) {
    std::mutex mtx;
    std::condition_variable cv;
    int step{0};
    uint32_t execution_count{0};
    auto timeout = 5ms;

    // the timer notifies the stopper to stop the timer and waits for the stop
    // to have been called.
    auto timer = timer::create(io_, timeout, [&] {
        std::unique_lock lock{mtx};
        ++execution_count;
        ++step;
        cv.notify_one();
        lock.unlock();
        cv.wait_for(lock, 5s, [&] { return step == 2; });
        return true;
    });

    auto stopper = std::thread([&] {
        std::unique_lock lock{mtx};
        cv.wait_for(lock, 5s, [&] { return step == 1; });
        timer->stop();
        ++step;
        cv.notify_one();
    });

    timer->start();
    std::this_thread::sleep_for(timeout + 5ms);
    io_.poll_one();
    std::this_thread::sleep_for(timeout + 5ms);
    io_.poll_one();

    EXPECT_EQ(execution_count, 1);
    if (stopper.joinable()) {
        stopper.join();
    }
}
}
