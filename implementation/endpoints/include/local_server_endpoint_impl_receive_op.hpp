// Copyright (C) 2020-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_SERVER_ENDPOINT_IMPL_RECEIVE_OP_HPP_
#define VSOMEIP_V3_LOCAL_SERVER_ENDPOINT_IMPL_RECEIVE_OP_HPP_

#if VSOMEIP_BOOST_VERSION >= 106600
#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)

#include <boost/asio/local/stream_protocol.hpp>

namespace vsomeip_v3 {

typedef boost::asio::local::stream_protocol::socket socket_type_t;
typedef std::function<
    void (boost::system::error_code const &_error, size_t _size,
          const uint32_t &, const uint32_t &)> receive_handler_t;

struct local_server_endpoint_impl_receive_op {

    socket_type_t &socket_;
    receive_handler_t handler_;
    byte_t *buffer_;
    size_t length_;
    uid_t uid_;
    gid_t gid_;
    size_t bytes_;

    void operator()(boost::system::error_code _error) {

        if (!_error) {
            if (!socket_.native_non_blocking())
                socket_.native_non_blocking(true, _error);

            for (;;) {
                ssize_t its_result;
                int its_flags(0);

                // Set buffer
                struct iovec its_vec[1];
                its_vec[0].iov_base = buffer_;
                its_vec[0].iov_len = length_;

                union {
                    struct cmsghdr cmh;
                    char   control[CMSG_SPACE(sizeof(struct ucred))];
                } control_un;

                // Set 'control_un' to describe ancillary data that we want to receive
                control_un.cmh.cmsg_len = CMSG_LEN(sizeof(struct ucred));
                control_un.cmh.cmsg_level = SOL_SOCKET;
                control_un.cmh.cmsg_type = SCM_CREDENTIALS;

                // Build header with all informations to call ::recvmsg
                msghdr its_header = msghdr();
                its_header.msg_iov = its_vec;
                its_header.msg_iovlen = 1;
                its_header.msg_control = control_un.control;
                its_header.msg_controllen = sizeof(control_un.control);

                // Call recvmsg and handle its result
                errno = 0;
                its_result = ::recvmsg(socket_.native_handle(), &its_header, its_flags);
                _error = boost::system::error_code(its_result < 0 ? errno : 0,
                        boost::asio::error::get_system_category());
                bytes_ += _error ? 0 : static_cast<size_t>(its_result);

                if (_error == boost::asio::error::interrupted)
                    continue;

                if (_error == boost::asio::error::would_block
                        || _error == boost::asio::error::try_again) {
                    socket_.async_wait(socket_type_t::wait_read, *this);
                    return;
                }

                if (_error)
                    break;

                if (bytes_ == 0)
                    _error = boost::asio::error::eof;

                // Extract credentials (UID/GID)
                struct ucred *its_credentials;
                for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&its_header);
                     cmsg != NULL;
                     cmsg = CMSG_NXTHDR(&its_header, cmsg))
                {
                    if (cmsg->cmsg_level == SOL_SOCKET
                        && cmsg->cmsg_type == SCM_CREDENTIALS
                        && cmsg->cmsg_len == CMSG_LEN(sizeof(struct ucred))) {

                        its_credentials = (struct ucred *) CMSG_DATA(cmsg);
                        if (its_credentials) {
                            uid_ = its_credentials->uid;
                            gid_ = its_credentials->gid;
                            break;
                        }
                    }
                }

                break;
            }
        }

        // Call the handler
        handler_(_error, bytes_, uid_, gid_);
    }
};

} // namespace vsomeip

#endif // __linux__ || ANDROID
#endif // VSOMEIP_BOOST_VERSION >= 106600

#endif // VSOMEIP_V3_LOCAL_SERVER_ENDPOINT_IMPL_RECEIVE_OP_HPP_
