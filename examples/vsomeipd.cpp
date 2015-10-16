// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Example(!) for the most simple daemon application

#include <vsomeip/vsomeip.hpp>

int main(int argc, char **argv) {
    std::shared_ptr<vsomeip::application> its_daemon
        = vsomeip::runtime::get()->create_application("vsomeipd");

    if (its_daemon->init())
        its_daemon->start();

    return 0;
}
