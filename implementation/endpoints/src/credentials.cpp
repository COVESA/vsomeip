// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WIN32

#include <sys/socket.h>

#include "../include/credentials.hpp"

#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"

namespace vsomeip {

void credentials::activate_credentials(const int _fd) {
    int optval = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) == -1) {
        VSOMEIP_ERROR << "Activating socket option for receiving credentials failed.";
    }
}

void credentials::deactivate_credentials(const int _fd) {
    int optval = 0;
    if (setsockopt(_fd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) == -1) {
        VSOMEIP_ERROR << "Deactivating socket option for receiving credentials failed.";
    }
}

client_t credentials::receive_credentials(const int _fd, uid_t& _uid, gid_t& _gid) {
    struct ucred *ucredp;
    struct msghdr msgh;
    struct iovec iov;
    union {
        struct cmsghdr cmh;
        char   control[CMSG_SPACE(sizeof(struct ucred))];
    } control_un;
    struct cmsghdr *cmhp;
    // Sender client_id will be received as data
    client_t client = VSOMEIP_ROUTING_CLIENT;

    // Set 'control_un' to describe ancillary data that we want to receive
    control_un.cmh.cmsg_len = CMSG_LEN(sizeof(struct ucred));
    control_un.cmh.cmsg_level = SOL_SOCKET;
    control_un.cmh.cmsg_type = SCM_CREDENTIALS;

    // Set 'msgh' fields to describe 'control_un'
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    // Set fields of 'msgh' to point to buffer used to receive (real) data read by recvmsg()
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &client;
    iov.iov_len = sizeof(client_t);

    // We don't need address of peer as we using connect
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;

    // Receive client_id plus ancillary data
    ssize_t nr = recvmsg(_fd, &msgh, 0);
    if (nr == -1) {
        VSOMEIP_ERROR << "Receiving credentials failed. No data.";
    }

    cmhp = CMSG_FIRSTHDR(&msgh);
    if (cmhp == NULL || cmhp->cmsg_len != CMSG_LEN(sizeof(struct ucred))
            || cmhp->cmsg_level != SOL_SOCKET || cmhp->cmsg_type != SCM_CREDENTIALS) {
        VSOMEIP_ERROR << "Receiving credentials failed. Invalid data.";
    } else {
        ucredp = (struct ucred *) CMSG_DATA(cmhp);
        _uid = ucredp->uid;
        _gid = ucredp->gid;
    }

    return client;
}

void credentials::send_credentials(const int _fd, client_t _client) {
    struct msghdr msgh;
    struct iovec iov;

    // data to send
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &_client;
    iov.iov_len = sizeof(client_t);

    // destination not needed as we use connect
    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;

    // credentials not need to set explicitly
    msgh.msg_control = NULL;
    msgh.msg_controllen = 0;

    // send client id with credentials
    ssize_t ns = sendmsg(_fd, &msgh, 0);
    if (ns == -1) {
        VSOMEIP_ERROR << "Sending credentials failed.";
    }
}

} // namespace vsomeip

#endif

