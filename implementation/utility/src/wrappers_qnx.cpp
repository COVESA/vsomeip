// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef __QNX__

#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * QNX 8 (io-sock) provides a native accept4() with SOCK_CLOEXEC / SOCK_NONBLOCK,
 * so the QNX 7 (io-pkt) message-passing accept implementation that used
 * <sys/sockmsg.h> / _io_openfd is no longer needed (and that header was dropped).
 * All that remains are the CLOEXEC-adding linker --wrap targets referenced by the
 * QNX LINK_FLAGS in CMakeLists.txt (-Wl,-wrap,socket / accept / open).
 */
extern "C" {

int __real_socket(int domain, int type, int protocol) noexcept;

// Override socket(2) to set SOCK_CLOEXEC by default.
int __wrap_socket(int domain, int type, int protocol) noexcept {
    return __real_socket(domain, type | SOCK_CLOEXEC, protocol);
}

// Override accept(2) to set SOCK_CLOEXEC by default (native accept4 on QNX 8).
int __wrap_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);
}

int __real_open(const char* pathname, int flags, mode_t mode);

// Override open(2) to set O_CLOEXEC by default.
int __wrap_open(const char* pathname, int flags, mode_t mode) {
    return __real_open(pathname, flags | O_CLOEXEC, mode);
}

}

#endif // __QNX__
