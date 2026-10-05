// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "mock_configuration.hpp"
#include "../../../implementation/configuration/include/configuration_plugin_impl.hpp"
#include "../../../implementation/security/include/policy_manager_impl.hpp"

#include <common/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <atomic>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using ::testing::_;
using ::testing::Return;
using vsomeip_v3::configuration_plugin_impl;
using vsomeip_v3::policy_manager_impl;
using vsomeip_v3::cfg::configuration_impl;
using vsomeip_v3::testing::mock_configuration;

#if (defined(__linux__) || defined(__QNX__))

// RAII guard that captures the current value of an environment variable on
// construction and restores it (or removes it) on destruction.
struct env_guard {
    explicit env_guard(const std::string& name) : name_(name) {
        const char* current = std::getenv(name.c_str());
        if (current) {
            prev_value_ = current;
            had_value_ = true;
        }
    }
    ~env_guard() {
        if (had_value_) {
            setenv(name_.c_str(), prev_value_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }
    void set(const char* value) { setenv(name_.c_str(), value, 1); }

private:
    const std::string name_;
    std::string prev_value_;
    bool had_value_ = false;
};

#endif

// Runs _body(i) on _thread_count threads, released as simultaneously as the
// scheduler allows: every thread reports ready and then spins on a common gate,
// so the work actually overlaps instead of being serialised by thread creation.
template<typename F>
void run_concurrently(unsigned _thread_count, F _body) {
    std::atomic<unsigned> its_ready{0};
    std::atomic<bool> its_gate{false};
    std::vector<std::thread> its_workers;

    its_workers.reserve(_thread_count);
    for (unsigned i = 0; i < _thread_count; ++i) {
        its_workers.emplace_back([&_body, &its_ready, &its_gate, i]() {
            its_ready.fetch_add(1);
            while (!its_gate.load()) {
                std::this_thread::yield();
            }
            _body(i);
        });
    }

    while (its_ready.load() < _thread_count) {
        std::this_thread::yield();
    }
    its_gate.store(true);

    for (auto& w : its_workers) {
        w.join();
    }
}

class test_plugin : public configuration_plugin_impl {
public:
    // Pre-queue a mock to be returned by the next make_configuration() call.
    // The caller sets EXPECT_CALL on it before triggering get_configuration().
    std::deque<std::shared_ptr<mock_configuration>> mock_queue_;

protected:
    std::shared_ptr<vsomeip_v3::cfg::configuration_impl> make_configuration(const std::string&) override {
        if (mock_queue_.empty()) {
            ADD_FAILURE() << "test_plugin ran out of pre-queued mocks";
            throw std::runtime_error("test_plugin ran out of pre-queued mocks");
        }
        auto cfg = mock_queue_.front();
        mock_queue_.pop_front();
        return cfg;
    }
};

struct test_configuration_plugin : public ::testing::Test {
    void SetUp() override { plugin_ = std::make_unique<test_plugin>(); }

    // Creates a mock with a permissive default for load() and queues it.
    // Returns the mock so the caller can layer additional EXPECT_CALLs on top.
    std::shared_ptr<mock_configuration> push_mock() {
        auto m = std::make_shared<mock_configuration>();
        ON_CALL(*m, load(_)).WillByDefault(Return(true));
        plugin_->mock_queue_.push_back(m);
        return m;
    }

    std::unique_ptr<test_plugin> plugin_;
};

// The number of distinct configuration objects a plugin creates is observable
// through the mock queue: make_configuration() pops one queued mock per object
// it builds. Pushing exactly the expected number of mocks means an unexpected
// extra object trips make_configuration()'s "ran out of mocks" check, and a
// drained queue at the end confirms no fewer were created. Object *sharing* is
// asserted directly by comparing the returned shared_ptrs.

// A single application creates exactly one configuration object.
TEST_F(test_configuration_plugin, single_app_creates_one_config) {
    auto m = push_mock();

    auto its_config = plugin_->get_configuration("app1", "/path");

    EXPECT_EQ(its_config, m); // the created object is returned
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // exactly one object created
}

// Asking twice for the same app name and the same path resolves to the same
// cache key and therefore to the same configuration object.
TEST_F(test_configuration_plugin, same_app_name_same_path_returns_same_config) {
    auto m = push_mock();

    auto its_first = plugin_->get_configuration("app1", "/path");
    auto its_second = plugin_->get_configuration("app1", "/path");

    EXPECT_EQ(its_first, its_second); // same object
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // only one object created
}

// The cache is keyed purely by the resolved configuration source, not by the
// application name: the same name asking for a different path gets the
// configuration belonging to that other path.
TEST_F(test_configuration_plugin, same_app_name_different_path_gets_separate_config) {
    auto m1 = push_mock();
    auto m2 = push_mock();

    auto its_first = plugin_->get_configuration("app1", "/path_a");
    auto its_second = plugin_->get_configuration("app1", "/path_b");

    EXPECT_EQ(its_first, m1);
    EXPECT_EQ(its_second, m2);
    EXPECT_NE(its_first, its_second); // distinct objects, one per path
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // exactly two objects created
}

// Two applications that resolve to the same cache key (same path, no env vars)
// share a single configuration object.
TEST_F(test_configuration_plugin, two_apps_same_path_share_one_config) {
    auto m = push_mock();

    auto its_app1 = plugin_->get_configuration("app1", "/shared_path");
    auto its_app2 = plugin_->get_configuration("app2", "/shared_path");

    EXPECT_EQ(its_app1, its_app2); // same object shared
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // only one object created
}

// A configuration is parsed exactly once per cache key: an application handed an
// already-cached object does not trigger another load().
TEST_F(test_configuration_plugin, shared_config_is_loaded_only_once) {
    auto m = push_mock();
    EXPECT_CALL(*m, load("app1")).Times(1);
    EXPECT_CALL(*m, load("app2")).Times(0);

    auto its_app1 = plugin_->get_configuration("app1", "/shared_path");
    auto its_app2 = plugin_->get_configuration("app2", "/shared_path");

    EXPECT_EQ(its_app1, its_app2); // still one shared object
}

// Two applications with different paths get their own, distinct configuration
// objects.
TEST_F(test_configuration_plugin, two_apps_different_paths_get_separate_configs) {
    auto m1 = push_mock();
    auto m2 = push_mock();

    auto its_app1 = plugin_->get_configuration("app1", "/path_a");
    auto its_app2 = plugin_->get_configuration("app2", "/path_b");

    EXPECT_NE(its_app1, its_app2); // distinct objects
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // exactly two objects created
}

#if (defined(__linux__) || defined(__QNX__))

// VSOMEIP_CONFIGURATION (global) makes all apps share one configuration object
// regardless of the path argument.
TEST_F(test_configuration_plugin, global_env_var_causes_shared_config) {
    env_guard g("VSOMEIP_CONFIGURATION");
    g.set("/nonexistent_global_cfg");

    auto m = push_mock();

    auto its_app1 = plugin_->get_configuration("app1", "/path_a");
    auto its_app2 = plugin_->get_configuration("app2", "/path_b");

    EXPECT_EQ(its_app1, its_app2); // shared via the global env var
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // only one object created
}

// VSOMEIP_CONFIGURATION_<name> pointing to different paths gives each app its
// own configuration object.
TEST_F(test_configuration_plugin, per_app_env_var_causes_separate_configs) {
    env_guard g1("VSOMEIP_CONFIGURATION_app1");
    env_guard g2("VSOMEIP_CONFIGURATION_app2");
    g1.set("/nonexistent_cfg_app1");
    g2.set("/nonexistent_cfg_app2");

    auto m1 = push_mock();
    auto m2 = push_mock();

    auto its_app1 = plugin_->get_configuration("app1", "");
    auto its_app2 = plugin_->get_configuration("app2", "");

    EXPECT_NE(its_app1, its_app2); // distinct objects
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // exactly two objects created
}

// VSOMEIP_CONFIGURATION_<name> takes priority over VSOMEIP_CONFIGURATION:
// app1 uses its per-app key, app2 falls back to the global key, so they resolve
// to different keys and therefore different objects.
TEST_F(test_configuration_plugin, per_app_env_var_overrides_global_env_var) {
    env_guard gg("VSOMEIP_CONFIGURATION");
    env_guard g1("VSOMEIP_CONFIGURATION_app1");
    gg.set("/nonexistent_global_cfg");
    g1.set("/nonexistent_cfg_app1_only");

    auto m1 = push_mock();
    auto m2 = push_mock();

    auto its_app1 = plugin_->get_configuration("app1", ""); // keyed by per-app env var
    auto its_app2 = plugin_->get_configuration("app2", ""); // keyed by global env var

    EXPECT_NE(its_app1, its_app2); // distinct objects
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // exactly two objects created
}

// Two apps whose per-app env vars point to the same path share one
// configuration object.
TEST_F(test_configuration_plugin, two_apps_same_per_app_env_var_share_config) {
    env_guard g1("VSOMEIP_CONFIGURATION_app1");
    env_guard g2("VSOMEIP_CONFIGURATION_app2");
    g1.set("/nonexistent_shared_cfg");
    g2.set("/nonexistent_shared_cfg");

    auto m = push_mock();

    auto its_app1 = plugin_->get_configuration("app1", "");
    auto its_app2 = plugin_->get_configuration("app2", "");

    EXPECT_EQ(its_app1, its_app2); // same key — shared object
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // only one object created
}

#endif

// clear_configurations() drops the cache, so the next request parses again. The
// object handed out earlier stays valid — the caller still owns it — but it is
// no longer shared with newcomers.
TEST_F(test_configuration_plugin, clear_configurations_forces_reparse) {
    auto m1 = push_mock();
    auto its_first = plugin_->get_configuration("app1", "/path");

    plugin_->clear_configurations();

    auto m2 = push_mock();
    auto its_second = plugin_->get_configuration("app2", "/path");

    EXPECT_EQ(its_second, m2);
    EXPECT_NE(its_second, its_first); // a fresh object after the cache was cleared
    EXPECT_TRUE(plugin_->mock_queue_.empty()); // exactly two objects created
}

// Applications are created concurrently in a process, so get_configuration() is
// entered from several threads at once. Resolving to the same cache key must
// build and load exactly one object, with the later threads handed the object the
// first one created rather than parsing another.
TEST_F(test_configuration_plugin, concurrent_get_configuration_creates_one_shared_config) {
    constexpr unsigned its_thread_count{8};

    auto m = push_mock();
    // Queued as a trap, not as an expectation: make_configuration() pops one mock
    // per object it builds, so a lost race that builds a second object consumes
    // this one and leaves the queue empty.
    push_mock();

    std::vector<std::shared_ptr<vsomeip_v3::configuration>> its_results(its_thread_count);
    run_concurrently(its_thread_count, [&](unsigned _index) {
        its_results[_index] = plugin_->get_configuration("app" + std::to_string(_index), "/shared_path");
    });

    for (const auto& r : its_results) {
        EXPECT_EQ(r, m); // every thread got the one shared object
    }
    EXPECT_EQ(plugin_->mock_queue_.size(), 1u); // exactly one object created
}

// configuration_impl itself: real load() runs against hand-written files

class scratch_config_dir {
public:
    scratch_config_dir() : path_(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("vsomeip_cfg_test_%%%%-%%%%")) {
        boost::filesystem::create_directories(path_);
    }

    ~scratch_config_dir() {
        boost::system::error_code ec;
        boost::filesystem::remove_all(path_, ec);
    }

    void write(const std::string& _filename, const std::string& _content) const {
        std::ofstream its_file((path_ / _filename).string());
        its_file << _content;
    }

    std::string path() const { return path_.string(); }

private:
    boost::filesystem::path path_;
};

vsomeip_sec_client_t make_tcp_client(uid_t _uid, gid_t _gid) {
    // Any non-zero port marks the peer as a TCP connection: there is no UDS
    // credential to check, so check_routing_credentials must let it through.
    return vsomeip_sec_client_t{_uid, _gid, 0, 12345};
}

struct test_configuration_impl : ::testing::Test {
    std::shared_ptr<configuration_impl> load(const scratch_config_dir& _dir, const std::string& _app_name) {
        auto its_config = std::make_shared<configuration_impl>(_dir.path());
        its_config->set_configuration_path(_dir.path());
        EXPECT_TRUE(its_config->load(_app_name));
        // Mirrors application_impl::determine_routing_host(): the election is
        // decided on the mandatory view, and the optional configuration is
        // exclusive to the routing host. load() itself already parsed
        // everything up front if the routing block wasn't resolvable from the
        // mandatory data alone.
        if (its_config->get_routing_host_name() == _app_name) {
            its_config->load_optional();
        }
        return its_config;
    }
};

// A plain application (not the routing host) parses only the mandatory
// configuration files. Optional files are exclusive to the routing manager, so
// their content is not visible to it.
TEST_F(test_configuration_impl, plain_application_skips_optional_files) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ]
    })");
    // Arbitrary file name -> not part of VSOMEIP_MANDATORY_CONFIGURATION_FILES,
    // so its content is only picked up by the optional-elements read pass.
    dir.write("extra_optional.json", R"({ "unicast": "10.10.10.10" })");

    auto its_config = load(dir, "client_app");

    // Optional file not read -> unicast keeps its default value.
    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("127.0.0.1"));
}

// The routing manager parses the full configuration, including optional files.
TEST_F(test_configuration_impl, routing_manager_loads_optional_files) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ]
    })");
    dir.write("extra_optional.json", R"({ "unicast": "10.10.10.10" })");

    auto its_config = load(dir, "routingmanager");

    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("10.10.10.10"));
}

// If the routing block itself lives in a non-mandatory file, the mandatory
// pass alone cannot resolve the routing host, so load() must eagerly parse
// everything on its own - with no explicit load_optional() call - so that
// the caller can still run the routing-host election on the result.
TEST_F(test_configuration_impl, load_alone_resolves_routing_host_in_non_mandatory_file) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ]
    })");
    // Arbitrary file name -> not part of VSOMEIP_MANDATORY_CONFIGURATION_FILES,
    // so the routing block is invisible to the mandatory-only pass.
    dir.write("extra_optional.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "unicast": "10.10.10.10"
    })");

    auto its_config = std::make_shared<configuration_impl>(dir.path());
    its_config->set_configuration_path(dir.path());

    EXPECT_TRUE(its_config->load("routingmanager"));

    EXPECT_EQ(its_config->get_routing_host_name(), "routingmanager");
    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("10.10.10.10"));
}

// The shared configuration is loaded once per process, but if a plain
// application primes it first (mandatory only) and the routing manager takes
// over the same object later, load_optional() reads the optional files then -
// regardless of which application loaded it first.
TEST_F(test_configuration_impl, client_primed_config_is_completed_by_load_optional) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ]
    })");
    dir.write("extra_optional.json", R"({ "unicast": "10.10.10.10" })");

    auto its_config = std::make_shared<configuration_impl>(dir.path());
    its_config->set_configuration_path(dir.path());

    // Plain client loads first: optional file not read yet.
    EXPECT_TRUE(its_config->load("client_app"));
    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("127.0.0.1"));

    // Routing manager takes over the same object: optional file is read now.
    its_config->load_optional();
    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("10.10.10.10"));

    // Idempotent: a second call is a no-op.
    its_config->load_optional();
    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("10.10.10.10"));
}

// The elected routing host parses the optional configuration inside its own
// init(), while the other applications of the process are still running theirs
// and reading the same object.
TEST_F(test_configuration_impl, concurrent_reads_during_load_optional_see_stable_mandatory_values) {
    constexpr unsigned its_reader_count{7};
    constexpr unsigned its_reads_per_thread{2000};

    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "unicast": "10.10.10.10",
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [
            {
                "name": "client_app",
                "id": "0x2222",
                "max_dispatchers": "5",
                "max_dispatch_time": "1234",
                "has_session_handling": "false"
            }
        ]
    })");
    dir.write("extra_optional.json", R"({
        "services": [ { "service": "0x1234", "instance": "0x0001", "unreliable": "30509" } ]
    })");

    auto its_config = std::make_shared<configuration_impl>(dir.path());
    its_config->set_configuration_path(dir.path());
    ASSERT_TRUE(its_config->load("client_app"));

    // Counted rather than EXPECT_EQ'd inside the threads: a mismatch is reported
    // once, from the main thread, instead of thousands of times from all of them.
    std::atomic<unsigned> its_mismatches{0};

    // Thread 0 is the elected host running the parse, the rest are applications
    // reading their own settings while it does.
    run_concurrently(its_reader_count + 1, [&](unsigned _index) {
        if (_index == 0) {
            its_config->load_optional();
            return;
        }
        for (unsigned i = 0; i < its_reads_per_thread; ++i) {
            if (its_config->get_id("client_app") != 0x2222) {
                ++its_mismatches;
            }
            if (its_config->get_max_dispatchers("client_app") != 5u) {
                ++its_mismatches;
            }
            if (its_config->get_max_dispatch_time("client_app") != 1234u) {
                ++its_mismatches;
            }
            if (its_config->has_session_handling("client_app")) {
                ++its_mismatches;
            }
            if (its_config->get_routing_host_name() != "routingmanager") {
                ++its_mismatches;
            }
        }
    });

    EXPECT_EQ(its_mismatches.load(), 0u);
    // The readers did race against real work, not against an already-done parse.
    EXPECT_EQ(its_config->get_unicast_address(), boost::asio::ip::make_address("10.10.10.10"));
    EXPECT_EQ(its_config->get_unreliable_port(0x1234, 0x0001), 30509);
}

// Same window as above, on the security path: load_optional() appends the
// optional elements' policies to the shared policy base (policy_base_->load()),
// while every other application of the process copies that very base into its own
// policy manager through load_security_policies() -> init_from_base().
TEST_F(test_configuration_impl, concurrent_init_from_base_during_load_optional) {
    constexpr unsigned its_reader_count{7};
    constexpr unsigned its_reads_per_thread{500};

    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "client_app", "id": "0x2222" } ],
        "security": {
            "check_credentials": "true",
            "policies": [
                {
                    "credentials": { "uid": "1000", "gid": "1000" },
                    "allow": {
                        "requests": [
                            {
                                "service": "0x1234",
                                "instances": [ { "ids": [ "0x5678" ], "methods": [ "0x0001" ] } ]
                            }
                        ]
                    }
                }
            ]
        }
    })");
    // Not a mandatory file name: this policy reaches the base only through
    // load_optional(), i.e. while the readers below are already running.
    dir.write("extra_security.json", R"({
        "security": {
            "policies": [
                {
                    "credentials": { "uid": "2000", "gid": "2000" },
                    "allow": {
                        "requests": [
                            {
                                "service": "0x4321",
                                "instances": [ { "ids": [ "0x0001" ], "methods": [ "0x0001" ] } ]
                            }
                        ]
                    }
                }
            ]
        }
    })");

    auto its_config = std::make_shared<configuration_impl>(dir.path());
    its_config->set_configuration_path(dir.path());
    ASSERT_TRUE(its_config->load("client_app"));

    std::atomic<unsigned> its_mismatches{0};

    // Thread 0 is the elected host running the optional parse, the rest are
    // applications initializing their own policy manager from the shared base.
    run_concurrently(its_reader_count + 1, [&](unsigned _index) {
        if (_index == 0) {
            its_config->load_optional();
            return;
        }
        // One manager per thread, re-initialized every round: init_from_base()
        // clears and refills, so this is the same code path an application runs,
        // just repeatedly, to widen the window.
        policy_manager_impl its_pm;
        auto its_allowed = utility::create_uds_client(1000, 1000, 0);
        auto its_denied = utility::create_uds_client(3000, 3000, 0);
        for (unsigned i = 0; i < its_reads_per_thread; ++i) {
            its_config->load_security_policies(its_pm);
            // The mandatory policy is in the base before any thread starts, so
            // every copy must carry it - whatever the optional parse is doing.
            if (!its_pm.is_client_allowed(&its_allowed, 0x1234, 0x5678, 0x0001)) {
                ++its_mismatches;
            }
            // And no copy may invent permissions for unrelated credentials.
            if (its_pm.is_client_allowed(&its_denied, 0x1234, 0x5678, 0x0001)) {
                ++its_mismatches;
            }
        }
    });

    EXPECT_EQ(its_mismatches.load(), 0u);

    // The optional policy did land in the base, so the readers really did race
    // against policy_base_->load() rather than against a finished parse.
    policy_manager_impl its_pm_after;
    its_config->load_security_policies(its_pm_after);
    auto its_optional_client = utility::create_uds_client(2000, 2000, 0);
    EXPECT_TRUE(its_pm_after.is_client_allowed(&its_optional_client, 0x4321, 0x0001, 0x0001));
}

// A routing.host uid/gid pair alone (no explicit routing-credentials block)
// configures non-strict credentials: mismatches stay permissive, preserving
// pre-existing audit-mode semantics.
TEST_F(test_configuration_impl, routing_host_uid_gid_alone_is_not_strict) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager", "uid": "1000", "gid": "1000" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ]
    })");

    auto its_config = load(dir, "routingmanager");
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_matching = utility::create_uds_client(1000, 1000, 0);
    auto its_mismatching = utility::create_uds_client(2000, 2000, 0);

    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_matching));
    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_mismatching));
}

// An explicit routing-credentials block always wins over routing.host uid/gid
// and is enforced strictly: once security's check_credentials is "true"
// (audit mode off), a mismatch must be rejected - including the uid/gid that
// used to be valid via routing.host.
TEST_F(test_configuration_impl, explicit_routing_credentials_override_routing_host_and_are_enforced) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager", "uid": "1000", "gid": "1000" } },
        "routing-credentials": { "uid": "2000", "gid": "2000" },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" }
    })");

    auto its_config = load(dir, "routingmanager");
    ASSERT_FALSE(its_config->is_security_audit());
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_old_host_credentials = utility::create_uds_client(1000, 1000, 0);
    auto its_explicit_credentials = utility::create_uds_client(2000, 2000, 0);

    EXPECT_FALSE(its_config->check_routing_credentials(its_host_id, its_old_host_credentials));
    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_explicit_credentials));
}

// The explicit routing-credentials block must win over routing.host uid/gid
// even when the host is parsed *after* the block. The block lives in
// "vsomeip_app.json" (sorts first) and the host uid/gid in "vsomeip_std.json"
// (sorts later); load_routing_host must not overwrite the already-strict
// credentials, so the block's uid/gid stays authoritative and enforced.
TEST_F(test_configuration_impl, explicit_routing_credentials_win_even_when_host_parsed_later) {
    scratch_config_dir dir;
    dir.write("vsomeip_app.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "routing-credentials": { "uid": "2000", "gid": "2000" },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" }
    })");
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager", "uid": "1000", "gid": "1000" } }
    })");

    auto its_config = load(dir, "routingmanager");
    ASSERT_FALSE(its_config->is_security_audit());
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_host_credentials = utility::create_uds_client(1000, 1000, 0);
    auto its_explicit_credentials = utility::create_uds_client(2000, 2000, 0);

    EXPECT_FALSE(its_config->check_routing_credentials(its_host_id, its_host_credentials));
    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_explicit_credentials));
}

// An explicit routing-credentials block is enforced even under global audit
// mode (security.check_credentials == "false"). check_routing_credentials must
// mirror the runtime's second gate - authenticate_router ->
// policy_manager_impl::check_routing_credentials - which rejects a routing-host
// uid/gid mismatch regardless of audit mode. So a mismatch is rejected here too.
TEST_F(test_configuration_impl, explicit_routing_credentials_enforced_even_in_audit_mode) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "routing-credentials": { "uid": "2000", "gid": "2000" },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "false" }
    })");

    auto its_config = load(dir, "routingmanager");
    ASSERT_TRUE(its_config->is_security_audit());
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_mismatching = utility::create_uds_client(9999, 9999, 0);
    EXPECT_FALSE(its_config->check_routing_credentials(its_host_id, its_mismatching));
}

// A second explicit routing-credentials definition must be ignored - the
// first one that was parsed keeps applying. "vsomeip_app.json" sorts before
// "vsomeip_std.json" so it is guaranteed to be processed first.
TEST_F(test_configuration_impl, duplicate_explicit_routing_credentials_keeps_first_definition) {
    scratch_config_dir dir;
    dir.write("vsomeip_app.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "routing-credentials": { "uid": "1111", "gid": "1111" },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" }
    })");
    dir.write("vsomeip_std.json", R"({
        "routing-credentials": { "uid": "2222", "gid": "2222" }
    })");

    auto its_config = load(dir, "routingmanager");
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_first_definition = utility::create_uds_client(1111, 1111, 0);
    auto its_second_definition = utility::create_uds_client(2222, 2222, 0);

    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_first_definition));
    EXPECT_FALSE(its_config->check_routing_credentials(its_host_id, its_second_definition));
}

// check_routing_credentials only ever gates the routing host's own client id;
// every other client must be permitted regardless of the credentials on file.
TEST_F(test_configuration_impl, non_routing_host_client_is_always_permitted) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "routing-credentials": { "uid": "1000", "gid": "1000" },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" }
    })");

    auto its_config = load(dir, "routingmanager");
    vsomeip_v3::client_t its_other_client = 0xABCD;

    auto its_mismatching = utility::create_uds_client(9999, 9999, 0);
    EXPECT_TRUE(its_config->check_routing_credentials(its_other_client, its_mismatching));
}

// A TCP peer has no UDS credentials to compare against, so it always bypasses
// the check - even against strictly enforced, mismatching credentials.
TEST_F(test_configuration_impl, tcp_socket_bypasses_uds_credential_check) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "routing-credentials": { "uid": "1000", "gid": "1000" },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" }
    })");

    auto its_config = load(dir, "routingmanager");
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_tcp_client = make_tcp_client(9999, 9999);
    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_tcp_client));
}

// With no routing-credentials configured at all (neither via routing.host nor
// an explicit block), the check must stay permissive.
TEST_F(test_configuration_impl, no_configured_credentials_is_permissive) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ]
    })");

    auto its_config = load(dir, "routingmanager");
    auto its_host_id = its_config->get_id("routingmanager");

    auto its_any_client = utility::create_uds_client(4242, 4242, 0);
    EXPECT_TRUE(its_config->check_routing_credentials(its_host_id, its_any_client));
}

// load_security_policies() must let each application replay the same
// compiled policy state into its own, independent policy_manager_impl - this
// is what makes it safe to load the configuration only once per process and
// share it across applications.
TEST_F(test_configuration_impl, load_security_policies_populates_independent_policy_managers) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": {
            "check_credentials": "true",
            "policies": [
                {
                    "credentials": { "uid": "1000", "gid": "1000" },
                    "allow": { "offers": [ { "service": "0x1234", "instance": "0x5678" } ] }
                }
            ]
        }
    })");

    auto its_config = load(dir, "routingmanager");

    policy_manager_impl its_pm_app1;
    policy_manager_impl its_pm_app2;
    its_config->load_security_policies(its_pm_app1);
    its_config->load_security_policies(its_pm_app2);

    auto its_allowed = utility::create_uds_client(1000, 1000, 0);
    auto its_denied = utility::create_uds_client(2000, 2000, 0);

    EXPECT_TRUE(its_pm_app1.is_offer_allowed(&its_allowed, 0x1234, 0x5678));
    EXPECT_FALSE(its_pm_app1.is_offer_allowed(&its_denied, 0x1234, 0x5678));
    EXPECT_TRUE(its_pm_app2.is_offer_allowed(&its_allowed, 0x1234, 0x5678));
    EXPECT_FALSE(its_pm_app2.is_offer_allowed(&its_denied, 0x1234, 0x5678));
}

// init_from_base() (via load_security_policies) must copy only the compiled,
// immutable policy state - never the per-app runtime maps. This is the core
// safety property that makes it correct to load the configuration once and
// share it: each application must own an isolated client<->sec_client mapping
// table. A regression that shared those maps (e.g. a shallow copy of the same
// container) would let one app observe another app's client registrations.
TEST_F(test_configuration_impl, init_from_base_does_not_share_runtime_client_mappings) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" }
    })");

    auto its_config = load(dir, "routingmanager");

    policy_manager_impl its_pm_app1;
    policy_manager_impl its_pm_app2;
    its_config->load_security_policies(its_pm_app1);
    its_config->load_security_policies(its_pm_app2);

    // Register a client->sec_client mapping in app1 only.
    const vsomeip_v3::client_t its_client = 0x2222;
    auto its_sec_client = utility::create_uds_client(1000, 1000, 0);
    ASSERT_TRUE(its_pm_app1.store_client_to_sec_client_mapping(its_client, &its_sec_client));

    // app1 sees its own mapping ...
    vsomeip_sec_client_t its_readback{};
    EXPECT_TRUE(its_pm_app1.get_client_to_sec_client_mapping(its_client, its_readback));

    // ... but app2's runtime table must be untouched.
    vsomeip_sec_client_t its_unused{};
    EXPECT_FALSE(its_pm_app2.get_client_to_sec_client_mapping(its_client, its_unused));
}

// In audit mode (check_credentials == "false") the compiled policies are still
// loaded and policy enforcement is enabled, but every decision falls back to
// "permit". This must survive init_from_base(): a per-app policy manager must
// stay permissive for credentials that would be rejected under strict mode.
TEST_F(test_configuration_impl, audit_mode_policy_is_permissive_after_init_from_base) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": {
            "check_credentials": "false",
            "policies": [
                {
                    "credentials": { "uid": "1000", "gid": "1000" },
                    "allow": { "offers": [ { "service": "0x1234", "instance": "0x5678" } ] }
                }
            ]
        }
    })");

    auto its_config = load(dir, "routingmanager");
    ASSERT_TRUE(its_config->is_security_audit());

    policy_manager_impl its_pm;
    its_config->load_security_policies(its_pm);
    ASSERT_TRUE(its_pm.is_enabled());
    ASSERT_TRUE(its_pm.is_audit());

    auto its_allowed = utility::create_uds_client(1000, 1000, 0);
    auto its_mismatching = utility::create_uds_client(2000, 2000, 0);

    // The matching offer is allowed, and - crucially - the mismatching one is
    // also allowed because audit mode never blocks.
    EXPECT_TRUE(its_pm.is_offer_allowed(&its_allowed, 0x1234, 0x5678));
    EXPECT_TRUE(its_pm.is_offer_allowed(&its_mismatching, 0x1234, 0x5678));
}

// is_client_allowed() (the request path, including method granularity) must be
// driven correctly by the policies replayed through init_from_base(), not just
// is_offer_allowed().
TEST_F(test_configuration_impl, request_policies_are_enforced_after_init_from_base) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": {
            "check_credentials": "true",
            "policies": [
                {
                    "credentials": { "uid": "1000", "gid": "1000" },
                    "allow": {
                        "requests": [
                            {
                                "service": "0x1234",
                                "instances": [ { "ids": [ "0x5678" ], "methods": [ "0x0001" ] } ]
                            }
                        ]
                    }
                }
            ]
        }
    })");

    auto its_config = load(dir, "routingmanager");

    policy_manager_impl its_pm;
    its_config->load_security_policies(its_pm);

    auto its_allowed = utility::create_uds_client(1000, 1000, 0);
    auto its_denied = utility::create_uds_client(2000, 2000, 0);

    // Allowed uid/gid on the exact service/instance/method.
    EXPECT_TRUE(its_pm.is_client_allowed(&its_allowed, 0x1234, 0x5678, 0x0001));
    // Right credentials, wrong method -> denied.
    EXPECT_FALSE(its_pm.is_client_allowed(&its_allowed, 0x1234, 0x5678, 0x0002));
    // Wrong credentials -> denied.
    EXPECT_FALSE(its_pm.is_client_allowed(&its_denied, 0x1234, 0x5678, 0x0001));
}

// A "deny" policy must also survive init_from_base(): the denied
// service/instance is blocked while everything else stays permitted.
TEST_F(test_configuration_impl, deny_policy_is_enforced_after_init_from_base) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": {
            "check_credentials": "true",
            "policies": [
                {
                    "credentials": { "uid": "1000", "gid": "1000" },
                    "deny": { "requests": [ { "service": "0x1234", "instance": "0x5678" } ] }
                }
            ]
        }
    })");

    auto its_config = load(dir, "routingmanager");

    policy_manager_impl its_pm;
    its_config->load_security_policies(its_pm);

    auto its_client = utility::create_uds_client(1000, 1000, 0);

    // The explicitly denied service/instance is blocked ...
    EXPECT_FALSE(its_pm.is_client_allowed(&its_client, 0x1234, 0x5678, 0x0001));
    // ... but a different service is not covered by the deny rule -> permitted.
    EXPECT_TRUE(its_pm.is_client_allowed(&its_client, 0x9999, 0x0001, 0x0001));
}

// When security is handled externally (an empty "security" block), load() must
// NOT compile any policies into the shared base. A per-app policy manager then
// stays disabled and permissive - the external component owns enforcement.
TEST_F(test_configuration_impl, external_security_loads_no_policies) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": {}
    })");

    auto its_config = load(dir, "routingmanager");
    ASSERT_TRUE(its_config->is_security_external());

    policy_manager_impl its_pm;
    its_config->load_security_policies(its_pm);

    EXPECT_FALSE(its_pm.is_enabled());

    // With no policy enabled every decision is permissive.
    auto its_client = utility::create_uds_client(4242, 4242, 0);
    EXPECT_TRUE(its_pm.is_offer_allowed(&its_client, 0x1234, 0x5678));
    EXPECT_TRUE(its_pm.is_client_allowed(&its_client, 0x1234, 0x5678, 0x0001));
}

// init_from_base() must also copy the policy-extension path table so that
// lazy_load_security() can resolve per-host extension folders on any per-app
// policy manager. Both replayed managers must resolve the same host path.
TEST_F(test_configuration_impl, policy_extension_paths_copied_by_init_from_base) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": { "check_credentials": "true" },
        "container_policy_extensions": [
            { "container": "my_host", "path": "/extensions/my_host" }
        ]
    })");

    auto its_config = load(dir, "routingmanager");

    policy_manager_impl its_pm_app1;
    policy_manager_impl its_pm_app2;
    its_config->load_security_policies(its_pm_app1);
    its_config->load_security_policies(its_pm_app2);

    const auto its_path_app1 = its_pm_app1.get_policy_extension_path("my_host");
    const auto its_path_app2 = its_pm_app2.get_policy_extension_path("my_host");

    EXPECT_FALSE(its_path_app1.empty());
    EXPECT_EQ(its_path_app1, its_path_app2);
    // An unknown host resolves to no path in the copied table.
    EXPECT_TRUE(its_pm_app1.get_policy_extension_path("unknown_host").empty());
}

// A runtime policy update (UPDATE_SECURITY_POLICY) is applied to a single
// application's policy manager and must stay there.
TEST_F(test_configuration_impl, runtime_policy_update_does_not_leak_across_apps) {
    scratch_config_dir dir;
    dir.write("vsomeip_std.json", R"({
        "routing": { "host": { "name": "routingmanager" } },
        "applications": [ { "name": "routingmanager", "id": "0x1111" } ],
        "security": {
            "check_credentials": "true",
            "policies": [
                {
                    "credentials": { "uid": "1000", "gid": "1000" },
                    "allow": {
                        "requests": [
                            {
                                "service": "0x1234",
                                "instances": [ { "ids": [ "0x5678" ], "methods": [ "0x0001" ] } ]
                            }
                        ]
                    }
                }
            ]
        }
    })");

    auto its_config = load(dir, "routingmanager");

    policy_manager_impl its_pm_app1;
    policy_manager_impl its_pm_app2;
    its_config->load_security_policies(its_pm_app1);
    its_config->load_security_policies(its_pm_app2);

    auto its_client = utility::create_uds_client(1000, 1000, 0);

    // The to-be-granted request is not allowed by the compiled policies.
    ASSERT_FALSE(its_pm_app1.is_client_allowed(&its_client, 0x4321, 0x0001, 0x0001));
    ASSERT_FALSE(its_pm_app2.is_client_allowed(&its_client, 0x4321, 0x0001, 0x0001));

    // Build an additive update granting service 0x4321. It deliberately
    // carries no credentials: it is only effective when merged into the
    // matching compiled uid/gid 1000/1000 allow policy, so the ASSERT_TRUE
    // below fails loudly if the merge path is ever not taken.
    auto its_update = std::make_shared<vsomeip_v3::policy>();
    its_update->allow_who_ = true;
    its_update->allow_what_ = true;
    boost::icl::interval_set<vsomeip_v3::method_t> its_methods;
    its_methods.insert(boost::icl::discrete_interval<vsomeip_v3::method_t>(0x0001, 0x0001, boost::icl::interval_bounds::closed()));
    boost::icl::interval_map<vsomeip_v3::instance_t, boost::icl::interval_set<vsomeip_v3::method_t>> its_instances;
    its_instances += std::make_pair(
            boost::icl::discrete_interval<vsomeip_v3::instance_t>(0x0001, 0x0001, boost::icl::interval_bounds::closed()), its_methods);
    its_update->requests_ += std::make_pair(
            boost::icl::discrete_interval<vsomeip_v3::service_t>(0x4321, 0x4321, boost::icl::interval_bounds::closed()), its_instances);

    its_pm_app1.update_security_policy(1000, 1000, its_update);

    // app1 accepted the update and now grants the request ...
    ASSERT_TRUE(its_pm_app1.is_client_allowed(&its_client, 0x4321, 0x0001, 0x0001));
    // ... but app2 never received it and must not.
    EXPECT_FALSE(its_pm_app2.is_client_allowed(&its_client, 0x4321, 0x0001, 0x0001));

    // An application starting after the update must get the pristine compiled
    // state, not one mutated through a shared policy object.
    policy_manager_impl its_pm_app3;
    its_config->load_security_policies(its_pm_app3);
    EXPECT_FALSE(its_pm_app3.is_client_allowed(&its_client, 0x4321, 0x0001, 0x0001));
}
