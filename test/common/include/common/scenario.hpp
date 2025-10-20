// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <common/interprocess.hpp>
#include <string_view>

namespace common {
enum class Steps : uint8_t {
    _1 = 1,
    _2,
    _3,
    _4,
    _5,
    _6,
    _7,
    _8,
    _9,
    _10,
    _11,
    _12,
    _13,
    _14,
    _15,
    _16,
    _17,
    _18,
    _19,
    _20,
    _21,
    _22,
    _23,
    _24,
    _25,
    _26,
    _27,
    _28,
    _29,
    _30,
    _31,
    _32,
    _33,
    _34,
    _35,
    _36,
    _37,
    _38,
    _39,
    _40,
    _41,
    _42,
    _43,
    _44,
    _45,
    _46,
    _47,
    _48,
    _49,
    _50,
};
template<typename T>
/**
 * @brief Test scenario runner for ordered, synchronized test steps across processes.
 * @tparam T Type used for shared memory communication.
 * @invariant Number of steps fixed at construction. Steps must be added before run().
 * @note Use add_step() to define steps, then run() to execute in order.
 */
class test_scenario_t {
    struct TestStep {
        std::function<void()> action;
        std::string name;
        void execute(common::shared_memory_t<T>& shm) const {
            if (!name.empty()) {
                std::cout << name << std::endl;
            }
            action();
            shm.wait_next_step();
        }
    };

    static inline const TestStep EMPTY_STEP = {[]() {}, ""};
    std::vector<TestStep> steps;
    size_t expected_steps;
    size_t next_sequential_index{0};

public:
    /**
     * @brief Deleted default constructor.
     */
    test_scenario_t() = delete;
    /**
     * @brief Construct a scenario with a fixed number of steps.
     * @param expected_number_of_steps Number of steps in the scenario.
     */
    explicit test_scenario_t(size_t expected_number_of_steps) :
        steps(expected_number_of_steps, EMPTY_STEP), expected_steps(expected_number_of_steps) { }

    /**
     * @brief Add a step with a name and action at the next sequential index.
     * @param name Step name.
     * @param action Function to execute for the step.
     * @return Reference to this scenario.
     */
    test_scenario_t& add_step(std::string name, std::function<void()> action) {
        if (next_sequential_index >= expected_steps) {
            throw std::runtime_error("Cannot add more steps than expected (" + std::to_string(expected_steps) + ")");
        }
        steps[next_sequential_index++] = {action, name};
        return *this;
    }

    /**
     * @brief Add a step at a specific index (by enum), with name and action.
     * @param step Step enum value (1-based).
     * @param name Step name.
     * @param action Function to execute for the step.
     * @return Reference to this scenario.
     */
    test_scenario_t& add_step(Steps step, std::string name, std::function<void()> action) {
        const auto index = static_cast<size_t>(step) - 1;
        if (index >= expected_steps) {
            throw std::runtime_error("Step index " + std::to_string(index) + " exceeds expected steps (" + std::to_string(expected_steps)
                                     + ")");
        }
        steps[index] = {action, name};
        if (index >= next_sequential_index) {
            next_sequential_index = index;
        }
        return *this;
    }

    /**
     * @brief Run all scenario steps in order, synchronizing via shared memory.
     * @param shm Shared memory object for synchronization.
     */
    void run(common::shared_memory_t<T>& shm) {
        for (auto& step : steps) {
            step.execute(shm); // Vector is pre-filled with EMPTY_STEP
        }
    }

    void run(common::shared_memory_t<T>& shm, std::string_view who) {
        auto i = 0;
        for (auto& step : steps) {
            std::cout << who << " ON STEP=" << i << std::endl;
            step.execute(shm); // Vector is pre-filled with EMPTY_STEP
            i++;
        }
    }
};
} // namespace common
