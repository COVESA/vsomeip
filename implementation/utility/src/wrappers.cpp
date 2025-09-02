// Copyright (C) 2020-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef __linux__

#include <cerrno>
#include <execinfo.h> // for backtrace()
#include <fcntl.h>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

#include <vsomeip/internal/logger.hpp> // for VSOMEIP_FATAL

void react(int fd, const char* func);

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
        react(fd, "close");
    }

    return ret;
}

/*
 * The real recvfrom(2), renamed by GCC.
 */
int __real_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen);

/*
 * Overrides recvfrom(2) to react on EBADF
 */
ssize_t __wrap_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
    int ret = __real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "recvfrom");
    }

    return ret;
}

/*
 * The real recvmsg(2), renamed by GCC.
 */
int __real_recvmsg(int sockfd, struct msghdr* msg, int flags);

/*
 * Overrides recvmsg(2) to react on EBADF
 */
ssize_t __wrap_recvmsg(int sockfd, struct msghdr* msg, int flags) {
    int ret = __real_recvmsg(sockfd, msg, flags);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "recvmsg");
    }

    return ret;
}

/*
 * The real sendto(2), renamed by GCC.
 */
int __real_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);

/*
 * Overrides sendto(2) to react on EBADF
 */
ssize_t __wrap_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, socklen_t addrlen) {
    int ret = __real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "sendto");
    }

    return ret;
}

/*
 * The real sendmsg(2), renamed by GCC.
 */
int __real_sendmsg(int sockfd, const struct msghdr* msg, int flags);

/*
 * Overrides sendmsg(2) to react on EBADF
 */
ssize_t __wrap_sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    int ret = __real_sendmsg(sockfd, msg, flags);
    if (ret == -1 && errno == EBADF) {
        react(sockfd, "sendmsg");
    }

    return ret;
}
}

void react(int fd, const char* func) {
    // if this happens, there is a serious logical issue somewhere:
    // 1) application accidentally close'd a descriptor that belongs to libvsomeip
    // 2) libvsomeip itself double close'd a descriptor

    // stderr
    fprintf(stderr, "[libvsomeip] %s(%d) failed with errno EBADF, descriptor leak!\n", func, fd);
    fflush(stderr);
    // libdlt (or well, stdout/stderr in case of no libdlt..)
    VSOMEIP_FATAL << func << "(" << fd << ") failed with errno EBADF, descriptor leak!";

    // Backtrace is not supported on android NDK, to be checked in the future
#if !defined(ANDROID_CI_BUILD)
    void* callstack[128];
    int nframes = backtrace(callstack, 128);
    char** symbols = backtrace_symbols(callstack, nframes);
    for (int i = 0; i < nframes; ++i) {
        // stderr
        fprintf(stderr, "[libvsomeip] %s\n", symbols[i]);
        fflush(stderr);
        // libdlt
        VSOMEIP_FATAL << symbols[i];
    }

    // see backtrace(3), caller needs to free memory after use, `backtrace` will malloc it
    free(symbols);
#endif

    if (getenv(VSOMEIP_ENV_ABORT_ON_CRIT_SYSCALL_ERROR)) {
        abort();
    }
}

#endif
