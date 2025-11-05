// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TIMER_HPP_
#define VSOMEIP_V3_TIMER_HPP_

#include "abstract_timer.hpp"

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <chrono>

namespace vsomeip_v3 {

/**
 *  Convenience class to schedule a task based on the abstract_timer.
 **/
class timer : public std::enable_shared_from_this<timer> {
private:
    struct hidden { };

public:
    using task_t = std::function<bool()>;

    /**
     * Use timer::create instead
     **/
    explicit timer(hidden _h, std::unique_ptr<abstract_timer> _timer, std::chrono::milliseconds _interval, task_t _task);

    ~timer();

    /**
     * Creates a timer that is ready to be started.
     *
     * @param _interval, time to wait before executing the passed in _task.
     * @param _task, if task returns true, the timer will restart itself.
     *
     * Note:
     * When the timer managed by the shared_ptr is deleted the timer is stopped.
     * But the timer is guaranteed to outlive the execution of any running task
     * (a task is not considered running, when the timer is currently awaiting
     * the interval to expire).
     * To avoid circular shared_ptr ownership (requiring to manually clear timer handles),
     * it is advised to capture weak references to the used objects within the task.
     * Warning:
     * The timer will not check the validity of the handed in task
     **/
    static std::shared_ptr<timer> create(boost::asio::io_context& _io, std::chrono::milliseconds _interval, task_t _task);

    /**
     * Starts the timer. If no further action is taken the following sequence can be expected:
     * 1. awaiting the timeout
     * 2. copy the shared_ptr of the timer
     * 3. execute the task
     * 4. if the task return true -> restart the timer, else stop
     * 5. delete copy of the shared_ptr
     *
     * If the timer had been started before then:
     * 1. If the timer is still waiting, the timer is restarted.
     * 2. If the timer is currently waiting for the execution of the task to finish, then the timer will be
     *    restarted afterwards irrespective of the return of the task itself.
     **/
    void start();

    /**
     * Tries to stop the running timer.
     * If the timer is currently waiting to expire: The task will not be scheduled.
     * If the timer is currently executing the task: After the task is done the timer will not be
     * restarted, irrespective of the return of the task itself.
     **/
    void stop();

    /**
     * Whether the timer was started and did not finish the execution of the task yet.
     * Note: In case the timer should be stopped, calling stop is sufficient
     **/
    [[nodiscard]] bool is_running() const;

    /**
     * Adjusts the interval of the timer.
     * Only allowed if the timer is currently stopped.
     *
     * @return false, if the timer is currently trying to execute the task, or started
     **/
    [[nodiscard]] bool set_interval(std::chrono::milliseconds _interval);

    /**
     * Adjusts the task the timer stores.
     * Beware about the notes in to timer::create about object lifetime.
     *
     * @return false, if the timer is currently trying to execute the task, or started
     **/
    [[nodiscard]] bool set_task(task_t _task);

private:
    enum class state_e { STOPPED, STARTED, IN_TASK, IN_TASK_STARTED, IN_TASK_STOPPED };

    void stop_unlocked();
    void start_unlocked();

    void handle(boost::system::error_code const& _ec);

    state_e state_{state_e::STOPPED};
    std::unique_ptr<abstract_timer> timer_;
    std::chrono::milliseconds interval_;
    task_t task_;
    mutable std::mutex mtx_;
};
}

#endif
