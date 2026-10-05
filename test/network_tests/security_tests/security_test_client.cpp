// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "security_client.hpp"

#include "common/test_main.hpp"

static bool is_remote_test = false;
static bool remote_client_allowed = true;
static bool is_offer_test = false;
static bool offer_allowed = false;

TEST(someip_security_test, basic_subscribe_request_response) {
    security_client test_client(is_remote_test, remote_client_allowed, is_offer_test, offer_allowed);
    if (test_client.init()) {
        test_client.start();
        test_client.join_sender_thread();
    }
}

int main(int argc, char** argv) {
    std::string test_remote("--remote");
    std::string test_local("--local");
    std::string test_allow_remote_client("--allow");
    std::string test_deny_remote_client("--deny");
    std::string test_offer_allow("--offer-allow");
    std::string test_offer_deny("--offer-deny");
    std::string help("--help");

    int i = 1;
    while (i < argc) {
        if (test_remote == argv[i]) {
            is_remote_test = true;
        } else if (test_local == argv[i]) {
            is_remote_test = false;
        } else if (test_allow_remote_client == argv[i]) {
            remote_client_allowed = true;
        } else if (test_deny_remote_client == argv[i]) {
            remote_client_allowed = false;
        } else if (test_offer_allow == argv[i]) {
            is_offer_test = true;
            offer_allowed = true;
        } else if (test_offer_deny == argv[i]) {
            is_offer_test = true;
            offer_allowed = false;
        } else if (help == argv[i]) {
            VSOMEIP_INFO << "Parameters:\n"
                         << "--remote: Run test between two hosts\n"
                         << "--local: Run test locally\n"
                         << "--allow: test is started with a policy that allows remote messages "
                            "sent by this test client to the service\n"
                         << "--deny: test is started with a policy that denies remote messages "
                            "sent by this test client to the service\n"
                         << "--offer-allow: consumer policy authorizes the provider's offer "
                            "-> responses/notifications are received\n"
                         << "--offer-deny: consumer policy omits the offer authorization "
                            "-> responses/notifications are dropped\n"
                         << "--help: print this help";
        }
        i++;
    }

    return test_main(argc, argv);
}
