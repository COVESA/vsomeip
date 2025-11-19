// Copyright (C) 2020-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef __linux__

#include <boost/stacktrace.hpp>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

#include <vsomeip/internal/logger.hpp> // for VSOMEIP_FATAL

void react(int fd, const char* func, int _errno);

/*
 * These definitions MUST remain in the global namespace.
 */
extern "C" {
/*
 * The real socket(2), renamed by GCC.
 */
int __real_socket(int domain, int type, int protocol) noexcept;

/*
 * Overrides socket(2) to set SOCK_CLOEXEC by default.
 */
int __wrap_socket(int domain, int type, int protocol) noexcept {
    return __real_socket(domain, type | SOCK_CLOEXEC, protocol);
}

/*
 * Overrides accept(2) to set SOCK_CLOEXEC by default.
 */
int __wrap_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    return accept4(sockfd, addr, addrlen, SOCK_CLOEXEC);
}

/*
 * The real open(2), renamed by GCC.
 */
int __real_open(const char* pathname, int flags, mode_t mode);

/*
 * Overrides open(2) to set O_CLOEXEC by default.
 */
int __wrap_open(const char* pathname, int flags, mode_t mode) {
    return __real_open(pathname, flags | O_CLOEXEC, mode);
}

/*
 * The real close(2), renamed by GCC.
 */
int __real_close(int fd);

/*
 * Overrides close(2) to react on EBADF
 */
int __wrap_close(int fd) {
    int ret = __real_close(fd);
    if (ret == -1 && errno == EBADF) {
        react(fd, "close", errno);
    }

    return ret;
}

/*
 * The real recvfrom(2), renamed by GCC.
 */
ssize_t __real_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);

/*
 * Overrides recvfrom(2) to react on EBADF
 */
ssize_t __wrap_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
    ssize_t ret = __real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "recvfrom", errno);
    }

    return ret;
}

/*
 * The real recvmsg(2), renamed by GCC.
 */
ssize_t __real_recvmsg(int sockfd, struct msghdr* msg, int flags);

/*
 * Overrides recvmsg(2) to react on EBADF
 */
ssize_t __wrap_recvmsg(int sockfd, struct msghdr* msg, int flags) {
    ssize_t ret = __real_recvmsg(sockfd, msg, flags);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "recvmsg", errno);
    }

    return ret;
}

/*
 * The real sendto(2), renamed by GCC.
 */
ssize_t __real_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);

/*
 * Overrides sendto(2) to react on EBADF
 */
ssize_t __wrap_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen) {
    ssize_t ret = __real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "sendto", errno);
    }

    return ret;
}

/*
 * The real sendmsg(2), renamed by GCC.
 */
ssize_t __real_sendmsg(int sockfd, const struct msghdr* msg, int flags);

/*
 * Overrides sendmsg(2) to react on EBADF
 */
ssize_t __wrap_sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    ssize_t ret = __real_sendmsg(sockfd, msg, flags);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "sendmsg", errno);
    }

    return ret;
}

/*
 * The real epoll_wait(2), renamed by GCC.
 */
int __real_epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);

/*
 * Overrides epoll_wait(2) to react on EBADF and EINVAL
 */
int __wrap_epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
    int ret = __real_epoll_wait(epfd, events, maxevents, timeout);
    if (ret == -1 && (errno == EBADF || errno == EINVAL)) {
        react(epfd, "epoll_wait", errno);
    }

    return ret;
}

/*
 * The real epoll_pwait(2), renamed by GCC.
 */
int __real_epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask);

/*
 * Overrides epoll_pwait(2) to react on EBADF and EINVAL
 */
int __wrap_epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask) {
    int ret = __real_epoll_pwait(epfd, events, maxevents, timeout, sigmask);
    if (ret == -1 && (errno == EBADF || errno == EINVAL)) {
        react(epfd, "epoll_pwait", errno);
    }

    return ret;
}

} /* extern "C" */

void react(int fd, const char* func, int _errno) {
    // if this happens, there is a serious logical issue somewhere:
    // 1) application accidentally close'd a descriptor that belongs to libvsomeip
    // 2) libvsomeip itself double close'd a descriptor

    const int e = (_errno != 0) ? _errno : errno;

    char errno_buf[32];
    const char* errno_str;
    // EBADF and EINVAL are handled differently in order to keep compatibility.
    // Previous versions had a "EBADF" and `strerrorname_np` is GNU/Linux only.
    if (errno == EBADF) {
        errno_str = "EBADF";
    } else if (errno == EINVAL) {
        errno_str = "EINVAL";
    } else {
        snprintf(errno_buf, sizeof(errno_buf), "%d", e);
        errno_str = errno_buf;
    }

    std::string msg = std::string(func) + "(" + std::to_string(fd) + ") failed with errno " + std::string(errno_str) + ", descriptor leak!";

    VSOMEIP_FATAL << msg;
    std::cerr << "[libvsomeip] " << msg << "\n";

    const auto st = boost::stacktrace::stacktrace();
    for (const auto& frame : st) {
        VSOMEIP_FATAL << frame;
        std::cerr << frame;
    }

    VSOMEIP_TERMINATE(msg.c_str());
}

#endif
